/*
 * cache.h   Definitions for structures and functions needed in cache.c
 *
 * Version: cache.c  0.99  04-13-1999  jeff@apex.net
 */    
#ifndef _CACHE_H
#define _CACHE_H

/* Misc definitions */
#define BUFSIZE  1024
#define MAXUSERNAME 20
#define HASHTABLESIZE 100000
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(bsdi)
#define PASSWDFILE "/etc/master.passwd"
#else 
#define PASSWDFILE "/etc/passwd"
#endif /* bsdi*/
#define SHADOWFILE "/etc/shadow"

/* Structure definitions */
struct mypasswd {
	char    *pw_name;       /* user name */
	char    *pw_passwd;     /* user password */
	uid_t   pw_uid;         /* user id */
	gid_t   pw_gid;         /* group id */
	int     loggedin;       /* number of logins */
	struct mypasswd *next;  /* next */
};

struct mygroup {
	char    *gr_name;        /* group name */
	char    *gr_passwd;      /* group password */
	gid_t   gr_gid;          /* group id */
	char    **gr_mem;        /* group members */
	struct mygroup *next;    /* next */
};         

/* Function prototypes */
int buildHashTable(void);
int buildGrpList(void);
void chgLoggedin(char *user, int diff);
struct mypasswd *findHashUser(const char *user);
int storeHashUser(struct mypasswd *new, int index);
int hashUserName(const char *s);
int hashradutmp(void);
int H_unix_pass(char *name, char *passwd);
int H_groupcmp(VALUE_PAIR *check, char *username);

#endif
