/*
 * acct.c	Accounting routines.
 *
 *		Copyright 1996-2001	Cistron Internet Services B.V.
 *		Copyright 2002		Cistron IP B.V.
 */
char acct_rcsid[] =
"$Id: acct.c,v 1.20 2002/01/22 14:13:45 miquels Exp $";

#include	<sys/types.h>
#include	<sys/socket.h>
#include	<sys/time.h>
#include	<sys/file.h>
#include	<sys/stat.h>
#include	<netinet/in.h>

#include	<stdio.h>
#include	<stdlib.h>
#include	<netdb.h>
#include	<pwd.h>
#include	<time.h>
#include	<ctype.h>
#include	<unistd.h>
#include	<signal.h>
#include	<errno.h>
#include	<sys/wait.h>

#include	"radiusd.h"
#include	"radutmp.h"

static char trans[64] =
   "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
#define ENC(c) trans[c]

#define NR_DETAIL_FN	8
static char *detail_fn[NR_DETAIL_FN] = { "%N/detail" };
static int detail_fn_count = 0;
static char *detail_auth[] = { "Verified", "None", "Unverified" };
static char porttypes[] = "ASITX";

/*
 *	used for caching radutmp lookups.
 */
typedef struct nas_port {
	UINT4			nasaddr;
	int			port;
	off_t			offset;
	struct nas_port		*next;
} NAS_PORT;
static NAS_PORT *nas_port_list = NULL;


/*
 *	Lookup a NAS_PORT in the nas_port_list
 */
static NAS_PORT *nas_port_find(UINT4 nasaddr, int port)
{
	NAS_PORT	*cl;

	for(cl = nas_port_list; cl; cl = cl->next)
		if (nasaddr == cl->nasaddr &&
			port == cl->port)
			break;
	return cl;
}

/*
 *	Lock / unlock file.
 */
static int lockfd(int fd, int lock)
{
	int		r;
	int		op;

#if defined(F_LOCK) && !defined(BSD)
	op = lock ? (lock == 1 ? F_LOCK : F_TLOCK) : F_ULOCK;
	r = lockf(fd, op, 0);
#else
	op = lock ? LOCK_EX : LOCK_UN;
	if (lock == 2) lock |= LOCK_NB;
	r = flock(fd, op);
#endif
	return r;
}


/*
 *	UUencode 4 bits base64. We use this to turn a 4 byte field
 *	(an IP adres) into 6 bytes of ASCII. This is used for the
 *	wtmp file if we didn't find a short name in the naslist file.
 */
char *uue(void *in)
{
	int i;
	static unsigned char res[7];
	unsigned char *data = (char *)in;

	res[0] = ENC( data[0] >> 2 );
	res[1] = ENC( ((data[0] << 4) & 060) + ((data[1] >> 4) & 017) );
	res[2] = ENC( ((data[1] << 2) & 074) + ((data[2] >> 6) & 03) );
	res[3] = ENC( data[2] & 077 );

	res[4] = ENC( data[3] >> 2 );
	res[5] = ENC( (data[3] << 4) & 060 );
	res[6] = 0;

	for(i = 0; i < 6; i++) {
		if (res[i] == ' ') res[i] = '`';
		if (res[i] < 32 || res[i] > 127)
			printf("uue: protocol error ?!\n");
	}
	return res;
}


/*
 *	Convert a struct radutmp to a normal struct utmp
 *	as good as we possibly can.
 */
