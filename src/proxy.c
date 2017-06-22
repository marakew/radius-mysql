/*
 * proxy.c	Proxy stuff.
 *
 *		Copyright 1999-2001	Cistron Internet Services B.V.
 *		Copyright 2002		Cistron IP B.V.
 */

char proxy_rcsid[] =
"$Id: proxy.c,v 1.23 2002/02/06 15:23:16 miquels Exp $";

#include	<sys/types.h>
#include	<sys/socket.h>
#include	<sys/time.h>
#include	<netinet/in.h>

#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<netdb.h>
#include	<pwd.h>
#include	<time.h>
#include	<ctype.h>

#include	"radiusd.h"

/*
 *	Make sure proxy_buffer is aligned properly.
 */
static		int proxy_buffer[1024];

int		proxy_id = 1;
AUTH_REQ	*proxy_requests;

static int allowed [] = {
	PW_SERVICE_TYPE,
	PW_FRAMED_PROTOCOL,
	PW_FILTER_ID,
	PW_FRAMED_MTU,
	PW_FRAMED_COMPRESSION,
	PW_LOGIN_SERVICE,
	PW_REPLY_MESSAGE,
	PW_SESSION_TIMEOUT,
	PW_IDLE_TIMEOUT,
	PW_PORT_LIMIT,
	0,
};


/*
 *	Cleanup old outstanding requests.
 */
static void proxy_cleanup(void)
{
	AUTH_REQ 		*a, *last, *next;
	time_t			now;

	last = NULL;
	now  = time(NULL);

	for (a = proxy_requests; a; a = next) {
		next = a->next;
		if (a->timestamp + MAX_REQUEST_TIME < now) {
			if (last)
				last->next = a->next;
			else
				proxy_requests = a->next;

			if (a->data) free(a->data);
			authfree(a);

			continue;
		}
		last = a;
	}
}

/*
 *	Add a proxy-pair to the end of the request.
 */
static VALUE_PAIR *proxy_addinfo(AUTH_REQ *authreq)
{
	VALUE_PAIR		*proxy_pair, *vp;

	if  (!(proxy_pair = (VALUE_PAIR *)malloc(sizeof(VALUE_PAIR)))) {
		log(L_ERR|L_CONS, "no memory");
		exit(1);
	}
	memset(proxy_pair, 0, sizeof(VALUE_PAIR));

	strcpy(proxy_pair->name, "Proxy-State");
	proxy_pair->attribute = PW_PROXY_STATE;
	proxy_pair->type = PW_TYPE_STRING;
	sprintf(proxy_pair->strvalue, "%04x", (unsigned)authreq->server_id);
	proxy_pair->length = 4;

	for (vp = authreq->request; vp && vp->next; vp = vp->next)
		;
	vp->next = proxy_pair;
	return vp;
}

/*
 *	Add the authreq to the list.
 */
int proxy_addrequest(AUTH_REQ *authreq, int *proxy_id)
{
	char		*m;
	AUTH_REQ	*a, *last = NULL;
	int		id = -1;

	/*
	 *	See if we already have a similar outstanding request.
	 */
	for (a = proxy_requests; a; a = a->next) {
		if (a->ipaddr == authreq->ipaddr &&
		    a->id == authreq->id &&
		    memcmp(a->vector, authreq->vector, sizeof(a->vector)) == 0)
			break;
		last = a;
	}
	if (a) {
		/*
		 *	Yes, this is a retransmit so delete the
		 *	old request.
		 */
		id = a->server_id;
		if (last)
			last->next = a->next;
		else
			proxy_requests = a->next;
		if (a->data) free(a->data);
		authfree(a);
	}
	if (id < 0) {
		id = (*proxy_id)++;
		*proxy_id &= 0xFFFF;
	}

	authreq->next = NULL;
	authreq->child_pid = -1;
	authreq->timestamp = time(NULL);

	/*
	 *	Copy the static data into malloc()ed memory.
	 */
	if ((m = malloc(authreq->data_len)) == NULL) {
		log(L_ERR|L_CONS, "no memory");
		exit(1);
	}
	memcpy(m, authreq->data, authreq->data_len);
	authreq->data = m;

	authreq->next = proxy_requests;
	proxy_requests = authreq;

	return id;
}

