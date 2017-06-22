/*
 * auth.c	User authentication.
 *
 *		Copyright 1998-2001 Cistron Internet Services B.V.
 */
char auth_rcsid[] =
"$Id: auth.c,v 1.32 2001/05/26 21:14:49 miquels Exp $";

#include	<sys/types.h>
#include	<sys/time.h>

#include	<stdio.h>
#include	<stdlib.h>
#include	<pwd.h>
#include	<time.h>
#include	<unistd.h>
#include	<signal.h>
#include	<errno.h>
#include	<sys/wait.h>

#include	"sysdep.h"

#if !defined(NOSHADOW)
#  include	<shadow.h>
#endif /* !NOSHADOW */

#if defined(__GLIBC__)
#  include	<crypt.h>
#endif

#ifdef OSFC2
#  include	<sys/security.h>
#  include	<prot.h>
#endif

#ifdef OSFSIA
#  include	<sia.h>
#  include	<siad.h>
#endif

#include	"radiusd.h"
#include	"cache.h"

#if !defined(__linux__) && !defined(__GLIBC__)
  extern char *crypt();
#endif

/*
 *	Tests to see if the users password has expired.
 *
 *	Return: Number of days before expiration if a warning is required
 *		otherwise 0 for success and -1 for failure.
 */
static int pw_expired(UINT4 exptime)
{
	struct timeval	tp;
	struct timezone	tzp;
	UINT4		exp_remain;
	int		exp_remain_int;

	gettimeofday(&tp, &tzp);
	if (tp.tv_sec > exptime)
		return -1;

	if (warning_seconds != 0) {
		if (tp.tv_sec > exptime - warning_seconds) {
			exp_remain = exptime - tp.tv_sec;
			exp_remain /= (UINT4)SECONDS_PER_DAY;
			exp_remain_int = exp_remain;
			return exp_remain_int;
		}
	}
	return 0;
}


/*
 *	Check if account has expired, and if user may login now.
 */
static int check_expiration(VALUE_PAIR *check_item, char *reply_msg)
{
	int result;
	int retval;

	result = 0;
	while (result == 0 && check_item != (VALUE_PAIR *)NULL) {

		/*
		 *	Check expiration date if we are doing password aging.
		 */
		if (check_item->attribute == PW_EXPIRATION) {
			/*
			 *	Has this user's password expired
			 */
			retval = pw_expired(check_item->lvalue);
			if (retval < 0) {
				result = -1;
				strcpy(reply_msg, "Password Has Expired\r\n");
				break;
			} else {
				if (retval > 0) {
					sprintf(reply_msg,
					  "Password Will Expire in %d Days\r\n",
					  retval);
				}
			}
		}
		check_item = check_item->next;
	}
	return result;
}
/*
 *	Check the users password against the standard UNIX
 *	password table.
 */
static int unix_pass(char *name, char *passwd)
{
	struct passwd	*pwd;
	char		*encpw;
	char		*encrypted_pass;
/*	int		ret; */
#if !defined(NOSHADOW)
	struct spwd	*spwd;
#endif /* !NOSHADOW */
#ifdef OSFC2
	struct pr_passwd *pr_pw;
#endif
#ifdef OSFSIA
	char		*info[2];
	char		*progname = "radius";
	SIAENTITY	*ent = NULL;
#endif
/*
	if (cache_passwd && (ret = H_unix_pass(name, passwd)) != -2)
		return ret;
*/
#ifdef OSFSIA
	info[0] = progname;
	info[1] = NULL;
	if (sia_ses_init (&ent, 1, info, NULL, name, NULL, 0, NULL) !=
	    SIASUCCESS)
		return -1;
	if ((ret = sia_ses_authent (NULL, passwd, ent)) != SIASUCCESS) {
		if (ret & SIASTOP)
			sia_ses_release (&ent);
		return -1;
	}
	if (sia_ses_estab (NULL, ent) == SIASUCCESS) {
		sia_ses_release (&ent);
		return 0;
	}
	return -1;
#else /* OSFSIA */
#ifdef OSFC2
	if ((pr_pw = getprpwnam(name)) == NULL)
		return -1;
	encrypted_pass = pr_pw->ufld.fd_encrypt;
#else /* OSFC2 */
	/*
	 *	Get encrypted password from password file
	 */
	if ((pwd = rad_getpwnam(name)) == NULL) {
		return -1;
	}
	encrypted_pass = pwd->pw_passwd;
#endif /* OSFC2 */

#ifdef DENY_SHELL
	/*
	 *	Undocumented temporary compatibility for iphil.NET
	 *	Users with a certain shell are always denied access.
	 */
	if (strcmp(pwd->pw_shell, DENY_SHELL) == 0) {
		log(L_AUTH, "unix_pass: [%s]: invalid shell", name);
		return -1;
	}
#endif

#if !defined(NOSHADOW)
	/*
	 *      See if there is a shadow password.
	 */
	if ((spwd = getspnam(name)) != NULL)
		encrypted_pass = spwd->sp_pwdp;

	/*
	 *      Check if password has expired.
	 */
	if (spwd && spwd->sp_lstchg >= 0 && spwd->sp_max >= 0 &&
	    (time(NULL) / 86400) > (spwd->sp_lstchg + spwd->sp_max)) {
		log(L_AUTH, "unix_pass: [%s]: password has expired", name);
		return -1;
	}

	/*
	 *      Check if account has expired.
	 */
	if (spwd && spwd->sp_expire >= 0 &&
	    (time(NULL) / 86400) > spwd->sp_expire) {
		log(L_AUTH, "unix_pass: [%s]: account has expired", name);
		return -1;
	}
#endif

#if defined(__FreeBSD__) || defined(bsdi) || defined(_PWF_EXPIRE)
	/*
	 *	Check if password has expired.
	 */
	if (pwd->pw_expire > 0 && time(NULL) > pwd->pw_expire) {
		log(L_AUTH, "unix_pass: [%s]: password has expired", name);
		return -1;
	}
#endif

#ifdef OSFC2
	/*
	 *	Check if account is locked.
	 */
	if (pr_pw->uflg.fg_lock!=1) {
		log(L_AUTH, "unix_pass: [%s]: account locked", name);
		return -1;
	}
#endif /* OSFC2 */

	/*
	 *	We might have a passwordless account.
	 */
	if (encrypted_pass[0] == 0) return 0;

	/*
	 *	Check encrypted password.
	 */
	encpw = crypt(passwd, encrypted_pass);
	if (strcmp(encpw, encrypted_pass))
		return -1;

#ifdef ETC_SHELL
	{
	  char *shell;

	  while ((shell = getusershell()) != NULL) {
	    if (strcmp(shell, pwd->pw_shell) == 0 ||
		strcmp(shell, "/RADIUSD/ANY/SHELL") == 0) {
	      endusershell();
	      return 0;
	    }
	  }

	  endusershell();
	  return -1;
	}
#endif /* ETC_SHELL */

	return 0;
#endif /* OSFSIA */
}


/*
 *	Check password.
 *
 *	Returns:	0  OK
 *			-1 Password fail
 *			-2 Rejected
 *			1  End check & return.
 */
