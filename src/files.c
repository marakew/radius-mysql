/*
 *
 * files.c	Routines to read config files into memory,
 *		and to lookup values.
 *
 *		Copyright 1996-2001	Cistron Internet Services B.V.
 *		Copyright 2002		Cistron IP B.V.
 */

char files_rcsid[] =
"$Id: files.c,v 1.40 2002/02/06 15:23:16 miquels Exp $";

#include	<sys/types.h>
#include	<sys/socket.h>
#include	<sys/time.h>
#include	<sys/stat.h>
#include	<netinet/in.h>

#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<netdb.h>
#include	<pwd.h>
#include	<grp.h>
#include	<time.h>
#include	<ctype.h>
#include	<fcntl.h>
#include	<errno.h>

#include	"radiusd.h"
#include	"cache.h"

#ifdef USE_DBM
#  include      <dbm.h>
#endif
#ifdef USE_NDBM
#  include      <ndbm.h>
#endif
#ifdef USE_GDBM     
#  include      <gdbm-ndbm.h>
#endif
#ifdef USE_DB
#  include      <db.h>
#endif

static	int  huntgroup_match(VALUE_PAIR *, char *);

#ifdef DBIF_DB
#define dptr  data
#define dsize size
#define datum DBT
#endif

#ifdef DBIF_DBM
static void	*dbmfile;
#endif
#ifdef DBIF_NDBM
static DBM	*dbmfile;
#endif
#ifdef DBIF_DB
static DB	*dbmfile;
#endif

static PAIR_LIST	*defaults;
static PAIR_LIST	*users;
static PAIR_LIST	*huntgroups;
static PAIR_LIST	*hints;
static RADCLIENT	*clients;
static NAS		*naslist;
static REALM		*realms;

#ifdef USE_DBX
PAIR_LIST *rad_dbm_lookup(char *name);
#endif

/*
 *	Returns a copy of a pair list.
 */
static VALUE_PAIR *paircopy(VALUE_PAIR *from)
{
	VALUE_PAIR *vp = NULL;
	VALUE_PAIR *last = NULL;
	VALUE_PAIR *i, *t;

	for(i = from; i; i = i->next) {
		if ((t = malloc(sizeof(VALUE_PAIR))) == NULL)
			continue;
		memcpy(t, i, sizeof(VALUE_PAIR));
		t->next = NULL;
		if (last)
			last->next = t;
		else
			vp = t;
		last = t;
	}

	return vp;
}


/*
 *	Strip a username, based on Prefix/Suffix from the "users" file.
 *	This is used by the accounting routines in acct.c to strip
 *	the username before logging it in radutmp/radwtmp/detail files.
 *	Not 100% safe, since we don't compare attributes.
 *
 *	In DBM mode we only use the DEFAULT entries, since I am lazy
 *	and do not want to duplicate user_find(). Besides, it would
 *	be pretty expensive disk-io wise.
 */
void presuf_setup(VALUE_PAIR *request_pairs)
{
	PAIR_LIST	*pl;
	PAIR_LIST	*userlist = users;
	VALUE_PAIR	*presuf_pair;
	VALUE_PAIR	*name_pair;
	VALUE_PAIR	*tmp;
	char		name[32];

	if ((name_pair = pairfind(request_pairs, PW_USER_NAME)) == NULL)
		return;

#ifdef USE_DBX
	if (use_dbm) {
		rad_dbm_open();
		userlist = defaults;
	}
#endif

	for (pl = userlist; pl; pl = pl->next) {
		/*
		 *	Matching name?
		 */
		if (strncmp(pl->name, "DEFAULT", 7) != 0 &&
		    strcmp(pl->name, name_pair->strvalue) != 0)
			continue;
		/*
		 *	Find Prefix / Suffix.
		 */
		if ((presuf_pair = pairfind(pl->check, PW_PREFIX)) == NULL &&
		    (presuf_pair = pairfind(pl->check, PW_SUFFIX)) == NULL)
			continue;
		if (presufcmp(presuf_pair, name_pair->strvalue, name, sizeof(name)) != 0)
			continue;
		/*
		 *	See if username must be stripped.
		 */
		if ((tmp = pairfind(pl->check, PW_STRIP_USERNAME)) != NULL &&
		    tmp->lvalue == 0)
			continue;
		/* sizeof(name_pair->strvalue) > sizeof(name) */
		strcpy(name_pair->strvalue, name);
		name_pair->length = strlen(name_pair->strvalue);
		break;
	}
}



/*
 *	Compare a portno with a range.
 */
static int portcmp(VALUE_PAIR *check, VALUE_PAIR *request)
{
	char buf[AUTH_STRING_LEN];
	char *s, *p;
	unsigned int lo, hi;
	unsigned int port = request->lvalue;

	/* Same size */
	strcpy(buf, check->strvalue);
	s = strtok(buf, ",");
	while(s) {
		if ((p = strchr(s, '-')) != NULL)
			p++;
		else
			p = s;
		lo = strtoul(s, NULL, 10);
		hi = strtoul(p, NULL, 10);
		/*DEBUG2("portcmp() got lo %u, hi %u, port %u", lo, hi, port);*/
		if (lo <= port && port <= hi) {
			return 0;
		}
		s = strtok(NULL, ",");
	}

	return -1;
}


/*
 *	See if user is member of a group.
 *	We also handle additional groups.
 */
static int groupcmp(VALUE_PAIR *check, char *username)
{
	struct passwd *pwd;
	struct group *grp;
	char **member;
	int retval;

	if (cache_passwd && (retval = H_groupcmp(check, username)) != -2)
		return retval;

	if ((pwd = rad_getpwnam(username)) == NULL)
		return -1;

	if ((grp = getgrnam(check->strvalue)) == NULL)
		return -1;

	retval = (pwd->pw_gid == grp->gr_gid) ? 0 : -1;
	if (retval < 0) {
		for (member = grp->gr_mem; *member && retval; member++) {
			if (strcmp(*member, pwd->pw_name) == 0)
				retval = 0;
		}
	}
	return retval;
}

/*
 *	Compare prefix/suffix.
 */
