/*
 * cache.c:  Offers ability to cache /etc/group, /etc/passwd, /etc/shadow,
 *           and /var/log/radutmp 
 *
 * All users in the passwd/shadow files are stored in a hash table.
 * the hash lookup is VERY fast,  generally 1.0673 comparisons per
 * lookup.  For the unitiated, that's blazing.  You can't have less
 * than one comparison, for example.
 *
 * The /etc/group file is stored in a singly linked list, as that appears
 * to be fast enough.  It's generally a small enough file that hashing is
 * unnecessary.
 *
 *	(c) 1999 Author - Jeff Carneal, Apex Internet Services, Inc.
 *
 * Version: cache.c  0.99  04-13-1999  jeff@apex.net
 *
 */    
char *cache_rcsid =
"$Id: cache.c,v 1.7 2001/05/07 20:33:45 miquels Exp $";

#include<stdio.h>
#include<string.h>
#include<fcntl.h>
#include<grp.h>
#include<unistd.h>
#include<stdlib.h>
#include<errno.h>
#include<sys/stat.h>
#include<sys/types.h>

#include "sysdep.h"

#if !defined(NOSHADOW)
#include <shadow.h>
#endif /* !NOSHADOW */  

#if defined(__GLIBC__)
#  include <crypt.h>
#endif 

#include "radiusd.h"
#include "radutmp.h"
#include "cache.h"

/* Make the tables global since so many functions rely on them */
static struct mypasswd *hashtable[HASHTABLESIZE];
static struct mygroup *grphead = NULL;

/* Builds the hash table up by storing passwd/shadow fields
 * in memory.  Returns -1 on failure, 0 on success.
 */
int buildHashTable(void) {
	FILE *passwd, *shadow;
	char buffer[BUFSIZE];
	char idtmp[10];
	char username[MAXUSERNAME];
	char *ptr, *bufptr;
	int len, hashindex, numread=0;
	struct mypasswd *new, *cur, *next;

	memset((char *)username, 0, MAXUSERNAME);

	/* Initialize the table.  This works even if we're rebuilding it */
	for(hashindex=0; hashindex<HASHTABLESIZE; hashindex++) {
		if(hashtable[hashindex]) {
			cur = hashtable[hashindex];
			while(cur) {
				next = cur->next;
				free(cur->pw_name);	
				free(cur->pw_passwd);	
				free(cur);	
				cur = next;
			}
		}
	}	

	/* Init hash array */
	memset((struct mypasswd *)hashtable, 0, (HASHTABLESIZE*(sizeof(struct mypasswd *))));

	if( (passwd = fopen(PASSWDFILE, "r")) == NULL) {
		log(L_ERR, "HASH:  Can't open file %s:  %s", PASSWDFILE, strerror(errno));
		return -1;
	} else {
		while(fgets(buffer, BUFSIZE , passwd) != (char *)NULL) {
		  if (*buffer < ' ') {
		    continue;
		  }

			numread++;

			bufptr = buffer;
			/* Get usernames from password file */
			for(ptr = bufptr; *ptr!=':'; ptr++);
			len = ptr - bufptr;
			if((len+1) > MAXUSERNAME) {
				log(L_ERR, "HASH:  Username too long in line: %s", buffer);
			}
			strncpy(username, buffer, len);
			username[len] = '\0';

			/* Hash the username */
			hashindex = hashUserName(username);	
			/*printf("%s:%d\n", username, hashindex);*/
	
			/* Allocate space for structure to go in hashtable */
			if((new = (struct mypasswd *)malloc(sizeof(struct mypasswd))) == NULL) {
				log(L_ERR, "HASH:  Out of memory!");
				return -1;
			}
			memset((struct mypasswd *)new, 0, sizeof(struct mypasswd));

			/* Put username into new structure */
			if((new->pw_name = (char *)malloc(strlen(username)+1)) == NULL) {
				log(L_ERR, "HASH:  Out of memory!");
				return -1;
			}
			strncpy(new->pw_name, username, strlen(username)+1);

			/* Put passwords into array, if not shadowed */
			/* Get passwords from password file (shadow comes later) */
			ptr++;
			bufptr = ptr;
			while(*ptr!=':')
				ptr++;

#if defined(NOSHADOW)
			/* Put passwords into new structure (*/
			len = ptr - bufptr;
			if((new->pw_passwd = (char *)malloc(len+1)) == NULL) {
				log(L_ERR, "HASH:  Out of memory!");
				return -1;
			}
			strncpy(new->pw_passwd, bufptr, len);
			new->pw_passwd[len] = '\0';
#endif /* NOSHADOW */  

			/* 
		    * Put uid into structure.  Not sure why, but 
			 * at least we'll have it later if we need it
			 */
			ptr++;
			bufptr = ptr;
			while(*ptr!=':')
				ptr++;
			len = ptr - bufptr;
			strncpy(idtmp, bufptr, len);
			idtmp[len] = '\0';
			new->pw_uid = (uid_t)atoi(idtmp);	

			/* 
		    * Put gid into structure.  
			 */
			ptr++;
			bufptr = ptr;
			while(*ptr!=':')
				ptr++;
			len = ptr - bufptr;
			strncpy(idtmp, bufptr, len);
			idtmp[len] = '\0';
			new->pw_gid = (gid_t)atoi(idtmp);	

			/* 
			 * We're skipping name, home dir, and shell
			 * as I can't think of any use for storing them
			 */

			/*printf("User:  %s, UID:  %d, GID:  %d\n", new->pw_name, new->pw_uid, new->pw_gid);*/
			/* Store user in the hash */
			storeHashUser(new, hashindex);
		}	/* End while(fgets(buffer, BUFSIZE , passwd) != (char *)NULL) { */
	} /* End if */
	fclose(passwd);

#if !defined(NOSHADOW)
	if( (shadow= fopen(SHADOWFILE, "r")) == NULL) {
		log(L_ERR, "HASH:  Can't open file %s: %s", SHADOWFILE, strerror(errno));
		return -1;
	} else {
		while(fgets(buffer, BUFSIZE , shadow) != (char *)NULL) {

		  if (*buffer < ' ') {
		    continue;
		  }

			bufptr = buffer;
			/* Get usernames from shadow file */
			for(ptr = bufptr; *ptr!=':'; ptr++);
			len = ptr - bufptr;
			if((len+1) > MAXUSERNAME) {
				log(L_ERR, "HASH:  Username too long in line:  %s", buffer);
			}
			strncpy(username, buffer, len);
			username[len] = '\0';
			if((new = findHashUser(username)) == NULL) {
				log(L_ERR, "HASH:  Username %s in shadow but not passwd??", username);
				continue;
			}

			/* Put passwords into struct from shadow file */
			ptr++;
			bufptr = ptr;
			while(*ptr!=':')
				ptr++;
			len = ptr - bufptr;

			if((new->pw_passwd = (char *)malloc(len+1)) == NULL) {
				log(L_ERR, "HASH:  Out of memory!");
				return -1;
			}
			strncpy(new->pw_passwd, bufptr, len);
			new->pw_passwd[len] = '\0';
		}
	}
	fclose(shadow);
#endif

	/* Finally, let's look at radutmp and make a record of everyone
    * that's logged in currently */
	hashradutmp();

	/* log how many entries we stored from the passwd file */
	log(L_INFO, "HASH:  Stored %d entries from %s", numread, PASSWDFILE);

	return 0;
}