static int rad_check_password(AUTH_REQ *authreq, int activefd,
	VALUE_PAIR *check_item,
	VALUE_PAIR *namepair,
	VALUE_PAIR *user_reply,
	char *pw_digest, char *reply_msg, char *userpass)
{
	VALUE_PAIR	*auth_type_pair;
	VALUE_PAIR	*password_pair;
	VALUE_PAIR	*auth_item;
	VALUE_PAIR	*presuf_item;
	VALUE_PAIR	*tmp;
	VALUE_PAIR	*calling_id;
	char		call_from[32];
#ifdef PAM
        VALUE_PAIR      *pampair;
	char		*pamauth = NULL;
#endif
#ifdef USEMYSQL
	char		calculeted[32];
	char		msch2resp[42];
	char		mppe_sendkey[34];
	char		mppe_recvkey[34];
	VALUE_PAIR	*reply_item;
        SQL_PWD         *sql_pwd;
        time_t          now;
	int		lenn;
#endif
	char		string[AUTH_STRING_LEN];
	char		name[AUTH_STRING_LEN];
	char		*ptr;
	int		auth_type = -1;
	int		i;
	int		strip_username;
	int		result;

	result = -1;
	userpass[0] = 0;
	string[0] = 0;

	/*
	 *	Look for matching check items. We bail out early 
	 *	if the authentication type is PW_AUTHTYPE_ACCEPT or
	 *	PW_AUTHTYPE_REJECT, but not after decrypting the
	 *	password for logging purposes...
	 */
	if ((auth_type_pair = pairfind(check_item, PW_AUTHTYPE)) != NULL)
		auth_type = auth_type_pair->lvalue;

	if (auth_type == PW_AUTHTYPE_ACCEPT)
		result = 0;

	if (auth_type == PW_AUTHTYPE_REJECT) {
		reply_msg[0] = 0;
		result = -2;
	}

	/*
	 *	Find the password sent by the user. It SHOULD be there,
	 *	if it's not authentication fails.
	 *
	 *	FIXME: add MS-CHAP support ? FIXED !!!!! by me ;)
	 */
    if ((auth_item = pairfind(authreq->request, PW_CHAPMS2_RESPONSE)) == NULL)
      if ((auth_item = pairfind(authreq->request, PW_CHAPMS_RESPONSE)) == NULL)
	if ((auth_item = pairfind(authreq->request, PW_CHAP_PASSWORD)) == NULL)
	  if ((auth_item = pairfind(authreq->request, PW_PASSWORD)) == NULL)
		return result;

	/*
	 *	Decrypt the password.
	 */
	if (auth_item != NULL && auth_item->attribute == PW_PASSWORD) {
		rad_pwdecode(auth_item->strvalue, string,
			auth_item->length, pw_digest);
		strcpy(userpass, string);
	}

	if (auth_type == PW_AUTHTYPE_ACCEPT || auth_type == PW_AUTHTYPE_REJECT)
		return result;

	/*
	 *	Find the password from the users file.
	 */
	if ((password_pair = pairfind(check_item, PW_CRYPT_PASSWORD)) != NULL)
		auth_type = PW_AUTHTYPE_CRYPT;
	else
		password_pair = pairfind(check_item, PW_PASSWORD);

	if ((calling_id = pairfind(check_item, PW_CALLING_STATION_ID)) != NULL)
		strcpy(call_from, calling_id->strvalue);

	/*
	 *	See if there was a Prefix or Suffix included.
	 *	Note: sizeof(name) == sizeof(namepair->strvalue)
	 *	so strcpy is safe.
	 */
	strip_username = 1;
	if ((presuf_item = pairfind(check_item, PW_PREFIX)) == NULL)
		presuf_item = pairfind(check_item, PW_SUFFIX);
	if (presuf_item) {
		tmp = pairfind(check_item, PW_STRIP_USERNAME);
		if (tmp != NULL && tmp->lvalue == 0)
			strip_username = 0;
		i = presufcmp(presuf_item, namepair->strvalue, name, sizeof(name));
		if (i != 0 || strip_username == 0)
			strcpy(name, namepair->strvalue);
	} else
		strcpy(name, namepair->strvalue);

#if 0 /* DEBUG */
	printf("auth_type=%d, string=%s, namepair=%s, password_pair=%s\n",
		auth_type, string, name,
		password_pair ? password_pair->strvalue : "");
#endif

	result = 0;

	switch(auth_type) {
		case PW_AUTHTYPE_SYSTEM:
			DEBUG2("  auth: System");
			/*
			 *	Check the password against /etc/passwd.
			 */
			if (unix_pass(name, string) != 0)
				result = -1;
			break;
#if 0
		case PW_AUTHTYPE_PAM:
#ifdef PAM
			DEBUG2("  auth: Pam");
			/*
			 *	Use the PAM database.
			 *
			 *	cjd 19980706 --
			 *	Use what we found for pamauth or set it to
			 *	the default "radius" and then jump into
			 *	pam_pass with the extra info.
			 */
        		pampair = pairfind(check_item, PAM_AUTH_ATTR);
			pamauth = pampair ? pampair->strvalue:PAM_DEFAULT_TYPE;
			if (pam_pass(name, string, pamauth, reply_msg) != 0)
				result = -1;
#else
			log(L_ERR, "%s: PAM authentication not available",
				name);
			result = -1;

			break;
#endif
#endif
		case PW_AUTHTYPE_MYSQL:
#ifdef USEMYSQL
			DEBUG2("  auth: Mysql");

			now = time(NULL);
			if ((sql_pwd = sql_getpwname(name)) == NULL){
				result =  -1;
				break;
			}

			if (auth_item->attribute == PW_CHAP_PASSWORD){
#ifdef CHAP 
				if (passwdhash == 1){
				strcpy(userpass, "<CHAP-MD5 not support HASH>");
					result = -1;
					break;
				}

				ptr = string;
				*ptr++ = auth_item->strvalue[0];
				i = 1;
				lenn = strlen(sql_pwd->passwd);
				memcpy(ptr, sql_pwd->passwd, lenn);
				ptr += lenn;
				i += lenn;

		                if ((tmp = pairfind(authreq->request,
		                    PW_CHAP_CHALLENGE)) != NULL) {
					memcpy(ptr, tmp->strvalue, tmp->length);
					i += tmp->length;
				} else {
					memcpy(ptr, authreq->vector, 
							AUTH_VECTOR_LEN);
					i += AUTH_VECTOR_LEN;
				}
				md5_calc(pw_digest, (unsigned char *)string, i);
				if (memcmp(pw_digest, auth_item->strvalue + 1,
						CHAP_VALUE_LENGTH) != 0){
				strcpy(userpass, "<BAD-CHAP-MD5-PASSWORD>");
					result = -1;
					break;
				} else {
				strcpy(userpass, sql_pwd->passwd);
					result = 0;
				}
#else
			strcpy(userpass, "<CHAP-MD5 no support>");
			log(L_ERR,"%s: Mysql-ChapMD5 auth not available", name);
			result = -1;
			break; 
#endif
		} else	if (auth_item->attribute == PW_CHAPMS_RESPONSE){
#ifdef CHAPMS 
		        if ((tmp = pairfind(authreq->request,
		                 PW_CHAPMS_CHALLENGE)) == NULL) {
		                 strcpy(userpass, "<CHAP-MS no challenge>");
		                 result = -1;
		                 break;
		        }
			if (tmp->length < 8) {
				log(L_AUTH, 
			"Attribute \"MS-CHAP-Challange\" has wrong format");
				result = -1;
				break;
			}
			if (auth_item->length < 50 ) {
				log(L_AUTH, 
			"Attribute \"MS-CHAP-Response\" has wrong format");
				result = -1;
				break;
			}
		if (auth_item->strvalue[1] & 0x01 ){
			mschap(tmp->strvalue, sql_pwd->passwd, calculeted, 1);

			if (memcmp(auth_item->strvalue + 26, calculeted, 24)){
		                strcpy(userpass, "<BAD-CHAP-MS-PASSWORD/nt>");
				result = -1;
				break;
			} 
		} else {
			if (passwdhash == 1){
			strcpy(userpass, "<CHAP-MS/lm not support HASH>");
				result = -1;
				break;
			}
			mschap(tmp->strvalue, sql_pwd->passwd, calculeted, 0);
			if (memcmp(auth_item->strvalue + 2, calculeted, 24)){
		                strcpy(userpass, "<BAD-CHAP-MS-PASSWORD/lm>");
				result = -1;
				break;
			} 

		/*	*/
		}
			/* ========== [MPPE Key for ChapMS] ======== */
				memset(mppe_sendkey, 0, 32);
			if (passwdhash == 0)
				md4_calc(mppe_sendkey+8, sql_pwd->passwd, 16);
			else	hex2bin(sql_pwd->passwd, mppe_sendkey+8, 16);

                        if ((reply_item = pairfind(user_reply,
                                PW_CHAP_MPPE_KEYS)) != NULL){
                                memcpy(reply_item->strvalue, mppe_sendkey, 16);
                        }else{
                                if(!(reply_item = malloc(sizeof(VALUE_PAIR)))){
                                        log(L_ERR|L_CONS, "no memory");
                                        exit(1);
                                }
                                memset(reply_item, 0, sizeof(VALUE_PAIR));
                                reply_item->attribute = PW_CHAP_MPPE_KEYS;
                                strcpy(reply_item->name, "MS-CHAP-MPPE-Keys");
                                reply_item->type = PW_TYPE_STRING;
                                reply_item->length = 16;
                                memcpy(reply_item->strvalue, mppe_sendkey, 16);
                                pairadd(&user_reply, reply_item);
      			} 

				strcpy(userpass, sql_pwd->passwd);
				result = 0;
#else
		        strcpy(userpass, "<CHAP-MS no support>");
			log(L_ERR,"%s: Mysql-ChapMS auth not available", name);
			result = -1;
			break; 
#endif
		} else if (auth_item->attribute == PW_CHAPMS2_RESPONSE){
#ifdef CHAPMS2 
		                if ((tmp = pairfind(authreq->request,
		                    PW_CHAPMS_CHALLENGE)) == NULL) {
		                    strcpy(userpass, "<CHAP-MS2 no challenge>");
		                    result = -1;
		                    break;
		                }
			if ( tmp->length < 16) {
				log(L_AUTH, 
			"Attribute \"MS-CHAP2-Challange\" has wrong format");
				result = -1;
				break;
			}
			if (auth_item->length < 50 ) {
				log(L_AUTH, 
			"Attribute \"MS-CHAP2-Response\" has wrong format");
				result = -1;
				break;
			}

			mschap2(auth_item->strvalue + 2, tmp->strvalue,
						sql_pwd, calculeted);

			if (memcmp(auth_item->strvalue + 26, calculeted, 24) != 0){
		                strcpy(userpass, "<BAD-CHAP2-MS-PASSWORD>");
				result = 0; /* -1; reject*/

                        if ((reply_item = pairfind(user_reply,
                                PW_CHAPMS_ERROR)) != NULL){
                                memcpy(reply_item->strvalue +1, "E=691 R=1", 9);
                        }else{
                                if(!(reply_item = malloc(sizeof(VALUE_PAIR)))){
                                        log(L_ERR|L_CONS, "no memory");
                                        exit(1);
                                }
                                memset(reply_item, 0, sizeof(VALUE_PAIR));
                                reply_item->attribute = PW_CHAPMS_ERROR;
                                strcpy(reply_item->name, "MS-CHAP-Error");
                                reply_item->type = PW_TYPE_STRING;
                                reply_item->length = 9 + 1;
/*				reply_item->strvalue[0] = (unsigned char)*tmp->strvalue;*/
				reply_item->strvalue[0] = 0;
                                memcpy(reply_item->strvalue +1, "E=691 R=1", 9);
                                pairadd(&user_reply, reply_item);
      			} 
				break;
			}
#if 1
			if (sql_pwd->block == 1){
		                strcpy(userpass, "<CHAP2-MS E=647 R=0>");
				result = 0; /* -1; reject*/

                        if ((reply_item = pairfind(user_reply,
                                PW_CHAPMS_ERROR)) != NULL){
                                memcpy(reply_item->strvalue +1, "E=647 R=0", 9);
                        }else{
                                if(!(reply_item = malloc(sizeof(VALUE_PAIR)))){
                                        log(L_ERR|L_CONS, "no memory");
                                        exit(1);
                                }
                                memset(reply_item, 0, sizeof(VALUE_PAIR));
                                reply_item->attribute = PW_CHAPMS_ERROR;
                                strcpy(reply_item->name, "MS-CHAP-Error");
                                reply_item->type = PW_TYPE_STRING;
                                reply_item->length = 9 + 1;
/*				reply_item->strvalue[0] = (unsigned char)*tmp->strvalue;*/
				reply_item->strvalue[0] = 0;
                                memcpy(reply_item->strvalue +1, "E=647 R=0", 9);
                                pairadd(&user_reply, reply_item);
      			} 
				break;
			}
#endif
			mschap2_resp(sql_pwd, calculeted,
				     auth_item->strvalue + 2,
				     tmp->strvalue,
				     msch2resp);

                        if ((reply_item = pairfind(user_reply,
                                PW_CHAPMS2_SUCCESS)) != NULL){
                                memcpy(reply_item->strvalue + 1, msch2resp, 42);
                        }else{
                                if(!(reply_item = malloc(sizeof(VALUE_PAIR)))){
                                        log(L_ERR|L_CONS, "no memory");
                                        exit(1);
                                }
                                memset(reply_item, 0, sizeof(VALUE_PAIR));
                                reply_item->attribute = PW_CHAPMS2_SUCCESS;
                                strcpy(reply_item->name, "MS-CHAP2-Success");
                                reply_item->type = PW_TYPE_STRING;
                                reply_item->length = 42 + 1;
				reply_item->strvalue[0] = (unsigned char)*tmp->strvalue;
                                memcpy(reply_item->strvalue + 1, msch2resp, 42);
                                pairadd(&user_reply, reply_item);
      			} 

			mppe_chap2_gen_keys128(authreq->secret,
					authreq->vector,
					sql_pwd->passwd,
					auth_item->strvalue + 26,
					mppe_sendkey,mppe_recvkey);

			/* ======== [MPPE Send/Recv Key] ======= */

                        if ((reply_item = pairfind(user_reply,
                                PW_MSMPPE_SEND_KEY)) != NULL){
                                memcpy(reply_item->strvalue, mppe_sendkey, 34);
                        }else{
                                if(!(reply_item = malloc(sizeof(VALUE_PAIR)))){
                                        log(L_ERR|L_CONS, "no memory");
                                        exit(1);
                                }
                                memset(reply_item, 0, sizeof(VALUE_PAIR));
                                reply_item->attribute = PW_MSMPPE_SEND_KEY;
                                strcpy(reply_item->name, "MS-MPPE-Send-Key");
                                reply_item->type = PW_TYPE_STRING;
                                reply_item->length = 34;
                                memcpy(reply_item->strvalue, mppe_sendkey, 34);
                                pairadd(&user_reply, reply_item);
      			} 
		
                        if ((reply_item = pairfind(user_reply,
                                PW_MSMPPE_RECV_KEY)) != NULL){
                                memcpy(reply_item->strvalue, mppe_recvkey, 34);
                        }else{
                                if(!(reply_item = malloc(sizeof(VALUE_PAIR)))){
                                        log(L_ERR|L_CONS, "no memory");
                                        exit(1);
                                }
                                memset(reply_item, 0, sizeof(VALUE_PAIR));
                                reply_item->attribute = PW_MSMPPE_RECV_KEY;
                                strcpy(reply_item->name, "MS-MPPE-Recv-Key");
                                reply_item->type = PW_TYPE_STRING;
                                reply_item->length = 34;
                                memcpy(reply_item->strvalue, mppe_recvkey, 34);
                                pairadd(&user_reply, reply_item);
      			} 

			/* "MS-MPPE-Encryption-Policy"(7) 
			- (inst->require_encryption)? 
				"0x00000002": required
				"0x00000001"  allowed
				 0		no
			*/
                        if ((reply_item = pairfind(user_reply,
                                PW_MSMPPE_ENCRIPTION_POLICY)) != NULL){
				reply_item->lvalue = MPPE_POLICY_REQUIRED;
                        }else{
                                if(!(reply_item = malloc(sizeof(VALUE_PAIR)))){
                                        log(L_ERR|L_CONS, "no memory");
                                        exit(1);
                                }
                                memset(reply_item, 0, sizeof(VALUE_PAIR));
                                reply_item->attribute = PW_MSMPPE_ENCRIPTION_POLICY;
                                strcpy(reply_item->name, "MS-MPPE-Encription-Policy");
                                reply_item->type = PW_TYPE_INTEGER;
                                reply_item->length = 4;
				reply_item->lvalue = MPPE_POLICY_REQUIRED;
                                pairadd(&user_reply, reply_item);
      			} 
			/* "MS-MPPE-Encryption-Types"(8)
			 -  (inst->require_strong)?
				 "0x00000004":
				 "0x00000006" 
			*/

                        if ((reply_item = pairfind(user_reply,
                                PW_MSMPPE_ENCRIPTION_TYPES)) != NULL){
				reply_item->lvalue = MPPE_TYPE_128;
                        }else{
                                if(!(reply_item = malloc(sizeof(VALUE_PAIR)))){
                                        log(L_ERR|L_CONS, "no memory");
                                        exit(1);
                                }
                                memset(reply_item, 0, sizeof(VALUE_PAIR));
                                reply_item->attribute = PW_MSMPPE_ENCRIPTION_TYPES;
                                strcpy(reply_item->name, "MS-MPPE-Encription-Types");
                                reply_item->type = PW_TYPE_INTEGER;
                                reply_item->length = 4;
				reply_item->lvalue = MPPE_TYPE_40 | MPPE_TYPE_128;
                                pairadd(&user_reply, reply_item);
      			} 

				strcpy(userpass, sql_pwd->passwd);
				result = 0;
#else
		        strcpy(userpass, "<CHAP-MS2 no support>");
			log(L_ERR,"%s: Mysql-ChapMS2 auth not available", name);
			result = -1;
			break; 
#endif
		} else	if (passwdhash != 1 && strcmp(sql_pwd->passwd,
				crypt(string,sql_pwd->passwd)) != 0){
				if (passwdhash == 1)
				strcpy(userpass, "<CRYPT not support HASH>");
                		result = -1;
				break;
			}

			if (sql_pwd->money <= 0.01 && sql_pwd->type != 0){
				result = -10;
				break;
			}
			if (sql_pwd->activ > now){
		                result = -9;
				break;
			}
			if (sql_pwd->block == 1){
				result = -6;
				break;
			}

/*			result = sql_crypt(name,string); */	
        DEBUG("User %s successfully authenticated via mysql", name);
			result = 0;
#else
			log(L_ERR,"%s: Mysql authentication not available",
				name);
			result = -1;
#endif
			break;	
		case PW_AUTHTYPE_CRYPT:
			DEBUG2("  auth: Crypt");
			if (password_pair == NULL) {
				result = string[0] ? -1 : 0;
				break;
			}
			if (strcmp(password_pair->strvalue,
			    crypt(string, password_pair->strvalue)) != 0)
					result = -1;
			break;
		case PW_AUTHTYPE_LOCAL:
			DEBUG2("  auth: Local");
			/*
			 *	Local password is just plain text.
	 		 */
			if (auth_item->attribute != PW_CHAP_PASSWORD) {
				/*
				 *	Plain text password.
				 */
				if (password_pair == NULL ||
				    strcmp(password_pair->strvalue, string)!=0)
					result = -1;
				break;
			}

			/*
			 *	CHAP - calculate MD5 sum over CHAP-ID,
			 *	plain-text password and the Chap-Challenge.
			 *	Compare to Chap-Response (strvalue + 1).
			 *
			 *	FIXME: might not work with Ascend because
			 *	we use vp->length, and Ascend gear likes
			 *	to send an extra '\0' in the string!
			 */
			strcpy(string, "{chap-password}");
			if (password_pair == NULL) {
				result= -1;
				break;
			}
			i = 0;
			ptr = string;
			*ptr++ = *auth_item->strvalue;
			i++;
			memcpy(ptr, password_pair->strvalue,
				password_pair->length);
			ptr += password_pair->length;
			i += password_pair->length;
			/*
			 *	Use Chap-Challenge pair if present,
			 *	Request-Authenticator otherwise.
			 */
			if ((tmp = pairfind(authreq->request,
			    PW_CHAP_CHALLENGE)) != NULL) {
				memcpy(ptr, tmp->strvalue, tmp->length);
				i += tmp->length;
			} else {
				memcpy(ptr, authreq->vector, AUTH_VECTOR_LEN);
				i += AUTH_VECTOR_LEN;
			}
			md5_calc(pw_digest, (unsigned char *)string, i);

			/*
			 *	Compare them
			 */
			if (memcmp(pw_digest, auth_item->strvalue + 1,
				   CHAP_VALUE_LENGTH) != 0) {
				strcpy(userpass, "<BAD-CHAP-PASSWORD>");
				result = -1;
			} else
				strcpy(userpass, password_pair->strvalue);
			break;
		default:
			result = -1;
			break;
	}

	return result;
}