int presufcmp(VALUE_PAIR *check, char *name, char *rest, int restlen)
{
	char *realm;
	int len, namelen;
	int ret = -1;
	int i;

#if 0 /* DEBUG */
	printf("Comparing %s and %s, check->attr is %d\n",
		name, check->strvalue, check->attribute);
#endif

	/*
	 *	Temporarily strip off the realm, unless ofcourse
	 *	we explicitly want to compare against a realm.
	 */
	realm = NULL;
	if (strchr(check->strvalue, '@') == NULL) {
		realm = strchr(name, '@');
		if (realm) *realm = 0;
	}

	len = strlen(check->strvalue);
	switch (check->attribute) {
		case PW_PREFIX:
			ret = strncmp(name, check->strvalue, len);
			if (ret == 0 && rest)
				strNcpy(rest, name + len, restlen);
			break;
		case PW_SUFFIX:
			namelen = strlen(name);
			if (namelen < len)
				break;
			ret = strcmp(name + namelen - len, check->strvalue);
			if (ret == 0 && rest) {
				i = namelen - len;
				if (i >= restlen - 1) i = restlen - 1;
				strncpy(rest, name, i);
				rest[i] = 0;
			}
			break;
	}

	if (realm) *realm = '@';
	if (ret == 0 && rest && realm) {
		len = strlen(rest);
		strNcpy(rest + len, realm, restlen -len);
	}

	return ret;
}



/*
 *	Compare two pair lists except for the password information.
 *	Return 0 on match.
 */
static int paircmp(VALUE_PAIR *request, VALUE_PAIR *check)
{
	VALUE_PAIR *check_item = check;
	VALUE_PAIR *auth_item;
	int result = 0;
	char username[AUTH_STRING_LEN];
	int compare;

	username[0] = '\0';
	while (result == 0 && check_item != NULL) {
		switch (check_item->attribute) {
			/*
			 *	Attributes we skip during comparison.
			 *	These are "server" check items.
			 */
			case PW_EXPIRATION:
			case PW_LOGIN_TIME:
			case PW_PASSWORD:
			case PW_CRYPT_PASSWORD:
			case PW_AUTHTYPE:
#ifdef PAM /* cjd 19980706 */
                        case PAM_AUTH_ATTR:
#endif
			case PW_SIMULTANEOUS_USE:
			case PW_STRIP_USERNAME:
				check_item = check_item->next;
				continue;
		}
		/*
		 *	See if this item is present in the request.
		 */
		auth_item = request;
		for (; auth_item != NULL; auth_item = auth_item->next) {
			switch (check_item->attribute) {
				case PW_PREFIX:
				case PW_SUFFIX:
				case PW_GROUP_NAME:
				case PW_GROUP:
					if (auth_item->attribute  !=
					    PW_USER_NAME)
						continue;
					/* Sizes are the same */
					strcpy(username, auth_item->strvalue);
				case PW_HUNTGROUP_NAME:
					break;
				case PW_HINT:
					if (auth_item->attribute !=
					    check_item->attribute)
						continue;
					if (strcmp(check_item->strvalue,
					    auth_item->strvalue) != 0)
						continue;
					break;
				default:
					if (auth_item->attribute !=
					    check_item->attribute)
						continue;
					/*
					 * If it's a tagged attr, and 
					 * the check item doesn't say 'any', 
					 * and the tags don't match, then
					 * consider this a different attribute.
					 */ 
					if (check_item->flags.has_tag &&
					    check_item->flags.tag != TAG_ANY &&
					    check_item->flags.tag != 
					    auth_item->flags.tag) 
						continue;
			}
			break;
		}
		if (auth_item == NULL) {
			result = -1;
			continue;
		}

		/*
		 *	OK it is present now compare them.
		 */
		
		compare = 0;	/* default result */
		switch(check_item->type) {
			case PW_TYPE_STRING:
				if (check_item->attribute == PW_PREFIX ||
				    check_item->attribute == PW_SUFFIX) {
					if (presufcmp(check_item,
					    auth_item->strvalue, username,
					    sizeof(username)) != 0)
						return -1;
				}
				else
				if (check_item->attribute == PW_GROUP_NAME ||
				    check_item->attribute == PW_GROUP) {
					compare = groupcmp(check_item, username);
				}
				else
				if (check_item->attribute == PW_HUNTGROUP_NAME){
					compare = (huntgroup_match(request,
						check_item->strvalue) == 0);
					break;
				}
				else
				compare = strcmp(auth_item->strvalue,
						 check_item->strvalue);
				break;

			case PW_TYPE_INTEGER:
				if (check_item->attribute == PW_NAS_PORT &&
				    check_item->operator == PW_OPERATOR_EQUAL) {
					compare = portcmp(check_item,auth_item);
					break;
				}
				/*FALLTHRU*/
			case PW_TYPE_IPADDR:
				compare = auth_item->lvalue - check_item->lvalue;
				break;

			default:
				return -1;
				break;
		}

		switch (check_item->operator)
		  {
		  default:
		  case PW_OPERATOR_EQUAL:
		    if (compare != 0) return -1;
		    break;

		  case PW_OPERATOR_NOT_EQUAL:
		    if (compare == 0) return -1;
		    break;

		  case PW_OPERATOR_LESS_THAN:
		    if (compare >= 0) return -1;
		    break;

		  case PW_OPERATOR_GREATER_THAN:
		    if (compare <= 0) return -1;
		    break;
		    
		  case PW_OPERATOR_LESS_EQUAL:
		    if (compare > 0) return -1;
		    break;

		  case PW_OPERATOR_GREATER_EQUAL:
		    if (compare < 0) return -1;
		    break;
		  }
		


		if (result == 0)
			check_item = check_item->next;
	}

	return result;

}

/*
 *	Compare two pair lists. At least one of the check pairs
 *	has to be present in the request.
 */
