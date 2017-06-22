/*
 * pam.c	Functions to access the PAM library. This was taken
 *		from the hacks that miguel a.l. paraz <map@iphil.net>
 *		did on radiusd-cistron-1.5.3 and migrated to a
 *		separate file.
 *
 *		That, in fact, was again based on the original stuff
 *		from Jeph Blaize <jab@kiva.net> done in May 1997.
 *
 * 		Copyright 1997 Jeph Blaize <jab@kiva.net>
 * 		Copyright 1998 cdent@kiva.net
 *
 */
char *pam_rcsid = "$Id: pam.c,v 1.6 2001/07/24 18:54:33 miquels Exp $";

#ifdef PAM

#include	<sys/types.h>
#include	<sys/socket.h>
#include	<sys/time.h>
#include	<netinet/in.h>

#include	<stdio.h>
#include	<netdb.h>
#include	<pwd.h>
#include	<time.h>
#include	<ctype.h>

#include	<security/pam_appl.h>

#include	"radiusd.h"

/*************************************************************************
 *
 *	Function: PAM_conv
 *
 *	Purpose: Dialogue between RADIUS and PAM modules.
 *
 * jab - stolen from pop3d
 *************************************************************************/

/*
 *	Since PAM may use openlog(), we set syslog_open to zero
 *	after every PAM operation so that log.c::do_log() will
 *	call openlog() for the main server again.
 */
extern int syslog_open;

static char *PAM_username;
static char *PAM_password;
static char *PAM_message;
static int PAM_error =0;

#define GET_MEM \
	if (reply) \
		realloc(reply, size); \
	else \
		reply = malloc(size); \
	if (!reply) return PAM_CONV_ERR; \
	size += sizeof(struct pam_response)
#define COPY_STRING(s) \
	(s) ? strdup(s) : NULL

static int PAM_conv (int num_msg,
                     const struct pam_message **msg,
                     struct pam_response **resp,
                     void *appdata_ptr)
{
	struct pam_response *reply = NULL;
	int size = sizeof(struct pam_response);
	int count = 0, replies = 0;

	for (count = 0; count < num_msg; count++) {
		switch (msg[count]->msg_style) {
			case PAM_PROMPT_ECHO_ON:
				GET_MEM;
				reply[replies].resp_retcode = PAM_SUCCESS;
				reply[replies++].resp =
					COPY_STRING(PAM_username);
				/* PAM frees resp */
				break;
			case PAM_PROMPT_ECHO_OFF:
				GET_MEM;
				reply[replies].resp_retcode = PAM_SUCCESS;
				reply[replies++].resp =
					COPY_STRING(PAM_password);
				/* PAM frees resp */
				break;
			case PAM_TEXT_INFO:
				DEBUG("PAM_TEXT_INFO: '%s'", msg[count]->msg);
				if (PAM_message) free(PAM_message);
				PAM_message = COPY_STRING(msg[count]->msg);
				break;
			case PAM_ERROR_MSG:
				DEBUG("PAM_ERROR_MSG: '%s'", msg[count]->msg);
			default:
				/* Must be an error of some sort... */
				free (reply);
				PAM_error = 1;
				return PAM_CONV_ERR;
		}
	}

	if (reply) *resp = reply;
	return PAM_SUCCESS;
}

struct pam_conv conv = {
	PAM_conv,
	NULL
};

/*************************************************************************
 *
 *	Function: pam_pass
 *
 *	Purpose: Check the users password against the standard UNIX
 *		 password table + PAM.
 *
 * jab start 19970529
 *************************************************************************/

/* cjd 19980706
 * 
 * for most flexibility, passing a pamauth type to this function
 * allows you to have multiple authentication types (i.e. multiple
 * files associated with radius in /etc/pam.d)
 */
int pam_pass(char *name, char *passwd, const char *pamauth, char *reply_msg)
{
	pam_handle_t *pamh=NULL;
	int retval;

	PAM_username = name;
	PAM_password = passwd;
	PAM_message = NULL;

	DEBUG("pam_pass: using pamauth string <%s> for pam.conf lookup",
		pamauth);
	retval = pam_start(pamauth, name, &conv, &pamh);
	syslog_open = 0;

	if (retval == PAM_SUCCESS) {
		DEBUG("pam_pass: function pam_start succeeded for <%s>", name);
		retval = pam_authenticate(pamh, 0);
	}

	if (retval == PAM_SUCCESS) {
		DEBUG("pam_pass: function pam_authenticate succeeded for <%s>",
			name);
		retval = pam_acct_mgmt(pamh, 0);
		syslog_open = 0;
	}

	if (PAM_message) {
		strNcpy(reply_msg, PAM_message, AUTH_STRING_LEN);
		free(PAM_message);
		PAM_message = 0;
	}

	if (retval == PAM_SUCCESS) {
		DEBUG("pam_pass: function pam_acct_mgmt succeeded for <%s>",
			name);
		pam_end(pamh, 0);
		syslog_open = 0;
		return 0;
	} else {
		DEBUG("pam_pass: PAM FAILED for <%s> failed", name);
		pam_end(pamh, 0);
		syslog_open = 0;
		return -1;
	}
}

#endif /* PAM */