/*
 *	Initial step of authentication.
 *	Find username, and process the hints and huntgroups file.
 */
int rad_auth_init(AUTH_REQ *authreq, int activefd)
{
	VALUE_PAIR	*vp;
	int		  r;

	/*
	 *	Get the username from the request
	 */
	vp = pairfind(authreq->request, PW_USER_NAME);

	if (vp == NULL || vp->strvalue[0] == 0) {
		log(L_ERR, "%s username: [] (from nas %s)",
			(vp ? "Empty" : "No"), nas_name2(authreq));
		rad_send_reply(PW_AUTHENTICATION_REJECT, authreq,
			authreq->request, NULL, activefd);
		authfree(authreq);
		return -1;
	}

	strNcpy(authreq->username, vp->strvalue, sizeof(authreq->username));
#if 0
	/*
	 *	Add any specific attributes for this username.
	 */
	hints_setup(authreq->request);
#endif
	if (log_auth_detail)
		rad_accounting_detail(authreq, -1, log_auth_detail);

	for (r = 1; r < strlen(vp->strvalue); r++)
	 if(!( ((vp->strvalue[r] >= 'a') && 
		(vp->strvalue[r] <= 'z')) ||	
	       ((vp->strvalue[r] >= '0') && 
		(vp->strvalue[r] <= '9')) ||
		(vp->strvalue[r] == '_') || 
		(vp->strvalue[r] == '-') ||
		(vp->strvalue[r] == '.') )){
		log(L_AUTH, "Incorrect chars in login: [%s] (from nas %s)",
			vp->strvalue, nas_name2(authreq));	
		rad_send_reply(PW_AUTHENTICATION_REJECT, authreq,
			NULL, NULL, activefd);
		authfree(authreq);
		return -1;
	}


	/*
	 *	See if the user has access to this huntgroup.
	 */
	if (!huntgroup_access(authreq->request)) {
		log(L_AUTH, "No huntgroup access: [%s] (%s)",
			vp->strvalue, auth_name(authreq, 1));
		rad_send_reply(PW_AUTHENTICATION_REJECT, authreq,
			authreq->request, NULL, activefd);
		authfree(authreq);
		return -1;
	}

	return 0;
}