static int hunt_paircmp(VALUE_PAIR *request, VALUE_PAIR *check)
{
	VALUE_PAIR *check_item = check;
	VALUE_PAIR *auth_item;
	int result = -1;

	if (check == NULL) return 0;

	while (result != 0 && check_item != (VALUE_PAIR *)NULL) {
		switch (check_item->attribute) {
			/*
			 *	Attributes we skip during comparison.
			 *	These are "server" check items.
			 */
			case PW_EXPIRATION:
			case PW_PASSWORD:
			case PW_AUTHTYPE:
/* cjd 19980706
 */
#ifdef PAM
                        case PAM_AUTH_ATTR:
#endif
			case PW_SIMULTANEOUS_USE:
				check_item = check_item->next;
				continue;
		}
		/*
		 *	See if this item is present in the request.
		 */
		auth_item = request;
		while(auth_item != (VALUE_PAIR *)NULL) {
			if (check_item->attribute == auth_item->attribute ||
			    ((check_item->attribute == PW_GROUP_NAME ||
			      check_item->attribute == PW_GROUP) &&
			     auth_item->attribute  == PW_USER_NAME))
				break;
			auth_item = auth_item->next;
		}
		if (auth_item == (VALUE_PAIR *)NULL) {
			check_item = check_item->next;
			continue;
		}

		/*
		 *	OK it is present now compare them.
		 */
		
		switch(check_item->type) {

			case PW_TYPE_STRING:
				if (check_item->attribute == PW_GROUP_NAME ||
				    check_item->attribute == PW_GROUP) {
					if (groupcmp(check_item, auth_item->strvalue)==0)
							result = 0;
				}
				else
				if (check_item->attribute == PW_HUNTGROUP_NAME){
					if (huntgroup_match(request,
						check_item->strvalue))
							result = 0;
				}
				else
				if (strcmp(check_item->strvalue,
						auth_item->strvalue) == 0) {
					result = 0;
				}
				break;

			case PW_TYPE_INTEGER:
				if (check_item->attribute == PW_NAS_PORT) {
					if (portcmp(check_item, auth_item) == 0)
						result = 0;
					break;
				}
				/*FALLTHRU*/
			case PW_TYPE_IPADDR:
				if(check_item->lvalue == auth_item->lvalue) {
					result = 0;
				}
				break;

			default:
				break;
		}
		check_item = check_item->next;
	}

	return result;

}


/*
 *	Free a PAIR_LIST
 */
static void pairlist_free(PAIR_LIST **pl)
{
	PAIR_LIST *p, *next;

	for (p = *pl; p; p = next) {
		if (p->name) free(p->name);
		if (p->check) pairfree(p->check);
		if (p->reply) pairfree(p->reply);
		next = p->next;
		free(p);
	}
	*pl = NULL;
}

#ifdef USE_DBX
/*
 *	See if a potential DBM file is present.
 */
static int checkdbm(char *users, int do_open)
{
	char buffer[256];
	int fd = -1;

#if defined (DBIF_DB) || defined(DBIF_NDBM)
	if (fd < 0) {
		sprintf(buffer, "%.250s.db", users);
		fd = open(buffer, O_RDONLY);
	}
#endif
#if defined (DBIF_NDBM) || defined(DBIF_DBM)
	if (fd < 0) {
		sprintf(buffer, "%.250s.pag", users);
		fd = open(buffer, O_RDONLY);
	}
#endif
	if (do_open) return fd;

	if (fd >= 0) close(fd);
	return (fd >= 0) ? 0 : -1;
}

static int rad_dbm_fail(char *buffer)
{
	log(L_ERR|L_CONS, "cannot open dbm file %s", buffer);
	return -1;
}

/*
 *	Called before every dbm file access. We open the file if
 *	it's not opened, or if the link count is down to zero.
 *	if the mtime changed, re-read all the DEFAULT entries.
 */
int rad_dbm_open(void)
{
	static int	dbmfd = -1;
	static time_t	lastmtime;
	static time_t	lastcheck;
	struct stat	st;
	time_t		now;
	char		buffer[256];
	int		need_open = 1;
	int		need_read = 1;

	/*
	 *	Don't check more often than every 2 seconds.
	 */
	now  = time(NULL);
	if (dbmfile != NULL && now < lastcheck + 2)
		return 0;
	lastcheck = now;

	/* Check */
	if (dbmfd >= 0) {
		if (fstat(dbmfd, &st) == 0) {
			if (st.st_nlink > 0)
				need_open = 0;
			if (!need_open && st.st_mtime <= lastmtime)
				need_read = 0;
		}
	}

	/* Open */
	if (need_open) {
		sprintf(buffer, "%.200s/%.40s", radius_dir, RADIUS_USERS);
#ifdef USE_DB
		strcat(buffer, ".db");
#endif
#ifdef DBIF_DBM
		if (dbmfd >= 0) close(dbmfd);
		dbmfd = -1;
		if (dbmfile) dbmclose();
		dbmfile = NULL;
		if (dbminit(buffer) != 0) return rad_dbm_fail(buffer);
		dbmfile = (void *)1;
		dbmfd = checkdbm(buffer, 1);
#endif
#ifdef DBIF_NDBM
		if (dbmfile) {
			dbm_close(dbmfile);
			dbmfd = -1;
		}
		if ((dbmfile = dbm_open(buffer, O_RDONLY, 0)) == NULL)
			return rad_dbm_fail(buffer);
		dbmfd = dbm_pagfno(dbmfile);
#endif
#ifdef DBIF_DB1
		if (dbmfd >= 0) close(dbmfd);
		dbmfd = -1;
		if (dbmfile) dbmfile->close(dbmfile);
		if ((dbmfile = dbopen(buffer, O_RDONLY, 0600,
				      DB_HASH, NULL)) == NULL)
			return rad_dbm_fail(buffer);
		dbmfd = checkdbm(buffer, 1);
#endif
#ifdef DBIF_DB2
		if (dbmfd >= 0) close(dbmfd);
		dbmfd = -1;
		if (dbmfile) dbmfile->close(dbmfile, 0);
		if (db_open(buffer, DB_HASH, DB_RDONLY, 0600, NULL,
				NULL, &dbmfile) < 0) {
			dbmfile = NULL;
			return rad_dbm_fail(buffer);
		}
		dbmfd = open(buffer, O_RDONLY);
#endif
#ifdef DBIF_DB3
		if (dbmfd >= 0) close(dbmfd);
		dbmfd = -1;
		if (dbmfile) {
			dbmfile->close(dbmfile, 0);
			dbmfile = NULL;
		}
		if (db_create(&dbmfile, NULL, 0) < 0) {
			dbmfile = NULL;
			return rad_dbm_fail(buffer);
		}
		if (dbmfile->open(dbmfile, buffer, NULL, DB_HASH,
				DB_RDONLY, 0600) < 0) {
			dbmfile->close(dbmfile, 0);
			dbmfile = NULL;
			return rad_dbm_fail(buffer);
		}
		dbmfd = open(buffer, O_RDONLY);
#endif
		if (dbmfd >= 0 && fstat(dbmfd, &st) == 0)
			lastmtime = st.st_mtime;
		else
			lastmtime = 0;
	}

	/* Read DEFAULT entries */
	if (need_read) {
		pairlist_free(&defaults);
		defaults = rad_dbm_lookup("DEFAULT");
	}

	return 0;
}