/*
 *	Decode a password and encode it again.
 *
 *	FIXME: Ascend gear sends short passwords, they should be
 *	AUTH_PASS_LEN. We should fix this up, instead we send a
 *	short password to next radius server too. Well, as long as it works..
 */
static void passwd_recode(char *secret_key, char *vector,
		char *pw_digest, char *pass, int passlen)
{
	char	decoded[AUTH_PASS_LEN + 1];
	char	encoded[AUTH_PASS_LEN];

	/*
	 *	Decode - encode.
	 */
	rad_pwdecode(pass, decoded, passlen, pw_digest);
	rad_pwencode(decoded, encoded, secret_key, vector);

	/*
	 *	Copy newly encoded password back to
	 *	where we got it from.
	 */
	memcpy(pass, encoded, passlen);
}

static void striprealm(VALUE_PAIR *vp)
{
	char	*s;

	if ((s = strrchr(vp->strvalue, '@')) != NULL) {
		*s = 0;
		vp->length = strlen(vp->strvalue);
	}
}

/*
 *	Relay the request to a remote server.
 *	Returns:  1 success (we reply, caller returns without replying)
 *	          0 fail (caller falls through to normal processing)
 *		 -1 fail (we don't reply, caller returns without replying)
 */
int proxy_send(AUTH_REQ *authreq, int activefd)
{
	VALUE_PAIR		*namepair;
	VALUE_PAIR		*called_id_pair;
	VALUE_PAIR		*vp_passwd;
	VALUE_PAIR		*vp, *pp;
	AUTH_HDR		*auth;
	REALM			*realm;
	RADCLIENT		*client;
	struct sockaddr_in	saremote, *sin;
	char			*secret_key;
	char			hn[16];
	char			vector[AUTH_VECTOR_LEN];
	char			pw_digest[16];
	char			saved_username[AUTH_STRING_LEN];
	char			saved_passwd[AUTH_STRING_LEN];
	char			called_id[AUTH_STRING_LEN+1];
	char			*realmname;
	char			*ptr, *r;
	char			*what = "unknown";
	unsigned short		rport;
	int			code, total_length;
	int			do_striprealm;

	/*
	 *	First cleanup old outstanding requests.
	 */
	proxy_cleanup();
	realmname = NULL;
	realm = NULL;
	do_striprealm = 0;

	/*
	 *	Look up name.
	 */
	namepair = pairfind(authreq->request, PW_USER_NAME);
	if (namepair == NULL)
		return 0;
	/* Same size */
	strcpy(saved_username, namepair->strvalue);

	/*
	 *	Prefix the called-station-id with a '+' and look
	 *	it up in the realms file.
	 */
	called_id_pair = pairfind(authreq->request, PW_CALLED_STATION_ID);
	if (called_id_pair != NULL && called_id_pair->strvalue[0]) {
		called_id[0] = '+';
		/* Same size */
		strcpy(called_id + 1, called_id_pair->strvalue);
		if ((realm = realm_find(called_id, 0)) != NULL)
			realmname = called_id;
	}

	if (realm == NULL) {
		/*
		 *	Find the realm from the _end_ so that we can
		 *	cascade realms: user@realm1@realm2.
		 *	A NULL realm is OK, but an empty realm isn't.
		 *	If not found, we treat it as usual).
		 */
		ptr = authreq->username[0] ?
			(char *)authreq->username : namepair->strvalue;
		if ((r = strrchr(ptr, '@')) != NULL)
			r++;

		/*
		 *	Empty realms, realms starting with '+',
		 *	and explicit NULL and DEFAULT not allowed.
		 */
		if (r && (r[0] == 0 || r[0] == '+' ||
			!strcmp(r, "NULL") || !strcmp(r, "DEFAULT")))
			return 0;
		if (r == NULL) r = "NULL";
		if ((realm = realm_find(r, 1)) == NULL)
			return 0;
		realmname = r;
		do_striprealm = 1;
	}

	/*
	 *	Perhaps we're getting this request via another
	 *	radius server that has already proxied the request.
	 *	Check all Cistron-Proxied-To attributes.
	 */
	for (vp = authreq->request; vp; vp = vp->next) {
		if (vp->attribute == PW_CISTRON_PROXIED_TO &&
		    vp->lvalue == realm->ipaddr)
			return 0;
	}

	/*
	 *	Remember that we sent the request to a Realm.
	 */
	if ((vp = (VALUE_PAIR *)malloc(sizeof(VALUE_PAIR))) == NULL) {
		log(L_ERR|L_CONS, "no memory");
		exit(1);
	}
	memset(vp, 0, sizeof(VALUE_PAIR));
	strcpy(vp->name, "Realm");
	vp->attribute = PW_REALM;
	vp->type = PW_TYPE_STRING;
	vp->length = strlen(realm->realm);
	strNcpy(vp->strvalue, realm->realm, sizeof(vp->strvalue));
	pairadd((&authreq->request), vp);

	/*
	 *	If "hints" was not set, we have to use the original
	 *	username as it was before hints processing.
	 */
	if (!realm->opts.dohints && authreq->username[0]) {
		strNcpy(namepair->strvalue, authreq->username,
			sizeof(namepair->strvalue));
		namepair->length = strlen(namepair->strvalue);
	}

	/*
	 *	Should we treat this as a local request ?
	 */
	code = authreq->code;
	if (strcmp(realm->server, "LOCAL") == 0 ||
	    (realm->opts.noauth && code == PW_AUTHENTICATION_REQUEST) ||
	    (realm->opts.noacct && code == PW_ACCOUNTING_REQUEST)) {
		if (!realm->opts.nostrip)
			striprealm(namepair);
		return 0;
	}

	if ((client = client_find(realm->ipaddr)) == NULL) {
		log(L_PROXY, "cannot find secret for server %s in clients file",
			realm->server);
		return 0;
	}

	/* Find UDP port. */
	if (authreq->code == PW_AUTHENTICATION_REQUEST)
		rport = realm->auth_port;
	else
		rport = realm->acct_port;

	/* Strip realm if needed, and set authreq->realm */
	if (do_striprealm && !realm->opts.nostrip)
		striprealm(namepair);
	strNcpy(authreq->realm, realmname, sizeof(authreq->realm));

	secret_key = client->secret;
	authreq->server_ipaddr = realm->ipaddr;

	/*
	 *	Is this a valid & signed request ?
	 */
	switch (authreq->code) {
		case PW_AUTHENTICATION_REQUEST:
			what = "authentication";
			calc_digest(pw_digest, authreq);
			break;
		case PW_ACCOUNTING_REQUEST:
			what = "accounting";
			if (calc_acctdigest(pw_digest, authreq) < 0) {
				/*
				 *	FIXME: invalid digest - complain
				 */
				authfree(authreq);
				return -1;
			}
			break;
	}

	/*
	 *	Now build a new request and send it to the remote radiusd.
	 *
	 *	FIXME: it could be that the id wraps around too fast if
	 *	we have a lot of requests, it might be better to keep
	 *	a seperate ID value per remote server.
	 *
	 *	OTOH the remote radius server should be smart enough to
	 *	compare _both_ ID and vector. Right ?
	 */

	/*
	 *	XXX: we re-use the vector from the original request
	 *	here, since that's easy for retransmits ...
	 */
	/* random_vector(vector); */
	memcpy(vector, authreq->vector, sizeof(vector));
	auth = (AUTH_HDR *)proxy_buffer;
	memset(auth, 0, sizeof(AUTH_HDR));
	auth->code = authreq->code;
	if (auth->code == PW_AUTHENTICATION_REQUEST)
		memcpy(auth->vector, vector, AUTH_VECTOR_LEN);

	/*
	 *	Add the request to the list of outstanding requests.
	 *	Note that authreq->server_id is a 16 bits value,
	 *	while auth->id contains only the 8 least significant
	 *	bits of that same value.
	 */
	authreq->server_id = proxy_addrequest(authreq, &proxy_id);
	auth->id = authreq->server_id & 0xFF;

	/*
	 *	Add PROXY_STATE attribute.
	 */
	pp = proxy_addinfo(authreq);

	/*
	 *	If there is no PW_CHAP_CHALLENGE attribute but there
	 *	is a PW_CHAP_PASSWORD we need to add it since we can't
	 *	use the request authenticator anymore - we changed it.
	 */
	if (pairfind(authreq->request, PW_CHAP_PASSWORD) &&
	    pairfind(authreq->request, PW_CHAP_CHALLENGE) == NULL) {
		if (!(vp = (VALUE_PAIR *)malloc(sizeof(VALUE_PAIR)))) {
			log(L_ERR|L_CONS, "no memory");
			exit(1);
		}
		memset(vp, 0, sizeof(VALUE_PAIR));

		strcpy(vp->name, "CHAP-Challenge");
		vp->attribute = PW_CHAP_CHALLENGE;
		vp->type = PW_TYPE_STRING;
		vp->length = AUTH_VECTOR_LEN;
		memcpy(vp->strvalue, authreq->vector, AUTH_VECTOR_LEN);
		pairadd((&authreq->request), vp);
	}

	/*
	 *	We need to decode and encode the password
	 *	attribute. Yuck - but a server's got to do
	 *	what a server's got to do...
	 */
	if ((vp_passwd = pairfind(authreq->request, PW_PASSWORD)) != NULL) {
		memcpy(saved_passwd, vp_passwd->strvalue, AUTH_STRING_LEN);
		passwd_recode(secret_key, vector, pw_digest,
			vp_passwd->strvalue, vp_passwd->length);
	}

	/*
	 *	Ready to send the request.
	 */
	ipaddr2str(hn, realm->ipaddr);
	DEBUG("Sending %s request of id %d to %s (server %s:%d)",
		what, auth->id, hn, realm->server, rport);

	total_length = rad_build_packet(auth, sizeof(proxy_buffer),
			authreq->request, NULL, secret_key, vector);

	/*
	 *	And send it to the remote radius server.
	 */
	sin = (struct sockaddr_in *) &saremote;
	memset ((char *) sin, '\0', sizeof (saremote));
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = htonl(realm->ipaddr);
	sin->sin_port = htons(rport);

	sendto(activefd, auth, total_length, 0,
		(struct sockaddr *)sin, sizeof(struct sockaddr_in));

	/*
	 *	Remove proxy-state from list.
	 */
	pairfree(pp->next);
	pp->next = NULL;

	/*
	 *	Restore username.
	 */
	strcpy(namepair->strvalue, saved_username);
	namepair->length = strlen(namepair->strvalue);

	/*
	 *	And restore password.
	 */
	if (vp_passwd)
		memcpy(vp_passwd->strvalue, saved_passwd, AUTH_STRING_LEN);

	return 1;
}


