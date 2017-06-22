/*
 * radwho	Show who is logged in on the terminal servers.
 *		Can also be installed as fingerd on the UNIX
 *		machine RADIUS runs on.
 *
 *		Copyright 1997-2001	Cistron Internet Services B.V.
 *
 * Author:	miquels@cistron.nl
 * Patch:	22-Apr-1999	promera@cistron.nl
 *		Added support for raw, comma seperated output
 */
char *radwho_rcsid =
"$Id: radwho.c,v 1.10 2001/05/07 20:33:45 miquels Exp $";

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <time.h>
#include <ctype.h>
#include <netinet/in.h>
#include "sysdep.h"

#include "radutmp.h"
#include "radiusd.h"

/*
 *	FIXME: put in header file.
 */
#define SYS_FINGER "/usr/bin/finger"
#define FINGER_DIR "/usr/local/lib/finger"

char *progname = "radwho";

/*
 *	Header above output and format.
 */
char *hdr1 = 
"Login      Name              What  TTY  When      From      Location";
char *ufmt1 = "%-10.10s %-17.17s %-5.5s %-4.4s %-9.9s %-9.9s %-.16s%s";
char *ufmt1r = "%s,%s,%s,%s,%s,%s,%s%s";
char *rfmt1 = "%-10.10s %-17.17s %-5.5s %s%-3d %-9.9s %-9.9s %-.19s%s";
char *rfmt1r = "%s,%s,%s,%s%d,%s,%s,%s%s";

char *hdr2 = 
"Login      Port    What   When          From       Location";
char *ufmt2 = "%-10.10s %-6.6d %-7.7s %-13.13s %-10.10s %-.16s%s";
char *ufmt2r = "%s,%d,%s,%s,%s,%s%s";
char *rfmt2 = "%-10.10s %s%-5d  %-6.6s %-13.13s %-10.10s %-.28s%s";
char *rfmt2r = "%s,%s%d,%s,%s,%s,%s%s";

char *eol = "\n";
NAS *naslist;
int showname = -1;
int showptype = 0;
int showcid = 0;

/*
 *	Safe popen. Ugh.
 */
FILE *safe_popen(char *cmd, char *mode)
{
	char		*p;
	char		buf[1024];

	/*
	 *	Change all suspect characters into a space.
	 */
	strncpy(buf, cmd, sizeof(buf));
	buf[sizeof(buf) - 1] = 0;
	for (p = buf; *p; p++) {
		if (isalnum(*p))
			continue;
		if (strchr("@%-_ \t+:,./", *p) == NULL)
			*p = ' ';
	}

	return popen(buf, mode);
}

/*
 *	Read the naslist file.
 */
static NAS *my_read_naslist_file(char *file)
{
	FILE	*fp;
	char	buffer[256];
	char	hostnm[128];
	char	shortnm[32];
	char	nastype[32];
	int	lineno = 0;
	NAS	*cl = NULL;
	NAS	*c;

	if ((fp = fopen(file, "r")) == NULL) {
		fprintf(stderr, "cannot open %s\n", file);
		return NULL;
	}
	while(fgets(buffer, 256, fp) != NULL) {
		lineno++;
		if (buffer[0] == '#' || buffer[0] == '\n')
			continue;
		shortnm[0] = 0;
		if (sscanf(buffer, "%127s%31s%31s", hostnm, shortnm, nastype) < 2) {
			fprintf(stderr, "%s[%d]: syntax error\n", file, lineno);
			continue;
		}
		if ((c = malloc(sizeof(NAS))) == NULL) {
			fprintf(stderr, "%s[%d]: out of memory\n",
				file, lineno);
			return NULL;
		}

		c->ipaddr = get_ipaddr(hostnm);
		strNcpy(c->nastype, nastype, sizeof(c->nastype));
		strNcpy(c->shortname, shortnm, sizeof(c->shortname));
		strNcpy(c->longname, ip_hostname(c->ipaddr), sizeof(c->longname));

		c->next = cl;
		cl = c;
	}
	fclose(fp);

	return cl;
}


/*
 *	Print a file from FINGER_DIR. If the file is executable,
 *	execute it instead. Return 0 if succesfull.
 */
int ffile(char *arg)
{
	FILE *fp;
	char fn[1024];
	int p = 0;
	char *s;

	sprintf(fn, "%s/%.32s", FINGER_DIR, arg);
	if (access(fn, X_OK) == 0) {
		p = 1;
		sprintf(fn, "exec %s/%.32s 2>&1", FINGER_DIR, arg);
		fp = safe_popen(fn, "r");
	} else fp = fopen(fn, "r");

	if (fp == NULL) return -1;

	while(fgets(fn, 1024, fp)) {
		if ((s = strchr(fn, '\n')) != NULL)
			*s = 0;
		fprintf(stdout, "%s\r\n", fn);
	}
	if (p)
		pclose(fp);
	else
		fclose(fp);
	fflush(stdout);
	return 0;
}


/*
 *	Execute the system finger and translate LF to CRLF.
 */