/*
 *	Lookup a name in the database. If num > 0, then lookup
 *	name\nnum instead. This is to support multiple duplicate
 *	ordered keys.
 */
PAIR_LIST *rad_dbm_lookup1(char *name, int num)
{
	VALUE_PAIR	*check_tmp;
	VALUE_PAIR	*reply_tmp;
	PAIR_LIST	*pl;
	datum		key;
	datum		value;
	char		*ptr;
	char		*data;
	int		len;
	int		recno = 0;

	if (dbmfile == NULL) return NULL;

	memset(&key, 0, sizeof(key));
	memset(&value, 0, sizeof(value));

	/*
	 *	Change name into name\nnum if num > 0
	 */
	len = strlen(name);
	if ((key.dptr = malloc(len + 10)) == NULL) {
		log(L_CONS|L_ERR, "no memory");
		exit(1);
	}
	strcpy(key.dptr, name);
	key.dsize = len;
	if (num > 0) {
		sprintf(key.dptr + len, "\n%d", num);
		key.dsize += strlen(key.dptr + len);
	}
#ifdef DBIF_DBM
	value = fetch(key);
#endif
#ifdef DBIF_NDBM
	value = dbm_fetch(dbmfile, key);
#endif
#ifdef DBIF_DB1
	dbmfile->get(dbmfile, &key, &value, 0);
#endif
#if defined (DBIF_DB2) || defined(DBIF_DB3)
	dbmfile->get(dbmfile, NULL, &key, &value, 0);
#endif
	free(key.dptr);
	if (value.dptr == NULL)
		return NULL;

	check_tmp = NULL;
	reply_tmp = NULL;

	/*
	 *	Parse sequence number.
	 */
	if ((data = malloc(value.dsize + 1)) == NULL) {
		log(L_CONS|L_ERR, "no memory");
		exit(1);
	}
	memcpy(data, value.dptr, value.dsize);
	ptr = data;
	ptr[value.dsize] = '\0';

	while(*ptr != '\n' && *ptr != '\0')
		ptr++;
	if (*ptr == '\n') ptr++;
	if ((recno = atoi(value.dptr)) == 0) {
		log(L_ERR|L_CONS, "Database error for user %s", name);
		free(data);
		return NULL;
	}

	/*
	 *	Parse the check values
	 */
	if (*ptr != '\n' && userparse(ptr, &check_tmp, 1) != 0) {
		log(L_ERR|L_CONS, "Parse error (check) for user %s", name);
		pairfree(check_tmp);
		free(data);
		return NULL;
	}
	while(*ptr != '\n' && *ptr != '\0') {
		ptr++;
	}
	if(*ptr != '\n') {
		log(L_ERR|L_CONS, "Parse error (no reply pairs) for user %s",
			name);
		pairfree(check_tmp);
		free(data);
		return NULL;
	}
	ptr++;

	/*
	 *	Parse the reply values
	 */
	if (userparse(ptr, &reply_tmp, 1) != 0) {
		log(L_ERR|L_CONS, "Parse error (reply) for user %s", name);
		pairfree(check_tmp);
		pairfree(reply_tmp);
		free(data);
		return NULL;
	}
	free(data);

	/*
	 *	Fill the PAIR_LIST.
	 */
	if ((pl = malloc(sizeof(PAIR_LIST))) == NULL) {
		log(L_CONS|L_ERR, "no memory");
		exit(1);
	}
	if ((pl->name = strdup(name)) == NULL) {
		log(L_CONS|L_ERR, "no memory");
		exit(1);
	}
	pl->check = check_tmp;
	pl->reply = reply_tmp;
	pl->lineno = -1;
	pl->recno = recno;
	pl->next = NULL;

	return pl;
}

/*
 *	Lookup a key in the database. If it has multiple values,
 *	return them all in the original order.
 */
PAIR_LIST *rad_dbm_lookup(char *name)
{
	PAIR_LIST	*pl;
	PAIR_LIST	*first, *last;
	int		n;

	first = last = NULL;
	n = 0;

	while ((pl = rad_dbm_lookup1(name, n)) != NULL) {
		if (first == NULL)
			first = pl;
		else
			last->next = pl;
		last = pl;
		n++;
	}

	return first;
}
#endif /* USE_DBX */

/*
 *	See if a VALUE_PAIR list contains Fall-Through = Yes
 */
static int fallthrough(VALUE_PAIR *vp)
{
	VALUE_PAIR *tmp;

	tmp = pairfind(vp, PW_FALL_THROUGH);

	return tmp ? tmp->lvalue : 0;
}


#ifdef ASCEND_PORT_HACK

#if defined(CISCO_PORT_HACK)
#  error "ASCEND_PORT_HACK and CISCO_PORT_HACK are mutually exclusive"
#endif

/*
 *	dgreer --
 *	This hack changes Ascend's wierd port numberings
 *      to standard 0-??? port numbers so that the "+" works
 *      for IP address assignments.
 */
static int ascend_port_number(int nas_port)
{
	int service;
	int line;
	int channel;

	if (nas_port > 9999) {
		service = nas_port/10000; /* 1=digital 2=analog */
		line = (nas_port - (10000 * service)) / 100;
		channel = nas_port-((10000 * service)+(100 * line));
		nas_port =
			(channel - 1) + (line - 1) * ASCEND_CHANNELS_PER_LINE;
	}
	return nas_port;
}
#endif

#ifdef CISCO_PORT_HACK

#if defined(ASCEND_PORT_HACK)
#  error "ASCEND_PORT_HACK and CISCO_PORT_HACK are mutually exclusive"
#endif

/*
 *     hazard@hazard.maks.net:
 *     Cisco is not as weird as Ascend :-), but still,
 *     it sends port numbers using formula 20000+real_port_id if the 
 *     call is ISDN.
 *     This hack subtracts it if needed so that "+" works for IP addresses.
 */
 