/* This function caches the /etc/group file, so it's one less thing
 * we have to lookup on disk.  it uses getgrent(), which is quite slow,
 * but the group file is generally small enough that it won't matter
 * As a side note, caching the user list per group was a major pain
 * in the ass, and I won't even need it.  I really hope that somebody
 * out there needs and appreciates it.
 * Returns -1 on failure, and 0 on success
 */
int buildGrpList(void) {

	int len, len2, index, numread=0;
	struct group *grp;
	struct mygroup *new, *cur, *next;
	char **member;

	cur = grphead;

	/* Free up former grp list (we can use this as a rebuild function too */
	while(cur) {
		next = cur->next;

		/* Free name, name, member list */
		for(member = cur->gr_mem; *member; member++) {
			free(*member);
		}
		free(cur->gr_mem);
		free(cur->gr_name);
		free(cur->gr_passwd);
		free(cur);
		cur = next;
	}                                  
	grphead = NULL;
	
	/* Make sure to begin at beginning */
	setgrent();

	/* Get next entry from the group file */
	while((grp = getgrent()) != NULL) {

		/* Make new mygroup structure in mem */
		if((new = (struct mygroup *)malloc(sizeof(struct mygroup))) == NULL) {
			log(L_ERR, "HASH:  (buildGrplist) Out of memory!");
			return -1;
		}
		memset((struct mygroup*)new, 0, sizeof(struct mygroup));
	
		/* copy grp entries to my structure */
		len = strlen(grp->gr_name);
		if((new->gr_name = (char *)malloc(len+1)) == NULL) {
			log(L_ERR, "HASH:  (buildGrplist) Out of memory!");
			return -1;
		}
		strncpy(new->gr_name, grp->gr_name, len);
		new->gr_name[len] = '\0';
		
		len = strlen(grp->gr_passwd);
		if((new->gr_passwd= (char *)malloc(len+1)) == NULL) {
			log(L_ERR, "HASH:  (buildGrplist) Out of memory!");
			return -1;
		}
		strncpy(new->gr_passwd, grp->gr_passwd, len);
		new->gr_passwd[len] = '\0';

		new->gr_gid = grp->gr_gid;	
		
		/* Allocate space for user list, as much as I hate doing groups
	  	 * that way.  
		 */
		for(member = grp->gr_mem; *member!=NULL; member++);
		len = member - grp->gr_mem;
		if((new->gr_mem = (char **)malloc((len+1)*sizeof(char **))) == NULL) {
			log(L_ERR, "HASH:  (buildGrplist) Out of memory!");
			return -1;
		}
		/* Now go back and copy individual users into it */
		for(member = grp->gr_mem; *member; member++) {
			len2 = strlen(*member);
			index = member - grp->gr_mem;
			if((new->gr_mem[index] = (char *)malloc(len2+1)) == NULL) {
				log(L_ERR, "HASH:  (buildGrplist) Out of memory!");
				return -1;
			}
			strncpy(new->gr_mem[index], *member, len2);
			new->gr_mem[index][len2] = '\0';
		}
		/* Make sure last entry in user list is 0 so we can loop thru it */
		new->gr_mem[len] = 0;

		/* Insert at beginning of list */
		new->next = grphead;
		grphead = new;

		numread++;
	}

	/* End */
	endgrent();

	log(L_INFO, "HASH:  Stored %d entries from /etc/group", numread);

	return 0;
}

