/*
 * radutmp.h	Definitions for the Radius UTMP file.
 *
 *		Copyright 1998 Cistron Internet Services B.V.
 *
 * Version:	$Id: radutmp.h,v 1.2 2000/12/01 16:13:23 miquels Exp $
 */

#ifndef _RADUTMP_H
#define _RADUTMP_H

/*
 *      Types of connection.
 */
#ifndef P_UNKNOWN
#  define P_UNKNOWN       0
#  define P_LOCAL         'L'
#  define P_RLOGIN        'R'
#  define P_SLIP          'S'
#  define P_CSLIP         'C'
#  define P_PPP           'P'
#  define P_AUTOPPP       'A'
#  define P_TELNET        'E'
#  define P_TCPCLEAR      'T'
#  define P_TCPLOGIN      'U'
#  define P_CONSOLE       '!'
#  define P_SHELL         'X'
#endif

#define P_IDLE		0
#define P_LOGIN		1

#define RUT_NAMESIZE 32

struct radutmp {
  char login[32];		/* Loginname */
				/* FIXME: extend to 48 or 64 bytes */
  int  nas_port;		/* Port on the terminal server (32 bits). */
  char session_id[8];		/* Radius session ID (first 8 bytes at least)*/
				/* FIXME: extend to 16 or 32 bytes */
  unsigned int nas_address;	/* IP of portmaster. */
  unsigned int framed_address;	/* SLIP/PPP address or login-host. */
  int proto;			/* Protocol. */
  time_t time;			/* Time entry was last updated. */
  time_t delay;			/* Delay time of request */
  int type;			/* Type of entry (login/logout) */
  char porttype;		/* Porttype (I=ISDN A=Async T=Async-ISDN */
  char res1,res2,res3;		/* Fills up to one int */
  char caller_id[16];		/* Calling-Station-ID */
  char reserved[12];		/* 3 ints reserved */
};

#endif /* _RADUTMP_H */