static int cisco_port_number(int nas_port)
{
       if (nas_port > 20000) {
	       nas_port -= 20000;
       }
       return nas_port;
}
#endif

/*
 *	Find the named user in the database.  Create the
 *	set of attribute-value pairs to check and reply with
 *	for this user from the database. The main code only
 *	needs to check the password, the rest is done here.
 */
int user_find(char *name, VALUE_PAIR *request_pairs,
		VALUE_PAIR **check_pairs, VALUE_PAIR **reply_pairs)
{
	int		nas_port = 0;
	VALUE_PAIR	*check_tmp;
	VALUE_PAIR	*reply_tmp;
	VALUE_PAIR	*tmp;
	PAIR_LIST	*pl;
	PAIR_LIST	*userlist = users;
	PAIR_LIST	*userlist_ptr, *defaults_ptr;
	int		found = 0;
	int		group = 1;

	/* 
	 *	Check for valid input, zero length names not permitted 
	 */
	if (name[0] == 0) {
		log(L_ERR, "zero length username not permitted\n");
		return -1;
	}
	if (!strcmp(name, "DEFAULT")) {
		log(L_ERR, "username DEFAULT not permitted\n");
		return -1;
	}

	/*
	 *	Find the NAS port ID.
	 */
	if ((tmp = pairfind(request_pairs, PW_NAS_PORT)) != NULL)
		nas_port = tmp->lvalue;

	/*
	 *	Find the entry for the user.
	 */
#ifdef USE_DBX
	/*
	 *	If hashing for the in-memory userlist gets
	 *	implemented we'll have to do something similar
	 */	
	if (use_dbm) {
		rad_dbm_open();
		userlist = rad_dbm_lookup(name);
	}
#endif

	/*
	 *	We have two lists. The "userlist" contains entries
	 *	that matched "name". The "defaults" contains entries
	 *	that matched "DEFAULT". We'll have to walk over both
	 *	of them, using the order in "recno".
	 */
	userlist_ptr = userlist;
	defaults_ptr = defaults;

	while (userlist_ptr || defaults_ptr) {

		pl = NULL;
		if (userlist_ptr && (defaults_ptr == NULL ||
		    userlist_ptr->recno < defaults_ptr->recno)) {
			pl = userlist_ptr;
			userlist_ptr = userlist_ptr->next;
		} else if (defaults_ptr && (userlist_ptr == NULL ||
		    defaults_ptr->recno < userlist_ptr->recno)) {
			pl = defaults_ptr;
			defaults_ptr = defaults_ptr->next;
		}

		if (strcmp(name, pl->name) && strcmp(pl->name, "DEFAULT"))
			continue;

		if (paircmp(request_pairs, pl->check) == 0) {
			if (pl->lineno == 0) {
				DEBUG2("  users: Matched %s at recno %d",
					pl->name, pl->recno);
			} else {
				DEBUG2("  users: Matched %s at line %d",
					pl->name, pl->lineno);
			}
			found = 1;
			check_tmp = paircopy(pl->check);
			reply_tmp = paircopy(pl->reply);
			checkpair_move(check_pairs, &check_tmp);
			replypair_move(reply_pairs, &reply_tmp, group++);
			pairfree(check_tmp);
			pairfree(reply_tmp);

			/*
			 *	Fallthrough?
			 */
			if (!fallthrough(pl->reply))
				break;
		}
	}

#ifdef USE_DBX
	if (use_dbm && userlist) pairlist_free(&userlist);
#endif

	/*
	 *	See if we succeeded.
	 *	If not, print out a debugging error message,
	 *	and return an error.
	 */
	if (!found) {
		DEBUG("NO SUCH USER FOUND: \"%s\"", name);
		return -1;
	}

	/*
	 *	Fix dynamic IP address if needed.
	 */
	if ((tmp = pairfind(*reply_pairs, PW_FRAMED_IP_ADDRESS)) != NULL &&
	     tmp->flags.addport) {
		/*
	 	 *	FIXME: This only works because IP
		 *	numbers are stored in host order
		 *	everywhere in this program.
		 */
#ifdef ASCEND_PORT_HACK
		nas_port = ascend_port_number(nas_port);
#endif
#ifdef CISCO_PORT_HACK
		nas_port = cisco_port_number(nas_port);
#endif
		tmp->lvalue += nas_port;
	}

	/*
	 *	Remove server internal parameters.
	 */
	pairdelete(reply_pairs, PW_FALL_THROUGH);

	return 0;
}

/*
 *	Match a username with a wildcard expression.
 *	Is very limited for now.
 */
static int matches(char *name, PAIR_LIST *pl, char *matchpart, int matchlen)
{
	int len, wlen;
	int ret = 0;
	char *wild = pl->name;
	VALUE_PAIR *tmp;

	/*
	 *	We now support both:
	 *
	 *		DEFAULT	Prefix = "P"
	 *
	 *	and
	 *		P*
	 */
	if ((tmp = pairfind(pl->check, PW_PREFIX)) != NULL ||
	    (tmp = pairfind(pl->check, PW_SUFFIX)) != NULL) {

		if (strncmp(pl->name, "DEFAULT", 7) == 0 ||
		    strcmp(pl->name, name) == 0)
			return !presufcmp(tmp, name, matchpart, matchlen);
	}

	/*
	 *	Shortcut if there's no '*' in pl->name.
	 */
	if (strchr(pl->name, '*') == NULL &&
	    (strncmp(pl->name, "DEFAULT", 7) == 0 ||
	     strcmp(pl->name, name) == 0)) {
		strNcpy(matchpart, name, matchlen);
		return 1;
	}

	/*
	 *	Normally, we should return 0 here, but we
	 *	support the old * stuff.
	 *	FIXME: this doesn't support realms yet, while
	 *	presufcmp does!
	 */
	len = strlen(name);
	wlen = strlen(wild);

	if (len == 0 || wlen == 0) return 0;

	if (wild[0] == '*') {
		wild++;
		wlen--;
		if (wlen <= len && strcmp(name + (len - wlen), wild) == 0) {
			strNcpy(matchpart, name, matchlen);
			matchpart[len - wlen] = 0;
			ret = 1;
		}
	} else if (wild[wlen - 1] == '*') {
		if (wlen <= len && strncmp(name, wild, wlen - 1) == 0) {
			strNcpy(matchpart, name + wlen - 1, matchlen);
			ret = 1;
		}
	}

	return ret;
}