/*
 *	We received a response from a remote radius server.
 *	Find the original request, then return.
 *	Returns:   0 proxy found
 *		  -1 error don't reply
 */
int proxy_receive(AUTH_REQ *authreq, int activefd)
{
	VALUE_PAIR	*vp, *last, *prev, *x;
	VALUE_PAIR	*allowed_pairs;
	AUTH_REQ	*oldreq, *lastreq;
	REALM		*realm;
	char		*s;
	int		pp = -1;
	int		i;

	/*
	 *	First cleanup old outstanding requests.
	 */
	proxy_cleanup();

	/*
	 *	FIXME: calculate md5 checksum!
	 */

	/*
	 *	Find the last PROXY_STATE attribute.
	 */
	oldreq  = NULL;
	lastreq = NULL;
	last    = NULL;
	x       = NULL;
	prev    = NULL;

	for (vp = authreq->proxy_pairs; vp; vp = vp->next) {
		if (vp->attribute == PW_PROXY_STATE) {
			prev = x;
			last = vp;
		}
		x = vp;
	}
	if (last && last->strvalue) {
		/*
		 *	Merit really rapes the Proxy-State attribute.
		 *	See if it still is a valid 4-digit hex number.
		 */
		s = last->strvalue;
		if (strlen(s) == 4 && isxdigit(s[0]) && isxdigit(s[1]) &&
		    isxdigit(s[2]) && isxdigit(s[3])) {
			pp = strtol(last->strvalue, NULL, 16);
		} else {
			log(L_PROXY, "server %s mangled Proxy-State attribute",
			client_name(authreq->ipaddr));
		}
	}

	/*
	 *	Now find it in the list of outstanding requests.
	 */

	for (oldreq = proxy_requests; oldreq; oldreq = oldreq->next) {
		/*
		 *	Some servers drop the proxy pair. So
		 *	compare in another way if needed.
		 */
		if (pp >= 0 && pp == oldreq->server_id)
			break;
		if (pp < 0 &&
		    authreq->ipaddr == oldreq->server_ipaddr &&
		    authreq->id     == (oldreq->server_id & 0xFF))
			break;
		lastreq = oldreq;
	}

	if (oldreq == NULL) {
		log(L_PROXY, "Unrecognized proxy reply from server %s - ID %d",
			client_name(authreq->ipaddr), authreq->id);
		return -1;
	}

	/*
	 *	Remove oldreq from list.
	 */
	if (lastreq)
		lastreq->next = oldreq->next;
	else
		proxy_requests = oldreq->next;

	/*
	 *	Remove proxy pair from list.
	 */
	if (last) {
		if (prev)
			prev->next = last->next;
		else
			authreq->proxy_pairs = last->next;
		free(last);
	}

	/*
	 *	Only allow some attributes to be propagated from
	 *	the remote server back to the NAS, for security,
	 *	unless the remote server is designated as trusted.
	 */
	if ((realm = realm_find(oldreq->realm, 1)) && realm->opts.trusted) {
		allowed_pairs = authreq->request;
	} else {
		allowed_pairs = NULL;
		for(i = 0; allowed[i]; i++)
			pairmove2(&allowed_pairs, &(authreq->request),
					allowed[i]);
		pairfree(authreq->request);
	}

	/*
	 *      Now decrypt any encrypted attribute that we got back (and kept,
	 *      see above) from the remote server, otherwise rad_build_packet()
	 *      will happily have it encrypted once more.
	 *
	 *      It must be done here instead of in radrecv(), because that 
	 *      function doesn't know the original request authenticator that 
	 *      we used in the request we sent to the remote server. That is 
	 *      only known in the AUTHREQ - the list of which is not only used 
	 *      to keep track of incoming requests but also of the requests we 
	 *      sent as a client. Hence also the problem that we have to send
	 *      (as a client) the same request authenticator as we received 
	 *      from our client - otherwise we cannot verify the remote 
	 *      server's response authenticator. (Is that done at all btw?)
	 *
	 *      This proxying mechanism isn't great, in any case - copying the 
	 *      old request (with code AUTH_REQUEST) over this response 
	 *      'request' (with code ACCESS_ACCEPT) and then testing the code 
	 *      once again for the type of function to perform in radrespond().
	 *      No wonder there have been a couple of memory leaks...!  ;-)
	 *
	 *      The radius client and server should really be two separate
	 *      entities, that would clean up a lot of this type of issues.
	 */
	for(vp = allowed_pairs; vp; vp = vp->next) {
		if (vp->flags.encrypt) {
			decrypt_attr(oldreq->secret, oldreq->vector, vp);
		}
	}

	/*
	 *	Now rebuild the AUTHREQ struct, so that the
	 *	normal functions can process it.
	 */
	oldreq->server_reply = allowed_pairs;
	oldreq->server_code  = authreq->code;
	oldreq->validated    = 1;
	memcpy(authreq->data, oldreq->data, oldreq->data_len);
	free(oldreq->data);
	oldreq->data = authreq->data;
	memcpy(authreq, oldreq, sizeof(AUTH_REQ));

	free(oldreq);

	return 0;
}