static void make_wtmp(struct radutmp *ut, struct utmp *wt, int status)
{
	char		buf[32];
	NAS		*cl;
	char		*s;

	/*
	 *	Fill out the UTMP struct for the radwtmp file.
	 *	(this one must be "last" - compatible).
	 */
#ifdef __linux__
	/*
	 *	Linux has a field for the client address.
	 */
	wt->ut_addr = ut->framed_address;
#endif
	/*
	 *	We use the tty field to store the terminal servers' port
	 *	and address so that the tty field is unique.
	 */
	s = "";
	if ((cl = nas_find(ntohl(ut->nas_address))) != NULL &&
	    strcmp(cl->longname, "DEFAULT") != 0)
		s = cl->shortname;
	if (s == NULL || s[0] == 0) s = uue(&(ut->nas_address));
#if UT_LINESIZE > 9
	sprintf(buf, "%03d:%.20s", ut->nas_port, s);
#else
	sprintf(buf, "%02d%.20s", ut->nas_port, s);
#endif
	strncpy(wt->ut_line, buf, UT_LINESIZE);

	/*
	 *	We store the dynamic IP address in the hostname field.
	 */
#ifdef UT_HOSTSIZE
	if (ut->framed_address) {
		ipaddr2str(buf, ntohl(ut->framed_address));
		strncpy(wt->ut_host, buf, UT_HOSTSIZE);
	}
#endif
#ifdef __svr4__
	wt->ut_xtime = ut->time;
#else
	wt->ut_time = ut->time;
#endif
#ifdef USER_PROCESS
	/*
	 *	And we can use the ID field to store
	 *	the protocol.
	 */
	if (ut->proto == P_PPP)
		strcpy(wt->ut_id, "P");
	else if (ut->proto == P_SLIP)
		strcpy(wt->ut_id, "S");
	else
		strcpy(wt->ut_id, "T");
	wt->ut_type = status == PW_STATUS_STOP ? DEAD_PROCESS : USER_PROCESS;
#endif
	if (status == PW_STATUS_STOP)
		wt->ut_name[0] = 0;
}


/*
 *	Zap a user, or all users on a NAS, from the radutmp file.
 */
int radzap(UINT4 nasaddr, int port, char *user, time_t t, int dowtmp)
{
	struct radutmp	u;
	struct utmp	wt;
	FILE		*fp;
	int		fd;
	UINT4		netaddr;

	if (t == 0) time(&t);
	if (dowtmp)
		fp = fopen(radwtmp_path, "a");
	else
		fp = NULL;
	netaddr = htonl(nasaddr);

	if ((fd = open(radutmp_path, O_RDWR|O_CREAT, 0644)) >= 0) {
		int r;

		/*
		 *	Lock the utmp file.
		 */
		lockfd(fd, 1);

	 	/*
		 *	Find the entry for this NAS / portno combination.
		 */
		r = 0;
		while (read(fd, &u, sizeof(u)) == sizeof(u)) {
			if (((nasaddr != 0 && netaddr != u.nas_address) ||
			      (port >= 0   && port    != u.nas_port) ||
			      (user != NULL && strcmp(u.login, user) != 0) ||
			       u.type != P_LOGIN))
				continue;
			/*
			 *	Match. Zap it.
			 */
			if (lseek(fd, -(off_t)sizeof(u), SEEK_CUR) < 0) {
				log(L_ERR, "Accounting: radzap: negative lseek!\n");
				lseek(fd, (off_t)0, SEEK_SET);
			}
			u.type = P_IDLE;
			u.time = t;
			write(fd, &u, sizeof(u));

			/*
			 *	Add a logout entry to the wtmp file.
			 */
			if (fp != NULL)  {
				make_wtmp(&u, &wt, PW_STATUS_STOP);
				fwrite(&wt, sizeof(wt), 1, fp);
			}
		}
		close(fd);
	}
	if (fp) fclose(fp);

	return 0;
}


/*
 *	Store logins in the RADIUS utmp file.
 */