/*
 *	Add hints to the info sent by the terminal server
 *	based on the pattern of the username.
 */
int hints_setup(VALUE_PAIR *request_pairs)
{
	char		newname[AUTH_STRING_LEN];
	char		*name;
	VALUE_PAIR	*add;
	VALUE_PAIR	*last;
	VALUE_PAIR	*tmp;
	PAIR_LIST	*i;
	int		do_strip;
#if defined(NT_DOMAIN_HACK) || defined(SPECIALIX_JETSTREAM_HACK)
	char		*ptr;
#endif

	if (hints == NULL)
		return 0;

	/* 
	 *	Check for valid input, zero length names not permitted 
	 */
	if ((tmp = pairfind(request_pairs, PW_USER_NAME)) == NULL)
		name = NULL;
	else
		name = tmp->strvalue;

	if (name == NULL || name[0] == 0)
		/*
		 *	Will be complained about later.
		 */
		return 0;

#ifdef NT_DOMAIN_HACK
	/*
	 *	Windows NT machines often authenticate themselves as
	 *	NT_DOMAIN\username. Try to be smart about this.
	 *	Note that sizeof(newname) == sizeof(name)
	 *
	 *	FIXME: should we handle this as a REALM ?
	 */
	if ((ptr = strchr(name, '\\')) != NULL) {
		strncpy(newname, ptr + 1, sizeof(newname));
		newname[sizeof(newname) - 1] = 0;
		strcpy(name, newname);
	}
#endif /* NT_DOMAIN_HACK */

#ifdef SPECIALIX_JETSTREAM_HACK
	/*
	 *	Specialix Jetstream 8500 24 port access server.
	 *	If the user name is 10 characters or longer, a "/"
	 *	and the excess characters after the 10th are
	 *	appended to the user name.
	 *
	 *	Reported by Lucas Heise <root@laonet.net>
	 */
	if (strlen(name) > 10 && name[10] == '/') {
		for (ptr = name + 11; *ptr; ptr++)
			*(ptr - 1) = *ptr;
		*(ptr - 1) = 0;
	}
#endif

	/*
	 *	Small check: if Framed-Protocol present but Service-Type
	 *	is missing, add Service-Type = Framed-User.
	 */
	if (pairfind(request_pairs, PW_FRAMED_PROTOCOL) != NULL &&
	    pairfind(request_pairs, PW_SERVICE_TYPE) == NULL) {
		if ((tmp = malloc(sizeof(VALUE_PAIR))) != NULL) {
			memset(tmp, 0, sizeof(VALUE_PAIR));
			strcpy(tmp->name, "Service-Type");
			tmp->attribute = PW_SERVICE_TYPE;
			tmp->type = PW_TYPE_INTEGER;
			tmp->lvalue = PW_FRAMED_USER;
			pairmove2(&request_pairs, &tmp, PW_SERVICE_TYPE);
		}
	}

	for (i = hints; i; i = i->next) {
		if (matches(name, i, newname, sizeof(newname))) {
			DEBUG2("  hints: Matched %s at %d",
			       i->name, i->lineno);
			break;
		}
	}

	if (i == NULL) return 0;

	add = paircopy(i->reply);

#if 0 /* DEBUG */
	printf("In hints_setup, newname is %s\n", newname);
#endif

	/*
	 *	See if we need to adjust the name.
	 */
	do_strip = 1;
	if ((tmp = pairfind(i->reply, PW_STRIP_USERNAME)) != NULL
	     && tmp->lvalue == 0)
		do_strip = 0;
	if ((tmp = pairfind(i->check, PW_STRIP_USERNAME)) != NULL
	     && tmp->lvalue == 0)
		do_strip = 0;

	if (do_strip) {
		tmp = pairfind(request_pairs, PW_USER_NAME);
		if (tmp) {
			/* Same size */
			strcpy(tmp->strvalue, newname);
			tmp->length = strlen(tmp->strvalue);
		}
	}

	/*
	 *	Now add all attributes to the request list,
	 *	except the PW_STRIP_USERNAME one.
	 */
	pairdelete(&add, PW_STRIP_USERNAME);
	for(last = request_pairs; last && last->next; last = last->next)
		;
	if (last) last->next = add;

	return 0;
}

/*
 *	See if the huntgroup matches.
 */
static int huntgroup_match(VALUE_PAIR *request_pairs, char *huntgroup)
{
	PAIR_LIST	*i;

	for (i = huntgroups; i; i = i->next) {
		if (strcmp(i->name, huntgroup) != 0)
			continue;
		if (paircmp(request_pairs, i->check) == 0) {
			DEBUG2("  huntgroups: Matched %s at %d",
			       i->name, i->lineno);
			break;
		}
	}

	return (i != NULL);
}


/*
 *	See if we have access to the huntgroup.
 *	Returns  0 if we don't have access.
 *	         1 if we do have access.
 *	        -1 on error.
 */
int huntgroup_access(VALUE_PAIR *request_pairs)
{
	PAIR_LIST	*i;
	int		r = 1;

	if (huntgroups == NULL)
		return 1;

	for(i = huntgroups; i; i = i->next) {
		/*
		 *	See if this entry matches.
		 */
		if (paircmp(request_pairs, i->check) != 0)
			continue;

		r = 0;
		if (hunt_paircmp(request_pairs, i->reply) == 0) {
			r = 1;
		}
		break;
	}

	return r;
}


/*
 *	Debug code.
 */
#if 0
static void debug_pair_list(PAIR_LIST *pl)
{
	VALUE_PAIR *vp;

	while(pl) {
		printf("Pair list: %s\n", pl->name);
		printf("** Check:\n");
		for(vp = pl->check; vp; vp = vp->next) {
			printf("    ");
			fprint_attr_val(stdout, vp);
			printf("\n");
		}
		printf("** Reply:\n");
		for(vp = pl->reply; vp; vp = vp->next) {
			printf("    ");
			fprint_attr_val(stdout, vp);
			printf("\n");
		}
		pl = pl->next;
	}
}
#endif

/*
 *	Free a RADCLIENT list.
 */
static void clients_free(RADCLIENT *cl)
{
	RADCLIENT *next;

	while(cl) {
		next = cl->next;
		free(cl);
		cl = next;
	}
}