/* This function changes the loggedin variable for a user
 * when they login or out.  This lets us keep track of 
 * what radutmp is doing without having to read it
 */
void chgLoggedin(char *user, int diff) {
	struct mypasswd *cur;

	cur = findHashUser(user);
	if(cur) {
		cur->loggedin += diff;
		/* Can't have less than 0 logins */
		if(cur->loggedin<0) {
			cur->loggedin = 0;
		}
		DEBUG2("  HASH:  Changed user %s to %d logins", user, cur->loggedin);
	}
}

/*
 * Looks up user in hashtable.  If user can't be found, returns 0.  
 * Otherwise returns a pointer to the structure for the user
 */
struct mypasswd *findHashUser(const char *user) {

	struct mypasswd *cur;
	int index;

	/* first hash the username and get the index into the hashtable */
	index = hashUserName(user);

	cur = hashtable[index];

	while((cur != NULL) && (strcmp(cur->pw_name, user))) {
		cur = cur->next;
	}

	if(cur) {
		DEBUG2("  HASH:  user %s found in hashtable bucket %d", user, index);
		return cur;
	}

	return (struct mypasswd *)0;

}

/* Stores the username sent into the hashtable */
int storeHashUser(struct mypasswd *new, int index) {

	/* store new record at beginning of list */
	new->next = hashtable[index];
	hashtable[index] = new;

	return 1;
}

/* Hashes the username sent to it and returns index into hashtable */
int hashUserName(const char *s) {
     unsigned long hash = 0;

     while (*s != '\0') {
         hash = hash * 7907 + (unsigned char)*s++;
		}

     return (hash % HASHTABLESIZE);
}              

/* Reads radutmp file, and increments the loggedin variable
 * for every login a user has...assuming we can find the user
 * in the hashtable
 */
int hashradutmp(void) {

	int fd;
	struct radutmp u;
	struct mypasswd *cur;

   if ((fd = open(radutmp_path, O_CREAT|O_RDONLY, 0644)) < 0)
      return 0;

   while(read(fd, &u, sizeof(u)) == sizeof(u)) {
      if ((u.login) && (u.type == P_LOGIN)) {
			cur = findHashUser(u.login);
			if(cur) {
				cur->loggedin++;		
			}
		}
	}
	close(fd);

	return 1;
}

/*
 *	Emulate the cistron unix_pass function, but do it using 
 *	our hashtable (iow, make it blaze).
 * return  0 on success
 * return -1 on failure
 * return -2 on error (let caller fall back to old method)
 */
int H_unix_pass(char *name, char *passwd) {
	struct mypasswd	*pwd;
	char *encrypted_pass;
	char *encpw;

	/*
	 *	Get encrypted password from password file
	 */
	if ((pwd = findHashUser(name)) == NULL) {
		/* Default to old way if user isn't hashed */
		return -2;
	}
	encrypted_pass = pwd->pw_passwd;

	/*
	 *	We might have a passwordless account.
	 */
	if (encrypted_pass[0] == 0) return 0;

	/*
	 *	Check encrypted password.
	 */
	encpw = (char *)crypt(passwd, encrypted_pass);
	if (strcmp(encpw, encrypted_pass))
		return -1;

	return 0;
}

/*
 * Emulate groupcmp in files.c, but do it (much) faster
 * return -2 on error (let caller fall back to old method),
 * -1 on match fail, or 0 on success
 */
int H_groupcmp(VALUE_PAIR *check, char *username) {
	struct mypasswd *pwd;
	struct mygroup *cur;
	char **member;

	/* get the user from the hash */
	if (!(pwd = findHashUser(username)))
		return -2;

	/* let's find this group */
	if(grphead) {
		cur = grphead;
		while((cur) && (strcmp(cur->gr_name, check->strvalue))) {
			cur = cur->next;	
		}	
		/* found the group, now compare it */
		if(!cur) {
			/* Default to old function if we can't find it */
			return -2;
		} else {
			if(pwd->pw_gid == cur->gr_gid) {
				DEBUG2("  HASH:  matched user %s in group %s", username, cur->gr_name);
				return 0;
			} else {
				for(member = cur->gr_mem; *member; member++) {
					if (strcmp(*member, pwd->pw_name) == 0) {
						DEBUG2("  HASH:  matched user %s in group %s", username, cur->gr_name);
						return 0;
					}
				}
			}
		}
	}

	return -1;
}