int rad_accounting_radxtmp(AUTH_REQ *authreq, int dowtmp)
{
	struct radutmp	ut, u;
	struct utmp	wt;
	VALUE_PAIR	*vp;
	int		rb_record = 0;
	int		status = -1;
	int		nas_address = 0;
	int		framed_address = 0;
	int		protocol = -1;
	FILE		*fp;
	time_t		t;
	int		fd;
	int		ret = 0;
	int		just_an_update = 0;
	int		port_seen = 0;
	int		nas_port_type = 0;
	int		off;

	/*
	 *	Which type is this.
	 */
	if ((vp = pairfind(authreq->request, PW_ACCT_STATUS_TYPE)) == NULL) {
		log(L_ERR, "Accounting: no Accounting-Status-Type record.");
		return -1;
	}
	status = vp->lvalue;
	if (status == PW_STATUS_ACCOUNTING_ON ||
	    status == PW_STATUS_ACCOUNTING_OFF) rb_record = 1;

	if (!rb_record &&
	    (vp = pairfind(authreq->request, PW_USER_NAME)) == NULL) do {
		int check1 = 0;
		int check2 = 0;

		/*
		 *	ComOS (up to and including 3.9.1c1) does not send
		 *	standard PW_STATUS_ACCOUNTING_XXX messages.
		 *
		 *	Check for:  o no Acct-Session-Time, or time of 0
		 *		    o Acct-Session-Id of "00000000".
		 *
		 *	We could also check for NAS-Port, that attribute
		 *	should NOT be present (but we don't right now).
	 	 */
		if ((vp = pairfind(authreq->request, PW_ACCT_SESSION_TIME))
		     == NULL || vp->lvalue == 0)
			check1 = 1;
		if ((vp = pairfind(authreq->request, PW_ACCT_SESSION_ID))
		     != NULL && vp->length == 8 &&
		     memcmp(vp->strvalue, "00000000", 8) == 0)
			check2 = 1;
		if (check1 == 0 || check2 == 0) {
#if 0 /* Cisco sometimes sends START records without username. */
			log(L_ERR, "Accounting: no username in record");
			return -1;
#else
			break;
#endif
		}
		log(L_INFO, "Accounting: converting reboot records.");
		if (status == PW_STATUS_STOP)
			status = PW_STATUS_ACCOUNTING_OFF;
		if (status == PW_STATUS_START)
			status = PW_STATUS_ACCOUNTING_ON;
		rb_record = 1;
	} while(0);

#ifdef NT_DOMAIN_HACK
        if (!rb_record && vp) {
		char buffer[AUTH_STRING_LEN];
		char *ptr;
		if ((ptr = strchr(vp->strvalue, '\\')) != NULL) {
			strncpy(buffer, ptr + 1, sizeof(buffer));
			buffer[sizeof(buffer) - 1] = 0;
			strcpy(vp->strvalue, buffer);
		}
	}
#endif

	/*
	 *	Add any specific attributes for this username.
	 */
	if (!rb_record && vp != NULL) {
		hints_setup(authreq->request);
		presuf_setup(authreq->request);
	}
	time(&t);
	memset(&ut, 0, sizeof(ut));
	memset(&wt, 0, sizeof(wt));
	ut.porttype = 'A';

	/*
	 *	First, find the interesting attributes.
	 */
	for (vp = authreq->request; vp; vp = vp->next) {
		switch (vp->attribute) {
			case PW_USER_NAME:
				strncpy(ut.login, vp->strvalue, RUT_NAMESIZE);
				strncpy(wt.ut_name, vp->strvalue, UT_NAMESIZE);
				break;
			case PW_LOGIN_IP_HOST:
			case PW_FRAMED_IP_ADDRESS:
				if (vp->lvalue != 0) {
					framed_address = vp->lvalue;
					ut.framed_address = htonl(vp->lvalue);
				}
				break;
			case PW_FRAMED_PROTOCOL:
				protocol = vp->lvalue;
				break;
			case PW_NAS_IP_ADDRESS:
				nas_address = vp->lvalue;
				ut.nas_address = htonl(vp->lvalue);
				break;
			case PW_NAS_PORT:
				ut.nas_port = vp->lvalue;
				port_seen = 1;
				break;
			case PW_ACCT_DELAY_TIME:
				ut.delay = vp->lvalue;
				break;
			case PW_ACCT_SESSION_ID:
				/*
				 *	If length > 8, only store the
				 *	last 8 bytes.
				 */
				off = vp->length - sizeof(ut.session_id);
				/*
				 *	Ascend is br0ken - it adds a \0
				 *	to the end of any string.
				 *	Compensate.
				 */
				if (vp->length > 0 &&
				    vp->strvalue[vp->length - 1] == 0)
					off--;
				if (off < 0) off = 0;
				memcpy(ut.session_id, vp->strvalue + off,
					sizeof(ut.session_id));
				break;
			case PW_NAS_PORT_TYPE:
				if (vp->lvalue >= 0 && vp->lvalue <= 4)
					ut.porttype = porttypes[vp->lvalue];
				nas_port_type = vp->lvalue;
				break;
			case PW_CALLING_STATION_ID:
				strncpy(ut.caller_id, vp->strvalue,
					sizeof(ut.caller_id));
				ut.caller_id[sizeof(ut.caller_id) - 1] = 0;
				break;
		}
	}

	/*
	 *	If we didn't find out the NAS address, use the
	 *	originator's IP address.
	 */
	if (nas_address == 0) {
		nas_address = authreq->ipaddr;
		ut.nas_address = htonl(nas_address);
	}

	if (protocol < 0)
		ut.proto = 'T';
	else if (protocol == PW_SLIP)
		ut.proto = 'S';
	else if (protocol == PW_PPP)
		ut.proto = 'P';
	else
		ut.proto = 'P';
	ut.time = t - ut.delay;
	make_wtmp(&ut, &wt, status);

	/*
	 *	See if this was a portmaster reboot.
	 */
	if (status == PW_STATUS_ACCOUNTING_ON && nas_address) {
		log(L_INFO, "NAS %s restarted (Accounting-On packet seen)",
			nas_name(nas_address));
		radzap(nas_address, -1, NULL, ut.time, dowtmp);
		return 0;
	}
	if (status == PW_STATUS_ACCOUNTING_OFF && nas_address) {
		log(L_INFO, "NAS %s rebooted (Accounting-Off packet seen)",
			nas_name(nas_address));
		radzap(nas_address, -1, NULL, ut.time, dowtmp);
		return 0;
	}

	/*
	 *	If we don't know this type of entry pretend we succeeded.
	 */
	if (status != PW_STATUS_START &&
	    status != PW_STATUS_STOP &&
	    status != PW_STATUS_ALIVE) {
		log(L_ERR, "NAS %s port %d unknown packet type %d)",
			nas_name(nas_address), ut.nas_port, status);
		return 0;
	}

	/*
	 *	Perhaps we don't want to store this record into
	 *	radutmp/radwtmp. We skip records:
	 *
	 *	- without a NAS-Port (telnet / tcp access)
	 *	- with the username "!root" (console admin login)
	 *	- with Port-Type = Sync (leased line up/down)
	 */
	if (!port_seen ||
	    strncmp(ut.login, "!root", RUT_NAMESIZE) == 0
#if 0 /* I HATE Ascend - they label ISDN as sync */
	    || nas_port_type == PW_NAS_PORT_SYNC
#endif
	   )
		return 0;

	/*
	 *	Enter into the radutmp file.
	 */
	if ((fd = open(radutmp_path, O_RDWR|O_CREAT, 0644)) >= 0) {
		NAS_PORT *cache;
		int r;

		/*
		 *	Lock the utmp file.
		 */
		lockfd(fd, 1);

	 	/*
		 *	Find the entry for this NAS / portno combination.
		 */
		off = 0;
		if ((cache = nas_port_find(ut.nas_address, ut.nas_port)) != NULL) {
			lseek(fd, (off_t)cache->offset, SEEK_SET);
			off = (off_t)cache->offset;
		}

		r = 0;
		while (read(fd, &u, sizeof(u)) == sizeof(u)) {
			off += sizeof(u);
			if (u.nas_address != ut.nas_address ||
			    u.nas_port    != ut.nas_port)
				continue;

			if (status == PW_STATUS_STOP &&
			    strncmp(ut.session_id, u.session_id, 
			     sizeof(u.session_id)) != 0) {
				/*
				 *	Don't complain if this is not a
				 *	login record (some clients can
				 *	send _only_ logout records).
				 */
				if (u.type == P_LOGIN)
					log(L_ERR,
		"Accounting: logout: entry for NAS %s port %d has wrong ID",
					nas_name(nas_address), u.nas_port);
				r = -1;
				break;
			}

			if (status == PW_STATUS_START &&
			    strncmp(ut.session_id, u.session_id, 
			     sizeof(u.session_id)) == 0  &&
			    u.time >= ut.time) {
				if (u.type == P_LOGIN) {
					log(L_INFO,
		"Accounting: login: entry for NAS %s port %d duplicate",
					nas_name(nas_address), u.nas_port);
					r = -1;
					dowtmp = 0;
					break;
				}
				log(L_ERR,
		"Accounting: login: entry for NAS %s port %d wrong order",
				nas_name(nas_address), u.nas_port);
				r = -1;
				break;
			}

			/*
			 *	FIXME: the ALIVE record could need
			 *	some more checking, but anyway I'd
			 *	rather rewrite this mess -- miquels.
			 */
			if (status == PW_STATUS_ALIVE &&
			    strncmp(ut.session_id, u.session_id, 
			     sizeof(u.session_id)) == 0  &&
			    u.type == P_LOGIN) {
				/*
				 *	Keep the original login time.
				 */
				ut.time = u.time;
				if (u.login[0] != 0)
					just_an_update = 1;
			}

			if (lseek(fd, -(off_t)sizeof(u), SEEK_CUR) < 0) {
				log(L_ERR, "Accounting: negative lseek!\n");
				lseek(fd, (off_t)0, SEEK_SET);
				off = 0;
			} else
				off -= sizeof(u);
			r = 1;
			break;
		}

		if (r >= 0 &&  (status == PW_STATUS_START ||
				status == PW_STATUS_ALIVE)) {
			if (cache == NULL) {
			   if ((cache = malloc(sizeof(NAS_PORT))) != NULL) {
				   cache->nasaddr = ut.nas_address;
				   cache->port = ut.nas_port;
				   cache->offset = off;
				   cache->next = nas_port_list;
				   nas_port_list = cache;
			   }
			}
			ut.type = P_LOGIN;
			write(fd, &ut, sizeof(u));
		}
		if (status == PW_STATUS_STOP) {
			if (r > 0) {
				u.type = P_IDLE;
				u.time = ut.time;
				u.delay = ut.delay;
				write(fd, &u, sizeof(u));
			} else if (r == 0) {
				log(L_ERR,
		"Accounting: logout: login entry for NAS %s port %d not found",
				nas_name(nas_address), ut.nas_port);
				r = -1;
			}
		}
		close(fd);
	} else {
		log(L_ERR, "Accounting: %s: %s", radutmp_path, strerror(errno));
		ret = -1;
	}

	/*
	 *	Don't write wtmp if we don't have a username, or
	 *	if this is an update record and the original record
	 *	was already written.
	 */
	if ((status != PW_STATUS_STOP && wt.ut_name[0] == 0) || just_an_update)
		dowtmp = 0;

	/*
	 *	Write a RADIUS wtmp log file.
	 */
	if (dowtmp && (fp = fopen(radwtmp_path, "a")) != NULL) {
		ret = 0;
		fwrite(&wt, sizeof(wt), 1, fp);
		fclose(fp);
	}


	return ret;
}

