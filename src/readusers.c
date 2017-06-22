/*
 *
 * readusers.c	Routines to read a users-file.
 *
 *		Copyright 1997-2001	Cistron Internet Services B.V.
 */

char readusers_rcsid[] =
"$Id: readusers.c,v 1.6 2001/11/10 12:41:39 miquels Exp $";

#include	<sys/types.h>
#include	<sys/time.h>

#include	<stdio.h>
#include	<stdlib.h>
#include	<ctype.h>
#include	<errno.h>

#include	"radiusd.h"

#define SANITY_CHECK 0 /* Doesn't work on hints file .. */

#if SANITY_CHECK
static void pair_sanity(char *fn, PAIR_LIST *entry)
{
	VALUE_PAIR	*pair;
	int		is_check;

	for (pair = entry->reply; pair; pair = pair->next) {
		is_check = ((pair->attribute == PW_PASSWORD) ||
			   (pair->attribute == PW_CHAP_PASSWORD) ||
			   (pair->attribute >= 1000 &&
			    pair->attribute <= 65536));
		if (!is_check) continue;
		log(L_ERR|L_CONS, "[%s:%d] WARNING: Check item \"%s\"\n"
		    "\tfound in reply item list for user \"%s\".\n"
		    "\tThis attribute MUST go on the first line"
		    " with the other check items.\n", fn, entry->lineno,
		    pair->name, entry->name);
	}
}
#endif

/*
 *	Fixup a check line.
 *	If Password or Crypt-Password is set, but there is no
 *	Auth-Type, add one (kludge!).
 */
void auth_type_fixup(VALUE_PAIR *check)
{
	VALUE_PAIR	*vp;
	VALUE_PAIR	*c = NULL;
	int		n = 0;
	int		inplace = 0;
	char		*s = "";

	/*
	 *	See if a password is present. Return right away
	 *	if we see Auth-Type.
	 */
	for (vp = check; vp; vp = vp->next) {
		if (vp->attribute == PW_AUTHTYPE)
			return;
		/*
		 *	If Password = "UNIX" replace it with
		 *	Auth-Type = System. Likewise for PAM.
		 *	Otherwise add Auth-Type = Local.
		 */
		if (vp->attribute == PW_PASSWORD) {
			c = vp;

			if (strcmp(vp->strvalue, "UNIX") == 0) {
				inplace = 1;
				n = PW_AUTHTYPE_SYSTEM;
				s = "System";
			} else 
#ifdef PAM
			if (strcmp(vp->strvalue, "PAM") == 0) {
				inplace = 1;
				n = PW_AUTHTYPE_PAM;
				s = "Pam";
			} else 
#endif
			if (strcmp(vp->strvalue, "MYSQL") == 0) {
				inplace = 1;
				n = PW_AUTHTYPE_MYSQL;
				s = "Mysql";
			} else {
				inplace = 0;
				n = PW_AUTHTYPE_LOCAL;
				s = "Local";
			}
		}

		if (vp->attribute == PW_CRYPT_PASSWORD) {
			c = vp;
			n = PW_AUTHTYPE_CRYPT;
			s = "Crypt-Local";
			inplace = 0;
		}
	}

	if (c == NULL)
		return;

	/*
	 *	Add an Auth-Type attribute.
	 *	FIXME: put Auth-Type _first_ (doesn't matter now,
	 *	might matter some day).
	 *	
	 */
	if (!inplace) {
		if ((vp = malloc(sizeof(VALUE_PAIR))) == NULL) {
			log(L_CONS|L_ERR, "no memory");
			exit(1);
		}
		memset(vp, 0, sizeof(VALUE_PAIR));
	} else
		vp = c;

	strcpy(vp->name, "Auth-Type");
	strcpy(vp->strvalue, s);
	vp->attribute = PW_AUTHTYPE;
	vp->type = PW_TYPE_INTEGER;
	vp->lvalue = n;

	if (!inplace) {
		vp->next = c->next;
		c->next = vp;
	}
}

/*
 *	Copy username from the beginning of the buffer and return
 *	a pointer to the first non-space charachter after that.
 *
 *	We allow "" and \ to include spaces in the username.
 */