void sys_finger(char *l)
{
	FILE *fp;
	char fn[1024];
	char *p;

	if (ffile(l) == 0)
		exit(0);

	sprintf(fn, "exec %s %s", SYS_FINGER, l);
	if ((fp = safe_popen(fn, "r")) == NULL) {
		printf("popen: %s\r\n", strerror(errno));
		exit(1);
	}

	while(fgets(fn, 1024, fp)) {
		if ((p = strchr(fn, '\n')) != NULL)
			*p = 0;
		fprintf(stdout, "%s\r\n", fn);
	}
	pclose(fp);
	exit(0);
}


/*
 *	Get fullname of a user.
 */
char *fullname(char *login)
{
	struct passwd *pwd;
	char *s;

	if ((pwd = getpwnam(login)) != NULL) {
		if ((s = strchr(pwd->pw_gecos, ',')) != NULL) *s = 0;
		return pwd->pw_gecos;
	}
	return login;
}

/*
 *	Return protocol type.
 */
char *proto(int id, int porttype)
{
	static char buf[8];

	if (showptype) {
		if (!strchr("ASITX", porttype))
			porttype = ' ';
		if (id == 'S')
			sprintf(buf, "SLP %c", porttype);
		else if (id == 'P')
			sprintf(buf, "PPP %c", porttype);
		else
			sprintf(buf, "shl %c", porttype);
		return buf;
	}
	if (id == 'S') return "SLIP";
	if (id == 'P') return "PPP";
	return "shell";
}

/*
 *	Return a time in the form day hh:mm
 */
char *dotime(time_t t)
{
	char *s = ctime(&t);

	if (showname) {
		strncpy(s + 4, s + 11, 5);
		s[9] = 0;
	} else {
		strncpy(s + 4, s + 8, 8);
		s[12] = 0;
	}

	return s;
}

#if 0 /*UNUSED*/
/*
 *	See how long a tty has been idle.
 */
char *idletime(char *line)
{
	char tty[16];
	static char tmp[8];
	time_t t;
	struct stat st;
	int hr, min, days;

	if (line[0] == '/')
		strcpy(tty, "/dev/");
	else
		tty[0] = 0;
	strncat(tty, line, 10);
	tty[15] = 0;

	tmp[0] = 0;
	if (stat(tty, &st) == 0) {
		time(&t);
		t -= st.st_mtime;
		if (t >= 60) {
			min = (t / 60);
			hr = min / 24;
			days = hr / 24;
			min %= 60;
			hr %= 24;
			if (days > 0)
				sprintf(tmp, "%dd", days);
			else
				sprintf(tmp, "%2d:%02d", hr, min);
		}
	}
	return tmp;
}
#endif

/*
 *	Shorten tty name.
 */
char *ttyshort(char *tty)
{
	static char tmp[16];

	if (tty[0] == '/') tty += 5;

	if (strncmp(tty, "tty", 3) == 0) {
		if (tty[3] >= '0' && tty[3] <= '9')
			sprintf(tmp, "v%.14s", tty + 3);
		else
			sprintf(tmp, "%.15s", tty + 3);
		return tmp;
	}
	if (strncmp(tty, "vc", 2) == 0) {
		sprintf(tmp, "v%.14s", tty + 2);
		return tmp;
	}
	if (strncmp(tty, "cu", 2) == 0) {
		return tmp + 2;
	}
	return "??";
}


/*
 *	Find name of NAS
 */
char *nasname(UINT4 ipaddr)
{
	NAS *cl;

	for(cl = naslist; cl; cl = cl->next)
		if (cl->ipaddr == ipaddr)
			break;
	if (cl == NULL) {
		static char buffer[32];
		ipaddr2str(buffer, ipaddr);
		return buffer;
	}
	if (cl->shortname[0])
		return cl->shortname;
	return cl->longname;
}


/*
 *	Print address of NAS.
 */
char *hostname(UINT4 ipaddr)
{
	if (ipaddr == 0 || ipaddr == (UINT4)-1 || ipaddr == (UINT4)-2)
		return "";
	return ip_hostname(ntohl(ipaddr));
}


/*
 *	Print usage message and exit.
 */
void usage(void)
{
	fprintf(stderr, "Usage: radwho [-lhfnsipcr] [-d dir] [-u file]\n");
	fprintf(stderr, "       -l: show local (shell) users too\n");
	fprintf(stderr, "       -h: hide shell users from radius\n");
	fprintf(stderr, "       -f: give fingerd output\n");
	fprintf(stderr, "       -n: no full name\n");
	fprintf(stderr, "       -s: show full name\n");
	fprintf(stderr, "       -i: show session ID\n");
	fprintf(stderr, "       -p: show port type\n");
	fprintf(stderr, "       -c: show caller ID, if available\n");
	fprintf(stderr, "       -r: output as raw data\n");
	fprintf(stderr, "       -d: path to radius config files\n");
	fprintf(stderr, "       -u: path to radutmp file to use\n");
	exit(1);
}


/*
 *	Main program, either pmwho or fingerd.
 */