static int radius_xlate2(char *str, char *buf, int bufsz, AUTH_REQ *authreq)
{
	VALUE_PAIR	*pair;
	NAS		*cl;
	UINT4		nas;
	char		rep[256];
	char		*p, *s;
	int		len, i = 0;

	bufsz--;

	for (p = str; *p; p++) {
		if (i >= bufsz) break;
		if (*p != '%') {
			buf[i++] = *p;
			continue;
		}
		if (*++p == 0) break;
		switch(*p) {
			case '%':
				rep[0] = *p;
				rep[1] = 0;
				break;
			case 'N':
				/*
				 *	Find IP address in order from:
				 *	- remote proxy
				 *	- NAS-IP-Address
				 *	- Client-IP-Address
				 *
				 *	Then translate it to a hostname
				 *	trying in order:
				 *	- short nasname
				 *	- long nasname
				 *	- DNS lookup.
				 */
				cl = NULL;
				nas = authreq->ipaddr;
				if ((pair = pairfind(authreq->request,
				     PW_NAS_IP_ADDRESS)) != NULL)
					nas = pair->lvalue;

				/*
				 *	Reply from remote radius server:
				 *	use server_ipaddr
				 */
				if (authreq->server_ipaddr)
					nas = authreq->server_ipaddr;

				if ((cl = nas_find(nas)) != NULL) {
					/* {short,long}name are < 128 bytes */
					if (cl->shortname[0])
						strcpy(rep, cl->shortname);
					else
						strcpy(rep, cl->longname);
				}

				if (cl == NULL) {
					s = ip_hostname(nas);
					strNcpy(rep, s, sizeof(rep));
				}
				break;
			default:
				rep[0] = '%';
				rep[1] = *p;
				rep[2] = 0;
				break;
		}
		len = strlen(rep);
		if (i + len >= bufsz) break;
		strcpy(buf + i, rep);
		i += len;
	}
	buf[i++] = 0;

	return 0;
}

