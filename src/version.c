/*
 *
 * version.c	Compiled in version info.
 *
 *		Copyright 1998-2001	Cistron Internet Services B.V.
 *		Copyright 2002		Cistron IP B.V.
 */

char version_rcsid[] =
"$Id: version.c,v 1.35 2002/02/06 15:23:16 miquels Exp $";

#include        <sys/types.h>
#include	<stdio.h>
#include	"radiusd.h"

#define		VERSION		"1.6.6 06-Feb-2002"

/*
 *	Display the revision number for this program
 */
void version(void)
{

	fprintf(stderr, "%s: RADIUS version %s\n", progname, VERSION);
	fprintf(stderr, "Compilation flags: ");

	/*
	 *	Conditional features. Set by the Makefile,
	 *	or by conf.h
	 */
#if defined(USE_DBM)
	fprintf(stderr," USE_DBM");
#endif
#if defined(USE_NDBM)
	fprintf(stderr," USE_NDBM");
#endif
#if defined(USE_GDBM)
	fprintf(stderr," USE_GDBM");
#endif
#if defined(USE_DB1)
	fprintf(stderr," USE_DB1");
#endif
#if defined(USE_DB2)
	fprintf(stderr," USE_DB2");
#endif
#if defined(USE_DB3)
	fprintf(stderr," USE_DB3");
#endif
#if defined(USE_SYSLOG)
	fprintf(stderr, " USE_SYSLOG");
#endif
#if defined(NOSHADOW)
	fprintf(stderr," NOSHADOW");
#endif
#if defined(OSFC2)
	fprintf(stderr," OSFC2");
#endif
#if defined(OSFSIA)
	fprintf(stderr," OSFSIA");
#endif
#if defined(NT_DOMAIN_HACK)
	fprintf(stderr," NT_DOMAIN_HACK");
#endif
#if defined(SPECIALIX_JETSTREAM_HACK)
	fprintf(stderr," SPECIALIX_JETSTREAM_HACK");
#endif
#if defined(NOCASE)
	fprintf(stderr," NOCASE");
#endif
#if defined(ATTRIB_NMC)
	fprintf(stderr, " ATTRIB_NMC");
#endif
#if defined(COMPAT_1543)
	fprintf(stderr, " COMPAT_1543");
#endif
#if defined(MERIT_TAG_FORMAT)
	fprintf(stderr, " MERIT_TAG_FORMAT");
#endif

	/*
	 *	This are the system defined macros. Pretty
	 *	useless, really, but since the Livingston
	 *	server did this too we'll just follow their lead.
	 */
#if defined(__alpha)
	fprintf(stderr," __alpha");
#endif
#if defined(__osf__)
	fprintf(stderr," __osf__");
#endif
#if defined(aix)
	fprintf(stderr," aix");
#endif
#if defined(bsdi)
	fprintf(stderr," bsdi");
#endif
#if defined(sun)
	fprintf(stderr," sun");
#endif
#if defined(sys5)
	fprintf(stderr," sys5");
#endif
#if defined(unixware)
	fprintf(stderr," unixware");
#endif
#if defined(__linux__)
	fprintf(stderr," linux");
#endif
#if defined(M_UNIX)
	fprintf(stderr," M_UNIX");
#endif
	fprintf(stderr,"\n");
	exit(0);
}