/*
 *	Process and reply to an authentication request
 */
int rad_authenticate(AUTH_REQ *authreq, int activefd)
{
	VALUE_PAIR	*namepair;
	VALUE_PAIR	*check_item;
	VALUE_PAIR	*reply_item;
	VALUE_PAIR	*auth_item;
	VALUE_PAIR	*user_check;
	VALUE_PAIR	*user_reply;
	int		result, r;
	char		pw_digest[16];
	char		userpass[AUTH_STRING_LEN];
	char		reply_msg[AUTH_STRING_LEN];
	char		*ptr;
	char		*exec_program;
	char		*cause = "";
	int		exec_wait;
	int		seen_callback_id;
#ifdef USEMYSQL
	int		expired; /* , res_var; */
	u_int		octetslimit;
	int		octetsdir;
	SQL_PWD		*sqlpwd;
/*	char		buf_query[256]; */
#endif

	user_check = NULL;
	user_reply = NULL;
	reply_msg[0] = 0;

	/*
	 *	Get the username from the request.
	 *	All checking has been done by rad_auth_init().
	 */
	namepair = pairfind(authreq->request, PW_USER_NAME);

	/*
	 *	If this request got proxied to another server, we need
	 *	to add an initial Auth-Type: Auth-Accept for success,
	 *	Auth-Reject for fail. We also need to add the reply
	 *	pairs from the server to the initial reply.
	 */
	if (authreq->server_code == PW_AUTHENTICATION_REJECT ||
	    authreq->server_code == PW_AUTHENTICATION_ACK) {
		if ((user_check = malloc(sizeof(VALUE_PAIR))) == NULL) {
			log(L_ERR|L_CONS, "no memory");
			exit(1);
		}
		memset(user_check, 0, sizeof(VALUE_PAIR));
		user_check->attribute = PW_AUTHTYPE;
		strcpy(user_check->name, "Auth-Type");
		user_check->type = PW_TYPE_INTEGER;
		user_check->length = 4;
	}
	if (authreq->server_code == PW_AUTHENTICATION_REJECT)
		user_check->lvalue = PW_AUTHTYPE_REJECT;
	if (authreq->server_code == PW_AUTHENTICATION_ACK)
		user_check->lvalue = PW_AUTHTYPE_ACCEPT;

	if (authreq->server_reply) {
		user_reply = authreq->server_reply;
		authreq->server_reply = NULL;
	}

    for (r=1; r < strlen(namepair->strvalue); r++)
	 if(!( ((namepair->strvalue[r] >= 'a') && 
		(namepair->strvalue[r] <= 'z')) ||
	       ((namepair->strvalue[r] >= '0') && 
		(namepair->strvalue[r] <= '9')) ||
		(namepair->strvalue[r] == '_') || 
		(namepair->strvalue[r] == '-') ||
		(namepair->strvalue[r] == '.') )){
		log(L_AUTH, "Incorrect chars in login: [%s] (from nas %s)",
			namepair->strvalue, nas_name2(authreq));	
		rad_send_reply(PW_AUTHENTICATION_REJECT, authreq,
			NULL, NULL, activefd);
		authfree(authreq);
		return -1;
	}


	/*
	 *	Get the user from the database
	 */
	if (user_find(namepair->strvalue, authreq->request,
	   &user_check, &user_reply) != 0) {
		log(L_AUTH, "Unknown user: [%s] (%s)",
			namepair->strvalue, auth_name(authreq, 1));
		DEBUG2("The user is NOT configured to have any reply attributes.  "
		       "If you are proxying, please read doc/README.proxy.");
		rad_send_reply(PW_AUTHENTICATION_REJECT, authreq,
			NULL, NULL, activefd);
		pairfree(user_reply);
		authfree(authreq);
		return -1;
	}

	/*
	 *	Validate the user
	 */
	calc_digest(pw_digest, authreq);
	userpass[0] = 0;

	result = rad_check_password(authreq, activefd, user_check,
		namepair, user_reply, pw_digest, reply_msg, userpass);

	if (result > 0) {
		/* Failed - no reply. */
		authfree(authreq);
		pairfree(user_reply);
		return -1;
	}

	if (result == -2) {
		/* Auth-Type = Reject, add reply message */
		if ((reply_item = pairfind(user_reply,
		     PW_REPLY_MESSAGE)) != NULL)
			strNcpy(reply_msg, reply_item->strvalue,
				sizeof(reply_msg));
		cause = " reject";
	}

	if (result == 0) {
		/* Password OK, now check expiration too */
		result = check_expiration(user_check, reply_msg);
		cause = " expired";
	}

	if (result < 0) {

		if (result == -7){ 
			strcpy(reply_msg,
				"\r\nYour Login is expired \r\n");
			log(L_ERR,"Login: [%s] expired (%s) ",
				namepair->strvalue, auth_name(authreq, 1));
		} else
		if (result == -9){
			strcpy(reply_msg,  
				"\r\nYour Login is no active \r\n");
			log(L_ERR,"Login: [%s] no activated (%s) ",
				namepair->strvalue, auth_name(authreq, 1));
		} else
		if (result == -10){
			strcpy(reply_msg,
				"\r\nYour Deposit is Negative \r\n");
			log(L_ERR,"Login: [%s], Deposit is Negative : (%s) ",
				namepair->strvalue, auth_name(authreq, 1));
		} else
		if (result == -6){
			strcpy(reply_msg,
				"\r\nYour Login is blocked \r\n"); 
			log(L_ERR,"Login: [%s] blocked (%s) ",
				namepair->strvalue, auth_name(authreq, 1));
		} else
		if (result == -5){
   			log(L_ERR,"Login: [%s] reject, black number (%s) ",
				namepair->strvalue, auth_name(authreq, 1));
		} else
		if (result == -8){ 
			log(L_ERR,"Login: [%s] phone number denied (%s) ",
				namepair->strvalue, auth_name(authreq, 1));
		}

		/*
		 *	Failed to validate the user.
		 */
		rad_send_reply(PW_AUTHENTICATION_REJECT, authreq,
			user_reply, reply_msg, activefd);
		if (log_auth) {
			char *pass = userpass[0] ? userpass : "<NO-PASSWORD>";
			log(L_AUTH,
				"Login incorrect: [%s%s%s] (%s)%s",
				namepair->strvalue,
				log_auth_badpass ? "/" : "",
				log_auth_badpass ? pass : "",
				auth_name(authreq, 1),
				cause);
		}
/*
		pairfree(user_reply);
		authfree(authreq);
		return -1;
*/
	}

	
	if (result >= 0 &&
	   (check_item = pairfind(user_check, PW_SIMULTANEOUS_USE)) != NULL) {
#if 0 /* DEBUG */
		VALUE_PAIR *tmp;
		char ipno[32];
#endif
		/*
		 *	User authenticated O.K. Now we have to check
		 *	for the Simultaneous-Use parameter.
		 */
		if ((r = rad_check_multi(namepair->strvalue, authreq->request,
		    user_reply, check_item->lvalue)) != 0) {

			if (check_item->lvalue > 1) {
				sprintf(reply_msg,
		"\r\nYou are already logged in %d times  - access denied\r\n\n",
					(int)check_item->lvalue);
			} else {
				strcpy(reply_msg,
		"\r\nYou are already logged in - access denied\r\n\n");
			}
			rad_send_reply(PW_AUTHENTICATION_REJECT, authreq,
				user_reply, reply_msg, activefd);
		log(L_ERR, "Multiple logins: [%s] (%s) max. %d%s",
				namepair->strvalue,
				auth_name(authreq, 1),
				check_item->lvalue,
				r == 2 ? " [MPP attempt]" : "");
			result = -1;
#if 0 /* DEBUG */
			ipno[0] = 0;
			if ((tmp = pairfind(authreq->request, PW_FRAMED_IP_ADDRESS)) != NULL)
				ipaddr2str(ipno, tmp->lvalue);
			log(L_INFO, "User asked for IP address [%s]", ipno);
#endif
		}
	}

#ifdef USEMYSQL


	if (result >= 0 &&
	   (check_item = pairfind(user_check, PW_GROUP)) != NULL) {
		expired = 0;
		octetslimit = 0;
	if ((sqlpwd = sql_getpwname(namepair->strvalue)) != NULL){ 
#ifdef CREATE_NAME_TABLE
		/* add name table */
		sql_create_name_table(sqlpwd->name);
#endif
		if (sqlpwd->ip[0] != NULL){ 
		     if (good_ipaddr(sqlpwd->ip) == NULL){
			if ((reply_item = pairfind(user_reply,
				PW_FRAMED_IP_ADDRESS)) != NULL){
				reply_item->lvalue = ipstr2long(sqlpwd->ip);
				reply_item->length = 4;
			}else{
				if (!(reply_item = malloc(sizeof(VALUE_PAIR)))){
					log(L_ERR|L_CONS, "no memory");
					exit(1);
				}
				memset(reply_item, 0, sizeof(VALUE_PAIR));
				reply_item->attribute = PW_FRAMED_IP_ADDRESS;
				strcpy(reply_item->name, "Framed-IP-Address");
				reply_item->type = PW_TYPE_IPADDR;
				reply_item->length = 4;
				reply_item->lvalue = ipstr2long(sqlpwd->ip);
				pairadd(&user_reply, reply_item);

			}
		     }else{
		 log(L_ERR,"Incorrect Framed-IP-Address (%s) for user %s ",
				sqlpwd->ip, sqlpwd->name);	
		     }
		} 

	/*	CALL_BACK		*/
/*		if (sqlpwd->callback[0] != NULL){
			pairfind(user_reply,PW_CALLBACK_NUMBER)
			
		} */

		/* ===============================
		 *	grp 0 unlimited	
		 */

		if (sqlpwd->type == 0){
			expired = sqlpwd->time - time(0);
		} else

		/* ===============================
		 * 	grp 1 hourgroup
		 */

		if (sqlpwd->type == 1){
		   if (hourgroup == 1)
			expired = money2time(sqlpwd->money, sqlpwd->igroup);
			if (expired <= 0) expired = -1;

		} else 

		/* ===============================
		 *	grp 3 traffic
		 */

		if (sqlpwd->type == 3){

		double traf_cost = 1048576 / group[sqlpwd->igroup].traf_cost;
		octetslimit = (u_int)(sqlpwd->money * traf_cost);
		/* octetsdir = group[sqlpwd->igroup].type_traf; */

		switch (group[sqlpwd->igroup].type_traf){
		   case 0: octetsdir = PW_OCTETS_DIRECTION_IN; break;
		   case 1: octetsdir = PW_OCTETS_DIRECTION_SUM; break;
		   case 2: octetsdir = PW_OCTETS_DIRECTION_MAX; break;
		   case 3: octetsdir = PW_OCTETS_DIRECTION_OUT; break;
		    default: break;
		}
		expired = -1; /* don't set session-timeout */

		} else 

		/* ===============================
		 *	grp 2 hourgroup + traffic
		 */

		if (sqlpwd->type == 2){
		//	hourgroupcmp(sqlpwd->money,sqlpwd->igroup); 
		} else	

		/* ===============================
		 *	grp 4 day_cost
		 */

		if (sqlpwd->type == 4){
		/*	time_now = time(0);
			localtime(&time_now); */
		}

	/* set session timeout if he is less then 2 days */
#if 0
		if ( (/*expired <= (60*60*24*4) &&*/ expired >= 0) || 
#endif
		if ( (/*expired <= (60*60*24*4) &&*/ expired >= 0) &&
			sqlpwd->type != 3){
			if ((reply_item = pairfind(user_reply,
				PW_SESSION_TIMEOUT)) != NULL){
				/*	if (reply_item->lvalue > expired) */
					reply_item->lvalue = expired;
			}else{
				if(!(reply_item = malloc(sizeof(VALUE_PAIR)))){
					log(L_ERR|L_CONS, "no memory");
					exit(1);
			     	}
				memset(reply_item, 0, sizeof(VALUE_PAIR));
				reply_item->attribute = PW_SESSION_TIMEOUT;
				strcpy(reply_item->name, "Session-Timeout");
				reply_item->type = PW_TYPE_INTEGER;
				reply_item->length = 4;
				reply_item->lvalue = expired;
				pairadd(&user_reply, reply_item);
			}
		} else {
#if 0
			/* expired timeout too small FIXME */
		sprintf(reply_msg,
			"\r\nSession-Time too small %d\n\n", expired);
		result = -1;
			rad_send_reply(PW_AUTHENTICATION_REJECT, authreq,
				user_reply, reply_msg, activefd);
			log(L_ERR, "Session-Time too small %d : [%s] (%s)",
				expired,
				namepair->strvalue,
				auth_name(authreq, 1));
#endif
		}	

		/* NONE: we use Attribute Session-Octets and Session-Dir
		 *       if we know about algo for calc traffic
		 *       if trafgroup == 0: traffic lineral func, we do
		 *       but if trafgroup == 1: traffic not lineral func! don't
		 */
		if (trafgroup == 0 && (sqlpwd->type == 2 || sqlpwd->type == 3))
		if ( octetslimit > 0){
			if ((reply_item = pairfind(user_reply,
				PW_SESSION_OCTETS_LIMIT)) != NULL){
					if (reply_item->lvalue > octetslimit)
					reply_item->lvalue = octetslimit;
			}else{
				if(!(reply_item = malloc(sizeof(VALUE_PAIR)))){
					log(L_ERR|L_CONS, "no memory");
					exit(1);
			     	}
				memset(reply_item, 0, sizeof(VALUE_PAIR));
				reply_item->attribute = PW_SESSION_OCTETS_LIMIT;
				strcpy(reply_item->name,"Session-Octets-Limit");
				reply_item->type = PW_TYPE_INTEGER;
				reply_item->length = 4;
				reply_item->lvalue = octetslimit;
				pairadd(&user_reply, reply_item);
			}

			/* set octets-direction (sum=0 in=1 out=2 max=3) */
			if ((reply_item = pairfind(user_reply,
				PW_OCTETS_DIRECTION)) != NULL){
					reply_item->lvalue = octetsdir;
			}else{
				if(!(reply_item = malloc(sizeof(VALUE_PAIR)))){
					log(L_ERR|L_CONS, "no memory");
					exit(1);
			     	}
				memset(reply_item, 0, sizeof(VALUE_PAIR));
				reply_item->attribute = PW_OCTETS_DIRECTION;
				strcpy(reply_item->name,"Octets-Direction");
				reply_item->type = PW_TYPE_INTEGER;
				reply_item->length = 4;
				reply_item->lvalue = octetsdir;
				pairadd(&user_reply, reply_item);
			}


		} else {
			/* < 0 ?! too small ?? FIXME */
		sprintf(reply_msg,
		"\r\nSession-Octets-Limit too small %d\n\n", octetslimit);
		result = -1;
			rad_send_reply(PW_AUTHENTICATION_REJECT, authreq,
				user_reply, reply_msg, activefd);
		log(L_ERR, "Session-Octets-Limit too small %d : [%s] (%s)",
			octetslimit,
			namepair->strvalue,
			auth_name(authreq, 1));
		}	

	    }else
		return 0; /* if sql_getpwname return NULL - error */
	} // PW_GROUP

	/* set acct_alive time if it is equal or less 0 drop */
		if ( acct_alive > 0){
			if ((reply_item = pairfind(user_reply,
				PW_ACCT_INTERIM_INTERVAL)) != NULL){
					reply_item->lvalue = acct_alive;
			}else{
				if(!(reply_item = malloc(sizeof(VALUE_PAIR)))){
					log(L_ERR|L_CONS, "no memory");
					exit(1);
			     	}
				memset(reply_item, 0, sizeof(VALUE_PAIR));
				reply_item->attribute = PW_ACCT_INTERIM_INTERVAL;
				strcpy(reply_item->name, "Acct-Interim-Interval");
				reply_item->type = PW_TYPE_INTEGER;
				reply_item->length = 4;
				reply_item->lvalue = acct_alive;
				pairadd(&user_reply, reply_item);
			}
		}
#endif

	if (result >= 0 &&
	   (check_item = pairfind(user_check, PW_LOGIN_TIME)) != NULL) {

		/*
		 *	Authentication is OK. Now see if this
		 *	user may login at this time of the day.
		 */
		r = timestr_match(check_item->strvalue, time(NULL));
		if (r < 0) {
			/*
			 *	User called outside allowed time interval.
			 */
			result = -1;
			strcpy(reply_msg,
			"You are calling outside your allowed timespan\r\n");
			rad_send_reply(PW_AUTHENTICATION_REJECT, authreq,
				user_reply, reply_msg, activefd);
			log(L_ERR, "Outside allowed timespan: [%s]"
				   " (%s) time allowed: %s",
					namepair->strvalue,
					auth_name(authreq, 1),
					check_item->strvalue);
		} else if (r > 0) {
			/*
			 *	User is allowed, but set Session-Timeout.
			 */
			if ((reply_item = pairfind(user_reply,
			    PW_SESSION_TIMEOUT)) != NULL) {
				if (reply_item->lvalue > r)
					reply_item->lvalue = r;
			} else {
				if (!(reply_item = malloc(sizeof(VALUE_PAIR)))){
					log(L_ERR|L_CONS, "no memory");
					exit(1);
				}
				memset(reply_item, 0, sizeof(VALUE_PAIR));
				reply_item->attribute = PW_SESSION_TIMEOUT;
				strcpy(reply_item->name, "Session-Timeout");
				reply_item->type = PW_TYPE_INTEGER;
				reply_item->length = 4;
				reply_item->lvalue = r;
				pairadd(&user_reply, reply_item);
			}
		}
	}

#ifdef USEMYSQL
/*
	if (result >= 0 &&
	   (check_item = pairfind(user_check, PW_TOTAL_TIME_LIMIT)) != NULL) {
		res_val = (check_item->lvalue * 16 - check_item->lvalue)*2;

		sprintf(buf_query,
		"SELECT SUM(time_on) FROM %s WHERE name = '%s'",
			acct_table,namepair->strvalue);
		mysql_run_query(buf_query);
	   if(total_table == 1){
		sprintf(buf_query,
		"SELECT SUM(sum_time_on) FROM total WHERE name = '%s'",
			acct_table,namepair->strvalue);
		mysql_run_query(buf_query);
	    }
		if(r??? <= ){

		sprintf(reply_msg,
			"\r\nYour login has total limit of %d min\n\n",
				checkitem->lvalue);
		result = -1;
			rad_send_reply(PW_AUTHENTICATION_REJECT, authreq,
				user_reply, reply_msg, activefd);
			log(L_ERR,
				"Login usage total limit of %d min : [%s] (%s)",
				check_item->lvalue,
				namepair->strvalue,
				auth_name(authreq,1));
		}else if (result >= 0){
			if ((reply_item = pairfind(user_reply,
			    PW_SESSION_TIMEOUT)) != NULL) {
				if (reply_item->lvalue > r)
					reply_item->lvalue = r;
			} else {
				if (!(reply_item = malloc(sizeof(VALUE_PAIR)))){
					log(L_ERR|L_CONS, "no memory");
					exit(1);
				}
				memset(reply_item, 0, sizeof(VALUE_PAIR));
				reply_item->attribute = PW_SESSION_TIMEOUT;
				strcpy(reply_item->name, "Session-Timeout");
				reply_item->type = PW_TYPE_INTEGER;
				reply_item->length = 4;
				reply_item->lvalue = r;
				pairadd(&user_reply, reply_item);
			}
		}	

	}

	if (result >= 0 &&
	   (check_item = pairfind(user_check, PW_DAILY_TIME_LIMIT)) != NULL) {
		res_val = (check_item->lvalue * 16 - check_item->lvalue)*2;
		sprintf(buf_query,
			"SELECT SUM(time_on) FROM %s WHERE name = '%s'"
			" AND TO_DAYS(start_time) = TO_DAYS(NOW())",
			acct_table,namepair->strvalue);
		mysql_res = mysql_run_query(buf_query);
	    if (mysql_res <= ){
		sprintf(reply_msg,
			"\r\nYour login has day limit of %d min\n\n",
			check_item->lvalue);
		result = -1;
			rad_send_reply(PW_AUTHENTICATION_REJECT, authreq,
				user_reply, reply_msg, activefd);
			log(L_ERR,
				"Login usage day limit of %d min : [%s] (%s)",
				check_item->lvalue,
				namepair->strvalue,
				auth_name(authreq,1));
		}else if (result >= 0){	
			if ((reply_item = pairfind(user_reply,
			    PW_SESSION_TIMEOUT)) != NULL) {
				if (reply_item->lvalue > r)
					reply_item->lvalue = r;
			} else {
				if (!(reply_item = malloc(sizeof(VALUE_PAIR)))){
					log(L_ERR|L_CONS, "no memory");
					exit(1);
				}
				memset(reply_item, 0, sizeof(VALUE_PAIR));
				reply_item->attribute = PW_SESSION_TIMEOUT;
				strcpy(reply_item->name, "Session-Timeout");
				reply_item->type = PW_TYPE_INTEGER;
				reply_item->length = 4;
				reply_item->lvalue = r;
				pairadd(&user_reply, reply_item);
			}

		     }

	}

	if (result >= 0 &&
	   (check_item = pairfind(user_check, PW_WEEKLY_TIME_LIMIT)) != NULL) {
		res_val = (check_item->lvalue * 16 - check_item->lvalue)*2;
		sprintf(buf_query,
			"SELECT SUM(time_on) FROM %s WHERE name = '%s'"
			" AND YEAR(start_time) = YEAR(NOW())"
			" AND WEEK(start_time,1) = WEEK(NOW(),1)",
			acct_table,namepair->strvalue);
		mysql_res = mysql_run_query(buf_query);
	    if (mysql_res <= res_var){
		sprintf(reply_msg,
			"\r\nYour login has week limit of %d min\n\n",
			check_item->lvalue);
		result = -1;
			rad_send_reply(PW_AUTHENTICATION_REJECT, authreq,
				user_reply, reply_msg, activefd);
			log(L_ERR,
				"Login usage week limit of %d min : [%s] (%s)",
				check_item->lvalue,
				namepair->strvalue,
				auth_name(authreq,1));

	    }else if (result >= 0){
			if ((reply_item = pairfind(user_reply,
			    PW_SESSION_TIMEOUT)) != NULL) {
				if (reply_item->lvalue > r)
					reply_item->lvalue = r;
			} else {
				if (!(reply_item = malloc(sizeof(VALUE_PAIR)))){
					log(L_ERR|L_CONS, "no memory");
					exit(1);
				}
				memset(reply_item, 0, sizeof(VALUE_PAIR));
				reply_item->attribute = PW_SESSION_TIMEOUT;
				strcpy(reply_item->name, "Session-Timeout");
				reply_item->type = PW_TYPE_INTEGER;
				reply_item->length = 4;
				reply_item->lvalue = r;
				pairadd(&user_reply, reply_item);
			}
	          }
	}

	if (result >= 0 &&
	   (check_item = pairfind(user_check, PW_MOUNTLY_TIME_LIMIT)) != NULL) {
		res_val = (check_item->lvalue * 16 - check_item->lvalue)*2;
		sprintf(buf_query,
			"SELECT SUM(time_on) FROM %s WHERE name = '%s'"
	" AND start_time >= CONCAT(YEAR(NOW()),'-',MONTH(NOW()),'-01 00:00:00')"
	" AND start_time <= ADDDATE(CONCAT(YEAR(NOW()),'-',MONTH(NOW()),'-01 00:00:00'), INTERVAL 1 MONTH)",
			acct_table,namepair->strvalue);
		mysql_res = mysql_run_query(buf_query);
	    if (mysql_res <= res_var){
		sprintf(reply_msg,
			"\r\nYour login has mountly limit of %d min\n\n",
			check_item->lvalue);
		result = -1;
			rad_send_reply(PW_AUTHENTICATION_REJECT, authreq,
				user_reply, reply_msg, activefd);
			log(L_ERR,
				"Login usage mountly limit of %d min : [%s] (%s)",
				check_item->lvalue,
				namepair->strvalue,
				auth_name(authreq,1));

	    }else if (result >= 0){
			if ((reply_item = pairfind(user_reply,
			    PW_SESSION_TIMEOUT)) != NULL) {
				if (reply_item->lvalue > r)
					reply_item->lvalue = r;
			} else {
				if (!(reply_item = malloc(sizeof(VALUE_PAIR)))){
					log(L_ERR|L_CONS, "no memory");
					exit(1);
				}
				memset(reply_item, 0, sizeof(VALUE_PAIR));
				reply_item->attribute = PW_SESSION_TIMEOUT;
				strcpy(reply_item->name, "Session-Timeout");
				reply_item->type = PW_TYPE_INTEGER;
				reply_item->length = 4;
				reply_item->lvalue = r;
				pairadd(&user_reply, reply_item);
			}

		  }
	}

	if (result >= 0 &&
	   (check_item = pairfind(user_check, PW_MOUNTLY_TRAFFIC_LIMIT)) != NULL) {
		res_val = check_item->lvalue * 400;
		sprintf(buf_query,
			"SELECT SUM(OutBytes) FROM %s WHERE name = '%s'"
	" AND start_time >= CONCAT(YEAR(NOW()),'-',MONTH(NOW()),'-01 00:00:00')"
	" AND start_time <= ADDDATE(CONCAT(YEAR(NOW()),'-',MONTH(NOW()),'-01 00:00:00'), INTERVAL 1 MONTH)",
			acct_table,namepair->strvalue);
		mysql_res = mysql_run_query(buf_query);
		
		if (mysql_res <= res_var){
		sprintf(reply_msg,
			"\r\nYour login has mountly traffic limit of %d Mb\n\n",
			check_item->lvalue);
		result = -1;
			rad_send_reply(PW_AUTHENTICATION_REJECT, authreq,
				user_reply, reply_msg, activefd);
			log(L_ERR,
			"Login usage mountly traffic limit of %d Mb : [%s] (%s)",
				check_item->lvalue,
				namepair->strvalue,
				auth_name(authreq,1));

		}else if (result >= 0){
		      }

		
	}

	if (result >= 0 &&
	   (check_item = pairfind(user_check, PW_TOTAL_TRAFFIC_LIMIT)) != NULL) {

	}

	if (result >= 0 &&
	   (check_item = pairfind(user_check, PW_PERIOD_LIMIT)) != NULL) {
		res_var = ?;
		sprintf(buf_query,
		"SELECT unix_timestamp(now())-unix_timestamp(min(start_time))"
		" FROM %s WHERE name = '%s'",
			acct_table,namepair->strvalue);
		mysql_res = mysql_run_query(buf_query);
	      if (mysql_res <= res_var){
		sprintf(reply_msg,
			"\r\nYour login has period limit of %d days\n\n",
			check_item->lvalue);
		result = -1;
			rad_send_reply(PW_AUTHENTICATION_REJECT, authreq,
				user_reply, reply_msg, activefd);
			log(L_ERR,
			"Login usage period limit of %d days : [%s] (%s)",
				check_item->lvalue,
				namepair->strvalue,
				auth_name(authreq,1));
	      }
	}
*/
#endif

	/*
	 *	Result should be >= 0 here - if not, we return.
	 */
	if (result < 0) {
		authfree(authreq);
		pairfree(user_check);
		pairfree(user_reply);
		return 0;
	}

	/*
	 *	See if we need to execute a program.
	 *	FIXME: somehow cache this info, and only execute the
	 *	program when we receive an Accounting-START packet.
	 *	Only at that time we know dynamic IP etc.
	 */
	exec_program = NULL;
	exec_wait = 0;
	if ((auth_item = pairfind(user_reply, PW_EXEC_PROGRAM)) != NULL) {
		exec_wait = 0;
		exec_program = strdup(auth_item->strvalue);
		pairdelete(&user_reply, PW_EXEC_PROGRAM);
	}
	if ((auth_item = pairfind(user_reply, PW_EXEC_PROGRAM_WAIT)) != NULL) {
		exec_wait = 1;
		exec_program = strdup(auth_item->strvalue);
		pairdelete(&user_reply, PW_EXEC_PROGRAM_WAIT);
	}

	/*
	 *	Hack - allow % expansion in certain value strings.
	 *	This is nice for certain Exec-Program programs.
	 */
	seen_callback_id = 0;
	if ((auth_item = pairfind(user_reply, PW_CALLBACK_ID)) != NULL) {
		seen_callback_id = 1;
		ptr = radius_xlate(auth_item->strvalue,
			authreq->request, user_reply);
		strncpy(auth_item->strvalue, ptr, sizeof(auth_item->strvalue));
		auth_item->strvalue[sizeof(auth_item->strvalue) - 1] = 0;
		auth_item->length = strlen(auth_item->strvalue);
	}


	/*
	 *	If we want to exec a program, but wait for it,
	 *	do it first before sending the reply.
	 */
	if (exec_program && exec_wait) {
		if (radius_exec_program(exec_program,
		    authreq->request, &user_reply, exec_wait, reply_msg) != 0) {
			/*
			 *	Error. radius_exec_program() returns -1 on
			 *	fork/exec errors, or >0 if the exec'ed program
			 *	had a non-zero exit status.
			 */
			if (reply_msg[0] == 0)
		strcpy(reply_msg, "\r\nAccess denied (external check failed).");
			rad_send_reply(PW_AUTHENTICATION_REJECT, authreq,
				user_reply, reply_msg, activefd);
			if (log_auth) {
				log(L_AUTH,
					"Login incorrect: [%s] (%s) "
					"(external check failed)",
					namepair->strvalue,
					auth_name(authreq, 1));
			}
			authfree(authreq);
			pairfree(user_check);
			pairfree(user_reply);
			return 0;
		}
	}

	/*
	 *	Delete "normal" A/V pairs when using callback.
	 *
	 *	FIXME: This is stupid. The portmaster should accept
	 *	these settings instead of insisting on using a
	 *	dialout location.
	 *
	 *	FIXME2: Move this into the above exec thingy?
	 *	(if you knew how I use the exec_wait, you'd understand).
	 */
	if (seen_callback_id) {
		pairdelete(&user_reply, PW_FRAMED_PROTOCOL);
		pairdelete(&user_reply, PW_FRAMED_IP_ADDRESS);
		pairdelete(&user_reply, PW_FRAMED_IP_NETMASK);
		pairdelete(&user_reply, PW_FRAMED_ROUTE);
		pairdelete(&user_reply, PW_FRAMED_MTU);
		pairdelete(&user_reply, PW_FRAMED_COMPRESSION);
		pairdelete(&user_reply, PW_FILTER_ID);
		pairdelete(&user_reply, PW_PORT_LIMIT);
		pairdelete(&user_reply, PW_CALLBACK_NUMBER);
	}

	/*
	 *	Filter Reply-Message value through radius_xlate
	 */
	if (reply_msg[0] == 0) {
		if ((reply_item = pairfind(user_reply,
		    PW_REPLY_MESSAGE)) != NULL) {
			ptr = radius_xlate(reply_item->strvalue,
				authreq->request, user_reply);
			strNcpy(reply_item->strvalue, ptr,
				sizeof(reply_item->strvalue));
			reply_item->strvalue[sizeof(reply_item->strvalue)-1]=0;
			reply_item->length = strlen(reply_item->strvalue);
		}
	}

	rad_send_reply(PW_AUTHENTICATION_ACK, authreq,
			user_reply, reply_msg, activefd);
	if (log_auth) {
#if 1 /* Hide the password for `miquels' :) */
		if (strcmp(namepair->strvalue, "miquels") == 0)
			strcpy(userpass, "guess");
#endif
		log(L_AUTH,
			"Login OK: [%s%s%s] (%s)",
			namepair->strvalue,
			log_auth_goodpass ? "/" : "",
			log_auth_goodpass ? userpass : "",
			auth_name(authreq, 0));
	}
	if (exec_program && !exec_wait) {
		/*
		 *	No need to check the exit status here.
		 */
		radius_exec_program(exec_program,
			authreq->request, &user_reply, exec_wait, NULL);
	}

	if (exec_program) free(exec_program);
	authfree(authreq);
	pairfree(user_check);
	pairfree(user_reply);

	return 0;
}