int rad_accounting_detail(AUTH_REQ *authreq, int authtype, char *f)
{
	VALUE_PAIR	*pair;
	FILE		*outfd;
	struct stat	st;
	struct timeval	tv;
	time_t		curtime;
	char		detail[512];
	char		ipaddr[16];
	char		*ptr;
	char		*s;
	int		len, ret = 0;

	/*
	 *	See if we have an accounting directory. If not,
	 *	return.
	 */
	if (stat(radacct_dir, &st) < 0)
		return 0;
	curtime = time(0);

	/*
	 *	Calculate detail file location. See if we need to
	 *	create a subdirectory.
	 */
	len = strlen(radacct_dir) + 1;
	if (len >= sizeof(detail))
		return -1;
	strcpy(detail, radacct_dir);
	strcat(detail, "/");
	ptr = detail + len;
	radius_xlate2(f, ptr, sizeof(detail) - len, authreq);
	if ((s = strrchr(detail, '/')) != NULL && s > ptr) {
		*s = 0;
		(void) mkdir(detail, 0755);
		*s = '/';
	}

	/*
	 *	Open detail file, lock it, and write the details.
	 */
	do {
		if ((outfd = fopen(detail, "a")) == NULL) {
			log(L_ERR, "Acct: Couldn't open file %s", detail);
			ret = -1;
			break;
		}

		/*
		 *	Try to lock the detail file, reopen it
		 *	if it fails.
		 */
		if (lockfd(fileno(outfd), 2) < 0) {
			tv.tv_sec = 0;
			tv.tv_usec = 25000;
			select(0, NULL, NULL, NULL, &tv);
			fclose(outfd);
			continue;
		}

		/* Post a timestamp */
		fputs(ctime(&curtime), outfd);

		/* Write each attribute/value to the log file */
		pair = authreq->request;
		while (pair != NULL) {
			if (pair->attribute != PW_PASSWORD) {
				fputs("\t", outfd);
				fprint_attr_val(outfd, pair);
				fputs("\n", outfd);
			}
			pair = pair->next;
		}

		/*
		 *	Add non-protocol attibutes.
		 */
		if (authreq->server_ipaddr) {
			ipaddr2str(ipaddr, authreq->server_ipaddr);
			fprintf(outfd, "\tCistron-Proxied-To = %s\n", ipaddr);
		}
		fprintf(outfd, "\tTimestamp = %ld\n", curtime);
		if (authtype >= 0 && authtype <= 2)
			fprintf(outfd, "\tRequest-Authenticator = %s\n",
				detail_auth[authtype]);
		fputs("\n", outfd);
		fclose(outfd);
	} while(0);

	return ret;
}