/*
 *	Read the clients file.
 */
int read_clients_file(char *file)
{
	FILE	*fp;
	char	buffer[256];
	char	hostnm[128];
	char	secret[32];
	char	shortnm[32];
	int	lineno = 0;
	RADCLIENT	*c;

	clients_free(clients);
	clients = NULL;

	if ((fp = fopen(file, "r")) == NULL) {
		log(L_CONS|L_ERR, "cannot open %s: %s", file, strerror(errno));
		return -1;
	}
	while(fgets(buffer, 256, fp) != NULL) {
		lineno++;
		if (buffer[0] == '#' || buffer[0] == '\n' || buffer[0] == '\r')
			continue;

		shortnm[0] = 0;
		if (sscanf(buffer, "%127s%31s%31s", hostnm, secret, shortnm) < 2) {
			log(L_ERR, "%s[%d]: syntax error", file, lineno);
			continue;
		}
		if (shortnm[0] == '#') shortnm[0] = 0;

		if ((c = malloc(sizeof(RADCLIENT))) == NULL) {
			log(L_CONS|L_ERR, "%s[%d]: out of memory",
				file, lineno);
			return -1;
		}

		c->ipaddr = get_ipaddr(hostnm);
		strNcpy(c->secret, secret, sizeof(c->secret));
		strNcpy(c->shortname, shortnm, sizeof(c->shortname));
		strNcpy(c->longname, ip_hostname(c->ipaddr),
			sizeof(c->longname));

		c->next = clients;
		clients = c;
	}
	fclose(fp);

	return 0;
}


/*
 *	Find a client in the RADCLIENTS list.
 */
RADCLIENT *client_find(UINT4 ipaddr)
{
	RADCLIENT *cl;

	for(cl = clients; cl; cl = cl->next)
		if (ipaddr == cl->ipaddr)
			break;

	return cl;
}


/*
 *	Find the name of a client (prefer short name).
 */
char *client_name(UINT4 ipaddr)
{
	RADCLIENT *cl;

	if ((cl = client_find(ipaddr)) != NULL) {
		if (cl->shortname[0])
			return cl->shortname;
		else
			return cl->longname;
	}
	return ip_hostname(ipaddr);
}

/*
 *	Free a NAS list.
 */
static void nas_free(NAS *cl)
{
	NAS *next;

	while(cl) {
		next = cl->next;
		free(cl);
		cl = next;
	}
}

/*
 *	Free a REALM list.
 */
static void realm_free(REALM *cl)
{
	REALM *next;

	while(cl) {
		next = cl->next;
		free(cl);
		cl = next;
	}
}

/*
 *	Read the nas file.
 */
int read_naslist_file(char *file)
{
	FILE	*fp;
	char	buffer[256];
	char	hostnm[128];
	char	shortnm[32];
	char	nastype[32];
	int	lineno = 0;
	int	dotted;
	NAS	*c;

	nas_free(naslist);
	naslist = NULL;

	if ((fp = fopen(file, "r")) == NULL) {
		log(L_CONS|L_ERR, "cannot open %s: %s", file, strerror(errno));
		return -1;
	}
	while(fgets(buffer, 256, fp) != NULL) {
		lineno++;
		if (buffer[0] == '#' || buffer[0] == '\n' || buffer[0] == '\r')
			continue;
		nastype[0] = 0;
		if (sscanf(buffer, "%127s%31s%31s", hostnm, shortnm, nastype) < 2) {
			log(L_ERR, "%s[%d]: syntax error", file, lineno);
			return -1;
		}
		if ((c = malloc(sizeof(NAS))) == NULL) {
			log(L_CONS|L_ERR, "%s[%d]: out of memory",
				file, lineno);
			return -1;
		}
		c->ipaddr = 0;

		dotted = 0;
		if (strcmp(hostnm, "DEFAULT") != 0) {
			c->ipaddr = get_ipaddr(hostnm);
			dotted = good_ipaddr(hostnm);
		}
		strNcpy(c->nastype, nastype, sizeof(c->nastype));
		strNcpy(c->shortname, shortnm, sizeof(c->shortname));
		strNcpy(c->longname, dotted ? ip_hostname(c->ipaddr) : hostnm,
			sizeof(c->longname));

		c->next = naslist;
		naslist = c;
	}
	fclose(fp);

	return 0;
}

/*
 *	Read the realms file.
 */
int read_realms_file(char *file)
{
	FILE	*fp;
	char	buffer[256];
	char	realm[32];
	char	hostnm[128];
	char	opts[128];
	char	optsx[5][16];
	char	*p;
	int	i;
	int	lineno = 0;
	REALM	*c;

	realm_free(realms);
	realms = NULL;

	if ((fp = fopen(file, "r")) == NULL) {
		log(L_CONS|L_ERR, "cannot open %s: %s", file, strerror(errno));
		return -1;
	}
	while(fgets(buffer, 256, fp) != NULL) {

		lineno++;
		if (buffer[0] == '#' || buffer[0] == '\n' || buffer[0] == '\r')
			continue;

		/*
		 *	We use sscanf, yet we want to allow a list
		 *	of space-separated options at the end. And
		 *	that's how you end up with a hack like this.
		 *
		 *	Ofcourse options should be comma-separated.
		 */
		for (i = 0; i < 5; i++) optsx[i][0] = 0;
		if (sscanf(buffer, "%31s%127s%15s%15s%15s%15s%15s",
		    realm, hostnm, optsx[0], optsx[1],
		    	optsx[2], optsx[3], optsx[4]) < 2) {
			log(L_ERR, "%s[%d]: syntax error", file, lineno);
			return -1;
		}
		opts[0] = 0;
		for (i = 0; i < 5; i++) {
			if (optsx[i][0] == 0) break;
			if (opts[0]) strcat(opts, ",");
			strcat(opts, optsx[i]);
		}

		/* Initialize new REALM struct */
		if ((c = malloc(sizeof(REALM))) == NULL) {
			log(L_CONS|L_ERR, "%s[%d]: out of memory",
				file, lineno);
			return -1;
		}
		memset(c, 0, sizeof(REALM));

		/* Find the remote servers IP address and ports */
		if ((p = strchr(hostnm, ':')) != NULL) {
			*p++ = 0;
			c->auth_port = atoi(p);
			c->acct_port = c->auth_port + 1;
		} else {
			c->auth_port = auth_port;
			c->acct_port = acct_port;
		}
		if (strcmp(hostnm, "LOCAL") != 0)
			c->ipaddr = get_ipaddr(hostnm);
		strNcpy(c->realm, realm, sizeof(c->realm));
		strNcpy(c->server, hostnm, sizeof(c->server));

		/* Process the options. */
		p = strtok(opts, ", \t");
		while (p) {
			if (strcmp(p, "nostrip") == 0)
				c->opts.nostrip = 1;
			if (strcmp(p, "hints") == 0)
				c->opts.dohints = 1;
			if (strcmp(p, "noacct") == 0)
				c->opts.noacct = 1;
			if (strcmp(p, "noauth") == 0)
				c->opts.noauth = 1;
			if (strcmp(p, "trusted") == 0)
				c->opts.trusted = 1;
			p = strtok(NULL, ", \t");
		}

		c->next = realms;
		realms = c;
	}
	fclose(fp);

	return 0;
}