static char *getusername(char *buffer, char *entry)
{
	char *ptr;
	char *to;
	int spc_seen = 0;

	ptr = buffer;
	to = entry;

	while (*ptr && !spc_seen) {
		switch (*ptr) {
			case '"':
				ptr++;
				while (*ptr && *ptr != '"')
					*to++ = *ptr++;
				if (*ptr) ptr++;
				break;
			case '\\':
				ptr++;
				*to++ = *ptr;
				if (*ptr) ptr++;
				break;
			case ' ':
			case '\t':
			case '\r':
			case '\n':
				spc_seen = 1;
				break;
			default:
				*to++ = *ptr++;
				break;
		}
	}
	*to = 0;

	while (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')
		ptr++;

	return ptr;
}

/*
 *	Read one complete entry from the users file.
 *
 */
int read_entry(char *buf, FILE *fp, char *fn, int *li, char *entry,
	VALUE_PAIR **check, VALUE_PAIR **reply, int resolve)
{
	char		*ptr;
	char		buffer[256];
	int		first = 1;
	int		parsecode;
	int		state = 0;
	int		c;

	*check = NULL;
	*reply = NULL;
	first = (buf[0] != 0);

	while (1) {
		if (first) {
			strNcpy(buffer, buf, sizeof(buffer));
			first = 0;
		} else {
			if (fgets(buffer, sizeof(buffer), fp) == NULL)
				break;
			(*li)++;
		}
		if (buffer[0] == '#') continue;
		if (state == 0) {
			if (buffer[0] == '\n') continue;
			if (buffer[0] == ' ' || buffer[0] == '\t') {
				log(L_ERR|L_CONS,
					"%s[%d]: Expected username", fn, *li);
				return -1;
			}
			ptr = getusername(buffer, entry);
			parsecode = userparse(ptr, check, resolve);
			if (parsecode < 0) {
				log(L_ERR|L_CONS,
				"%s[%d]: Parse error (check) for entry %s",
					fn, *li, entry);
				return -1;
			} else if (parsecode == USERPARSE_COMMA) {
				log(L_ERR|L_CONS,
					"%s[%d]: Unexpected trailing comma in "
					"check item list for entry %s",
					fn, *li, entry);
				return -1;
			}
			/*
			 *	Peek at the first character of the next line
			 */
			c = fgetc(fp);
			ungetc(c, fp);
			if (!strchr(" \t#", c))
				break;
			state = 1;
		} else if (state == 1) {
			if (buffer[0] != ' ' && buffer[0] != '\t') {
				log(L_ERR|L_CONS,
					"%s[%d]: Expected reply-item "
					"(trailing comma on previous line?)",
					fn, *li);
				return -1;
			}
			parsecode = userparse(buffer, reply, resolve);
			if (parsecode < 0) {
				log(L_ERR|L_CONS,
				"%s[%d]: Parse error (reply) for entry %s",
					fn, *li, entry);
				return -1;
			}
			/* No trailing comma? Last line. */
			if (parsecode == USERPARSE_EOS)
				break;
		}
	}

	buffer[0] = 0;

	if (state == 0)
		return 0;

	if (state == 1 && parsecode == USERPARSE_EOS)
		return 0;

	log(L_ERR|L_CONS, "%s[%d]: Unexpected end-of-file", fn, *li);
	return -1;
}


/*
 *	Read the users, huntgroups or hints file.
 *	Return a PAIR_LIST.
 */
int file_read(char *file, char *dir, PAIR_LIST **ret, int resolve)
{
	FILE		*fp;
	char		buffer[256];
	char		entry[256];
	char		fn[256];
	char		*ptr, *s;
	VALUE_PAIR	*check_tmp;
	VALUE_PAIR	*reply_tmp;
	PAIR_LIST	*pl = NULL, *last = NULL, *t;
	int		lineno = 0;
	int		old_lineno = 0;
	int		recno = 1;

	*ret = NULL;

	/*
	 *	Open the table
	 */
	if (*file != '/') {
		sprintf(fn, "%.127s/%.127s", dir, file);
		file = fn;
	}
	if ((fp = fopen(file, "r")) == NULL) {
		log(L_CONS|L_ERR, "Couldn't open %s for reading: %s",
		    file, strerror(errno));
		return -1;
	}

	/*
	 *	Read the entire file into memory for speed.
	 */
	while (fgets(buffer, sizeof(buffer), fp) != NULL) {
		lineno++;

		if (buffer[0] == '#' || buffer[0] == '\n' ||
		    buffer[0] == '\r') continue;

		/*
		 *	Include another file if we see
		 *	$INCLUDE filename
		 */
		if (strncasecmp(buffer, "$include", 8) == 0) {
			ptr = buffer + 8;
			while(isspace(*ptr))
				ptr++;
			s = ptr;
			while (!isspace(*ptr))
				ptr++;
			*ptr = 0;

			if (file_read(s, dir, &t, resolve) < 0) {
				fclose(fp);
				return -1;
			}
			if (t) {
				if (last)
					last->next = t;
				else
					pl = t;
				last = t;
				while (last && last->next)
					last = last->next;
			}
			continue;
		}

		old_lineno = lineno;
		if (read_entry(buffer, fp, file, &lineno, entry,
		    &check_tmp, &reply_tmp, resolve) < 0) {
			fclose(fp);
			return -1;
		}
		if ((t = malloc(sizeof(PAIR_LIST))) == NULL) {
			perror(progname);
			exit(1);
		}
		auth_type_fixup(check_tmp);
		memset(t, 0, sizeof(*t));
		t->name = strdup(entry);
		t->check = check_tmp;
		t->reply = reply_tmp;
		t->lineno = old_lineno;
		t->recno = recno++;
		check_tmp = NULL;
		reply_tmp = NULL;
		if (last)
			last->next = t;
		else
			pl = t;
		last = t;

#if SANITY_CHECK
		pair_sanity(file, t);
#endif

	}
	fclose(fp);

	*ret = pl;
	return 0;
}