/*
 *	Add a detail file.
 */
int rad_add_detail(char *fn)
{
	if (detail_fn_count >= NR_DETAIL_FN)
		return -1;
	detail_fn[detail_fn_count++] = strdup(fn);
	return 0;
}


/*
 *	rad_accounting: call both the old and new style accounting functions.
 */
int rad_accounting(AUTH_REQ *authreq, int activefd)
{
	int i, reply = 0;
	int auth;
	char pw_digest[16];

	/*
	 *	See if we know this client, then check the
	 *	request authenticator.
	 */
	auth = calc_acctdigest(pw_digest, authreq);

	if (auth < 0) {
		authfree(authreq);
		return -1;
	}

	if (log_stripped_names) {
		/*
		 *	rad_accounting_xtmp strips authreq for us. If
		 *	we run it first, the stripped info will also
		 *	get into the "detail" file.
		 */
		if (rad_accounting_radxtmp(authreq, use_wtmp) == 0)
			reply = 1;
		for (i = 0; i < 1 || i < detail_fn_count; i++) {
			if (rad_accounting_detail(authreq, auth,
			    detail_fn[i]) == 0)
				reply = 1;
		}
	} else {
		/*
		 *	First log into the details file, before the
		 *	username gets stripped by rad_accounting_xtmp.
		 */
		for (i = 0; i < 1 || i < detail_fn_count; i++) {
			if (rad_accounting_detail(authreq, auth,
			    detail_fn[i]) == 0)
				reply = 1;
		}
		if (rad_accounting_radxtmp(authreq, use_wtmp) == 0)
			reply = 1;
	}

	if (reply) {
		/*
		 *	Now send back an ACK to the NAS.
		 */
		rad_send_reply(PW_ACCOUNTING_RESPONSE,
			authreq, NULL, NULL, activefd);
	}

	authfree(authreq);

	return reply ? 0 : -1;
}