/*
 *	Find a realm in the REALM list. If the realm can't be found, use
 *	the DEFAULT realm, *unless* the realm was the NULL realm.
 */
REALM *realm_find(char *r, int dfl)
{
	REALM	*cl;

	/* Sanity check */
	if (r == NULL || r[0] == 0) return NULL;

	for(cl = realms; cl; cl = cl->next)
		if (strcmp(cl->realm, r) == 0)
			break;
	if (cl || !dfl || !strcmp(r, "NULL")) return cl;

	for(cl = realms; cl; cl = cl->next)
		if (strcmp(cl->realm, "DEFAULT") == 0)
			break;
	return cl;
}



/*
 *	Find a nas in the NAS list.
 */
NAS *nas_find(UINT4 ipaddr)
{
	NAS *cl, *dfl;

	dfl = NULL;
	for(cl = naslist; cl; cl = cl->next) {
		if (ipaddr == cl->ipaddr)
			break;
		if (strcmp(cl->longname, "DEFAULT") == 0)
			dfl = cl;
	}

	return cl ? cl : dfl;
}


/*
 *	Find the name of a nas (prefer short name).
 */
char *nas_name(UINT4 ipaddr)
{
	NAS *cl;

	if ((cl = nas_find(ipaddr)) != NULL &&
	    strcmp(cl->longname, "DEFAULT") != 0) {
		if (cl->shortname[0])
			return cl->shortname;
		else
			return cl->longname;
	}
	return ip_hostname(ipaddr);
}

/*
 *	Find the name of a nas (prefer short name) based on the request.
 *	FIXME: the implementation in acct.c:radius_xlate2() is better.
 */
char *nas_name2(AUTH_REQ *authreq)
{
	UINT4	ipaddr;
	NAS	*cl;
	VALUE_PAIR	*pair;

	if ((pair = pairfind(authreq->request, PW_NAS_IP_ADDRESS)) != NULL)
		ipaddr = pair->lvalue;
	else
		ipaddr = authreq->ipaddr;

	if ((cl = nas_find(ipaddr)) != NULL &&
	    strcmp(cl->longname, "DEFAULT") != 0) {
		if (cl->shortname[0])
			return cl->shortname;
		else
			return cl->longname;
	}
	return ip_hostname(ipaddr);
}

/*
 *	Return a short string showing the terminal server, port
 *	and calling station ID.
 */
char *auth_name(AUTH_REQ *authreq, int do_cli)
{
	static char	buf[300];
	VALUE_PAIR	*cli;
	VALUE_PAIR	*pair;
	int		port = 0;

	if ((cli = pairfind(authreq->request, PW_CALLING_STATION_ID)) == NULL)
		do_cli = 0;
	if ((pair = pairfind(authreq->request, PW_NAS_PORT)) != NULL)
		port = pair->lvalue;

	sprintf(buf, "from nas %.128s/S%u%s%.128s", nas_name2(authreq), port,
		do_cli ? " cli " : "", do_cli ? cli->strvalue : "");

	return buf;
}


/*
 *	(Re-) read the configuration files.
 */
int read_config_files()
{
	struct stat	st;
	char		buffer[256];

	pairlist_free(&users);
	pairlist_free(&huntgroups);
	pairlist_free(&hints);

	sprintf(buffer, "%.200s/%.50s", radius_dir, RADIUS_USERS);
#if USE_DBX
	if (!use_dbm && checkdbm(buffer, 0) >= 0) {
		log(L_INFO|L_CONS, "DBM files found but no -b flag given - NOT using DBM");
		use_dbm = 0;
	}
#endif
	if (!use_dbm && file_read(buffer, radius_dir, &users, 1) < 0)
		return -1;

	sprintf(buffer, "%.200s/%.50s", radius_dir, RADIUS_HUNTGROUPS);
	if (stat(buffer, &st) == 0 || errno != ENOENT) {
		if (file_read(buffer, radius_dir, &huntgroups, 1) < 0)
			return -1;
	}
	sprintf(buffer, "%.200s/%.50s", radius_dir, RADIUS_HINTS);
	if (stat(buffer, &st) == 0 || errno != ENOENT) {
		if (file_read(buffer, radius_dir, &hints, 1) < 0)
			return -1;
	}

	sprintf(buffer, "%.200s/%.50s", radius_dir, RADIUS_CLIENTS);
	if (read_clients_file(buffer) < 0)
		return -1;
	sprintf(buffer, "%.200s/%.50s", radius_dir, RADIUS_NASLIST);
	if (stat(buffer, &st) == 0 || errno != ENOENT) {
		if (read_naslist_file(buffer) < 0)
			return -1;
	}
	sprintf(buffer, "%.200s/%.50s", radius_dir, RADIUS_REALMS);
	if (stat(buffer, &st) == 0 || errno != ENOENT) {
		if (read_realms_file(buffer) < 0)
			return -1;
	}

	if (cache_passwd) {
		log(L_INFO, "HASH:  Reinitializing hash structures "
			"and lists for caching...");
		if(buildHashTable() < 0) {
			log(L_ERR, "HASH:  unable to create user "
				"hash table.  disable caching and run debugs");
			return -1;
		}
		if (buildGrpList() < 0) {
			log(L_ERR, "HASH:  unable to cache groups file.  "
				"disable caching and run debugs");
			return -1;
		}
	}

	return 0;
}