int main(int argc, char **argv)
{
	FILE *fp;
	struct radutmp rt;
	struct utmp ut;
	int hdrdone = 0;
	char inbuf[128];
	char myname[128];
	char session_id[16];
	char *radutmp = RADUTMP;
	char *radius_dir = RADIUS_DIR;
	int fingerd = 0;
	int showlocal = 0;
	int hideshell = 0;
	int showsid = 0;
	int rawoutput = 0;
	char *p, *q, *portind;
	int c, portno;

	while((c = getopt(argc, argv, "Dd:flhnsipcru:")) != EOF) switch(c) {
		case 'D':
			use_dns = 0;
			break;

		case 'd':
			radius_dir = optarg;
			break;
		case 'f':
			fingerd++;
			showname = 0;
			break;
		case 'l':
			showlocal = 1;
			break;
		case 'h':
			hideshell = 1;
			break;
		case 'n':
			showname = 0;
			break;
		case 's':
			showname = 1;
			break;
		case 'i':
			showsid = 1;
			break;
		case 'p':
			showptype = 1;
			break;
		case 'c':
			showcid = 1;
			showname = 1;
			break;
		case 'r':
			rawoutput = 1;
			break;
		case 'u':
			radutmp = optarg;
			break;
		default:
			usage();
			break;
	}

	/*
	 *	Read the "naslist" file.
	 */
	sprintf(inbuf, "%s/%s", radius_dir, RADIUS_NASLIST);
	if ((naslist = my_read_naslist_file(inbuf)) == NULL)
		exit(1);

	/*
	 *	See if we are "fingerd".
	 */
	if (strstr(argv[0], "fingerd")) {
		fingerd++;
		eol = "\r\n";
		if (showname < 0) showname = 0;
	}
	if (showname < 0) showname = 1;

	if (fingerd) {
		/*
		 *	Read first line of the input.
		 */
		fgets(inbuf, 128, stdin);
		p = inbuf;
		while(*p == ' ' || *p == '\t') p++;
		if (*p == '/' && *(p + 1)) p += 2;
		while(*p == ' ' || *p == '\t') p++;
		for(q = p; *q && *q != '\r' && *q != '\n'; q++)
			;
		*q = 0;

		/*
		 *	See if we fingered a specific user.
		 */
		ffile("header");
		if (*p) sys_finger(p);
	}

	if (showlocal && (fp = fopen(UTMP_FILE, "r"))) {
		if (rawoutput == 0)
		{	
			fputs(showname ? hdr1 : hdr2, stdout);
			fputs(eol, stdout);
		}
		hdrdone = 1;

		/*
		 *	Show the logged in UNIX users.
		 */
		gethostname(myname, 128);
		while(fread(&ut, sizeof(ut), 1, fp) == 1) {
#ifdef USER_PROCESS
			if (ut.ut_user[0] && ut.ut_line[0] &&
				ut.ut_type == USER_PROCESS) {
#else
			if (ut.ut_user[0] && ut.ut_line[0]) {
#endif
			   if (showname)
				printf((rawoutput == 0? ufmt1: ufmt1r),
					ut.ut_name,
					fullname(ut.ut_name),
					"shell",
					ttyshort(ut.ut_line),
#ifdef __svr4__
					dotime(ut.ut_xtime),
#else
					dotime(ut.ut_time),
#endif
					ut.ut_host,
					myname, eol);
			    else
				printf((rawoutput==0? ufmt2:ufmt2r),
					ut.ut_name,
					ttyshort(ut.ut_line),
					"shell",
#ifdef __svr4__
					dotime(ut.ut_xtime),
#else
					dotime(ut.ut_time),
#endif
					ut.ut_host,
					myname, eol);
			}
		}
		fclose(fp);
	}

	/*
	 *	Show the users logged in on the terminal server(s).
	 */
	if ((fp = fopen(radutmp, "r")) == NULL)
		return 0;

	if (!hdrdone) {
		fputs(showname ? hdr1 : hdr2, stdout);
		fputs(eol, stdout);
	}

	while(fread(&rt, sizeof(rt), 1, fp) == 1) {
		if (rt.type == P_LOGIN) {
			/*
			 *	We don't show shell users if we are
			 *	fingerd, as we have done that above.
			 */
			if (hideshell && !strchr("PCS", rt.proto))
				continue;

			sprintf(session_id, "%.8s", rt.session_id);

			if (!rawoutput && rt.nas_port > (showname ? 999 : 99999)) {
				portind = ">";
				portno = (showname ? 999 : 99999);
			} else {
				portind = "S";
				portno = rt.nas_port;
			}
			if (showname)
			    printf((rawoutput == 0? rfmt1: rfmt1r),
				rt.login,
				showcid ? rt.caller_id :
				(showsid? session_id : fullname(rt.login)),
				proto(rt.proto, rt.porttype),
				portind, portno,
				dotime(rt.time),
				nasname(ntohl(rt.nas_address)),
				hostname(rt.framed_address), eol);
			else
			    printf((rawoutput == 0? rfmt2: rfmt2r),
				rt.login,
				portind, portno,
				proto(rt.proto, rt.porttype),
				dotime(rt.time),
				nasname(ntohl(rt.nas_address)),
				hostname(rt.framed_address), eol);
		}
	}
	fflush(stdout);
	fflush(stderr);
	fclose(fp);

	return 0;
}