/*
 *	Timeout handler (10 secs)
 */
static int got_alrm;
static void alrm_handler()
{
	got_alrm = 1;
}

/*
 *	Check one terminal server to see if a user is logged in.
 */
static int rad_check_ts(struct radutmp *ut)
{
	int	pid, st, e;
	int	n;
	NAS	*nas;
	char	address[16];
	char	port[8];
	char	session_id[12];
	char	*s;
	void	(*handler)(int);

	/*
	 *	Find NAS type.
	 */
	if ((nas = nas_find(ntohl(ut->nas_address))) == NULL) {
		log(L_ERR, "Accounting: unknown NAS");
		return -1;
	}

	/*
	 *	Fork.
	 */
	handler = signal(SIGCHLD, SIG_DFL);
	if ((pid = fork()) < 0) {
		log(L_ERR, "Accounting: fork: %s", strerror(errno));
		signal(SIGCHLD, handler);
		return -1;
	}

	if (pid > 0) {
		/*
		 *	Parent - Wait for checkrad to terminate.
		 *	We timeout in 10 seconds.
		 */
		got_alrm = 0;
		signal(SIGALRM, alrm_handler);
		alarm(10);
		while((e = waitpid(pid, &st, 0)) != pid)
			if (e < 0 && (errno != EINTR || got_alrm))
				break;
		alarm(0);
		signal(SIGCHLD, handler);
		if (got_alrm) {
			kill(pid, SIGTERM);
			sleep(1);
			kill(pid, SIGKILL);
			log(L_ERR, "Check-TS: timeout waiting for checkrad");
			return 2;
		}
		if (e < 0) {
			log(L_ERR, "Check-TS: unknown error in waitpid()");
			return 2;
		}
		return WEXITSTATUS(st);
	}

	/*
	 *	Child - exec checklogin with the right parameters.
	 */
	for (n = 32; n >= 3; n--)
		close(n);

	ipaddr2str(address, ntohl(ut->nas_address));
	sprintf(port, "%d", ut->nas_port);
	sprintf(session_id, "%.8s", ut->session_id);

	s = CHECKRAD2;
	execl(CHECKRAD2, "checkrad", nas->nastype, address, port,
		ut->login, session_id, NULL);
	if (errno == ENOENT) {
		s = CHECKRAD1;
		execl(CHECKRAD1, "checklogin", nas->nastype, address, port,
			ut->login, session_id, NULL);
	}
	log(L_ERR, "Check-TS: exec %s: %s", s, strerror(errno));

	/*
	 *	Exit - 2 means "some error occured".
	 */
	exit(2);
}

