/*
 * radzap	Zap a user from the radutmp and radwtmp file.
 *
 *		Copyright 1997-2001	Cistron Internet Services B.V.
 *
 */
char *radzap_version =
"$Id: radzap.c,v 1.11 2001/11/26 21:52:00 miquels Exp $";

#include	<sys/types.h>
#include	<sys/time.h>
#include	<sys/file.h>

#include	<stdio.h>
#include	<stdlib.h>
#include	<time.h>
#include	<unistd.h>
#include	<errno.h>
#include	<stdarg.h>
#include	<netinet/in.h>

#include	"radiusd.h"
#include	"radutmp.h"

char *progname = "radzap";
int verbose = 0;
NAS *naslist;
#define LOCK_LEN sizeof(struct radutmp)

char *radutmp_path = RADUTMP;
char *radwtmp_path = RADWTMP;
char *radius_dir = RADIUS_DIR;

/*
 *	Zap a user, or all users on a NAS, from the radutmp file.
 *	FIXME: duplicated from acct.c
 */
int radzap(UINT4 nasaddr, int port, char *user, time_t t, int dowtmp)
{
	struct radutmp	u;
#if 0
	struct utmp	wt;
#endif
	FILE		*fp;
	int		fd;
	UINT4		netaddr;
	char		buf[16];

	if (t == 0) time(&t);
	if (dowtmp)
		fp = fopen(radwtmp_path, "a");
	else
		fp = NULL;
	netaddr = htonl(nasaddr);

	if ((fd = open(radutmp_path, O_RDWR|O_CREAT, 0644)) >= 0) {
		int r;

		/*
		 *	Lock the utmp file, prefer lockf() over flock().
		 */
#if defined(F_LOCK) && !defined(BSD)
		(void)lockf(fd, F_LOCK, LOCK_LEN);
#else
		(void)flock(fd, LOCK_EX);
#endif
	 	/*
		 *	Find the entry for this NAS / portno combination.
		 */
		r = 0;
		while (read(fd, &u, sizeof(u)) == sizeof(u)) {
			if (u.login[0] == 0) continue;
			if (verbose) {
				ipaddr2str(buf, ntohl(u.nas_address));
				printf("user %s, nas %s, port %d - ",
					u.login, buf, u.nas_port);
			}
			if (((nasaddr != 0 && netaddr != u.nas_address) ||
			      (port >= 0   && port    != u.nas_port) ||
			      (user != NULL && strcmp(u.login, user) != 0) ||
			       u.type != P_LOGIN)) {
				if (verbose) printf("no match\n");
				continue;
			}
			if (verbose) printf("MATCH\n");

			/*
			 *	Match. Zap it.
			 */
			if (lseek(fd, -(off_t)sizeof(u), SEEK_CUR) < 0) {
				fprintf(stderr,
				"Accounting: radzap: negative lseek!\n");
				lseek(fd, (off_t)0, SEEK_SET);
			}
			u.type = P_IDLE;
			u.time = t;
			write(fd, &u, sizeof(u));

#if 0 /* FIXME */
			/*
			 *	Add a logout entry to the wtmp file.
			 */
			if (fp != NULL)  {
				make_wtmp(&u, &wt, PW_STATUS_STOP);
				fwrite(&wt, sizeof(wt), 1, fp);
			}
#endif
		}
		close(fd);
	}
	if (fp) fclose(fp);

	return 0;
}


/*
 *	Read the nas file.
 *	FIXME: duplicated from files.c
 */
int read_naslist_file(char *file)
{
	FILE	*fp;
	char	buffer[256];
	char	hostnm[128];
	char	shortnm[32];
	char	nastype[32];
	int	lineno = 0;
	NAS	*c;

	/*nas_free(naslist);*/

	if ((fp = fopen(file, "r")) == NULL) {
		perror(file);
		return -1;
	}
	while(fgets(buffer, 256, fp) != NULL) {
		lineno++;
		if (buffer[0] == '#' || buffer[0] == '\n')
			continue;
		nastype[0] = 0;
		if (sscanf(buffer, "%127s%31s%31s", hostnm, shortnm, nastype) < 2) {
			fprintf(stderr, "%s[%d]: syntax error\n", file, lineno);
			continue;
		}
		if ((c = malloc(sizeof(NAS))) == NULL) {
			fprintf(stderr, "%s[%d]: out of memory\n",
				file, lineno);
			return -1;
		}

		c->ipaddr = get_ipaddr(hostnm);
		strNcpy(c->nastype, nastype, sizeof(c->nastype));
		strNcpy(c->shortname, shortnm, sizeof(c->shortname));
		strNcpy(c->longname, ip_hostname(c->ipaddr), sizeof(c->longname));

		c->next = naslist;
		naslist = c;
	}
	fclose(fp);

	return 0;
}


UINT4 findnas(char *nasname)
{
	NAS *cl;

	for(cl = naslist; cl; cl = cl->next) {
		if (strcmp(nasname, cl->shortname) == 0 ||
		    strcmp(nasname, cl->longname) == 0)
			return cl->ipaddr;
	}

	return 0;
}


void usage(void)
{
	fprintf(stderr, "Usage: radzap [-v] nas [port] [user]\n");
	fprintf(stderr, "       radzap is only an admin tool to clean the radutmp file!\n");
	exit(1);
}

/*
 *	Zap a user from the radutmp and radwtmp file.
 */
int main(int argc, char **argv)
{
	int	nas_port = -1;
	int	c, dowtmp;
	char	*user = NULL;
	char	*s;
	UINT4	ip;
	time_t	t;
	char	buf[256];

	while ((c = getopt(argc, argv, "vd:W:u:")) != -1) switch(c) {
		case 'v':
			verbose++;
			break;
		case 'd':
			radius_dir = optarg;
			break;
		case 'W':
			radwtmp_path = optarg;
			break;
		case 'u':
			radutmp_path = optarg;
			break;
		default:
			usage();
			break;
	}
	argv += (optind - 1);
	argc -= (optind - 1);

	if (argc < 2 || argc > 4)
		usage();

	if (argc > 2) {
		s = argv[2];
		if (*s == 's' || *s == 'S') s++;
		nas_port = strtoul(s, NULL, 10);
	}
	if (argc > 3) user = argv[3];

	/*
	 *	Read the "naslist" file.
	 */
	sprintf(buf, "%s/%s", radius_dir, RADIUS_NASLIST);
	if (read_naslist_file(buf) < 0)
		exit(1);

	/*
	 *	Find the IP address of the terminal server.
	 */
	if ((ip = findnas(argv[1])) == 0 && argv[1][0] != 0) {
		if ((ip = get_ipaddr(argv[1])) == 0) {
			fprintf(stderr, "%s: host not found.\n", argv[1]);
			exit(1);
		}
	}

	ipaddr2str(buf, ip);
	printf("radzap: zapping user %s on nas %s, ",
		(user && user[0]) ? user : "<any>", buf);
	if (nas_port >= 0)
		printf("port %d\n", nas_port);
	else
		printf("port <any>\n");

	t = time(NULL);
	dowtmp = (access(radwtmp_path, F_OK) == 0);
	radzap(ip, nas_port, user, t, dowtmp);

	return 0;
}