/*
 *	See if a user is already logged in.
 *
 *	Check twice. If on the first pass the user exceeds his
 *	max. number of logins, do a second pass and validate all
 *	logins by querying the terminal server (using eg. telnet).
 *
 *	Returns: 0 == OK, 1 == double logins, 2 == multilink attempt
 */
int rad_check_multi(char *name, VALUE_PAIR *request, VALUE_PAIR *reply,
	int maxsimul)
{
	VALUE_PAIR	*vp;
	FILE		*wfp;
	struct radutmp	u, u2;
	struct utmp	wt;
	char		caller_id[16];
	int		multi_count = 0;
	int		portlimit;
	int		count;
	int		fd;

	if ((fd = open(radutmp_path, O_CREAT|O_RDWR, 0644)) < 0)
		return 0;

	/*
	 *	First pass: just read the radutmp file.
	 */
	count = 0;
	while(read(fd, &u, sizeof(u)) == sizeof(u))
		if (strncmp(name, u.login, RUT_NAMESIZE) == 0
		    && u.type == P_LOGIN)
			count++;

	if (count < maxsimul) {
		close(fd);
		return 0;
	}
	lseek(fd, (off_t)0, SEEK_SET);

	/*
	 *	Setup some stuff, like for MPP detection.
	 */
	caller_id[0] = 0;
	portlimit = 1;
	if ((vp = pairfind(request, PW_CALLING_STATION_ID)) != NULL)
		strNcpy(caller_id, vp->strvalue, sizeof(caller_id));
	if ((vp = pairfind(reply, PW_PORT_LIMIT)) != NULL)
		portlimit = vp->lvalue;

	/*
	 *	Allright, there are too many concurrent logins.
	 *	Check all registered logins by querying the
	 *	terminal server directly.
	 */
	count = 0;
	while (read(fd, &u, sizeof(u)) == sizeof(u)) {
		if (strncmp(name, u.login, RUT_NAMESIZE) == 0
		    && u.type == P_LOGIN) {
			if (rad_check_ts(&u) == 1) {
				count++;
				/*
				 *	Does it look like a MPP attempt?
				 */
				if (caller_id[0] &&
				    strcmp(caller_id, u.caller_id) == 0)
					multi_count++;
			}
			else {
				/*
				 *	False record - zap it.
				 */
				lockfd(fd, 1);
				lseek(fd, -(off_t)sizeof(u), SEEK_CUR);
				read(fd, &u2, sizeof(u2));
				if (!strncmp(u2.login, u.login, RUT_NAMESIZE)){
					u.type = P_IDLE;
					lseek(fd, -(off_t)sizeof(u), SEEK_CUR);
					write(fd, &u, sizeof(u));
				}
				lockfd(fd, 0);

				if ((wfp = fopen(radwtmp_path, "a")) != NULL) {
					make_wtmp(&u, &wt, PW_STATUS_STOP);
					fwrite(&wt, sizeof(wt), 1, wfp);
					fclose(wfp);
				}
			}
		}
	}
	close(fd);

	return (count < maxsimul) ? 0 : (multi_count ? 2 : 1);
}

