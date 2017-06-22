/*
 * radrelay.c	This program tails a detail logfile, reads the log
 *		entries, forwards them to a remote radius server,
 *		and moves the processed records to another file.
 *
 *		Used to replicate accounting records to one (central)
 *		server - works even if remote server has extended
 *		downtime, and/or if this program is restarted.
 *		
 *		Copyright 2001	Cistron Internet Services B.V.
 */
char radrelay_rcsid[] =
"$Id: radrelay.c,v 1.12 2001/11/26 21:51:59 miquels Exp $";

#include	<sys/types.h>
#include	<sys/socket.h>
#include	<sys/time.h>
#include	<sys/stat.h>
#include	<netinet/in.h>

#include	<stdio.h>
#include	<fcntl.h>
#include	<time.h>
#include	<unistd.h>
#include	<netdb.h>
#include	<stdlib.h>
#include	<signal.h>
#include	<errno.h>

#include	"radiusd.h"
#include	"conf.h"

char	*progname = "radrelay";
#ifdef RADRELAY_PID
char	*pid_file = RADRELAY_PID;
#endif

/*
 *	Possible states for request->state
 */
#define		STATE_EMPTY	0
#define		STATE_BUSY1	1
#define		STATE_BUSY2	2
#define		STATE_FULL	3

/*
 *	Possible states for the loop() function.
 */
#define		STATE_RUN	0
#define		STATE_BACKLOG	1
#define		STATE_WAIT	2
#define		STATE_SHUTDOWN	3

#define		NR_SLOTS	64

/*
 *	A request.
 */
struct request {
	VALUE_PAIR	*pairs;				/* value pairs */
	int		id;				/* ID */
	char		vector[AUTH_VECTOR_LEN];	/* Vector */
	int		state;				/* REQ_* state */
	time_t		retrans;			/* when to retrans */
	time_t		timestamp;			/* orig recv time */
	UINT4		client_ip;			/* Client-IP-Addr */
};
struct request slots[NR_SLOTS];
int request_head;
int radius_id;
int got_sigterm = 0;

void *xalloc(int x)
{
	void	*r;

	if ((r = malloc(x)) == NULL) {
		fprintf(stderr, "Out of memory\n");
		exit(1);
	}
	memset(r, 0, x);
	return r;
}
#define talloc(x) (x *)xalloc(sizeof(x))


void sigterm_handler(int sig)
{
	signal(sig, sigterm_handler);
	got_sigterm = 1;
}


/*
 *	Sleep a number of milli seconds
 */
void ms_sleep(int msec)
{
	struct timeval	tv;

	tv.tv_sec  = (msec / 1000);
	tv.tv_usec = (msec % 1000) * 1000;
	select(0, NULL, NULL, NULL, &tv);
}


/*
 *	Find secret for this client in the clients file.
 */
RADCLIENT *find_client(char *file, char *server_name, UINT4 server_ip)
{
	RADCLIENT	*r;
	FILE		*fp;
	UINT4		ip;
	char		buf[256];
	char		hostnm[128];
	char		secret[32];
	char		shortnm[32];

	r = NULL;

	if ((fp = fopen(file, "r")) == NULL)
		return NULL;

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		if (buf[0] == '\n' || buf[0] == '#')
			continue;
		shortnm[0] = 0;
		if (sscanf(buf, "%127s%31s%31s",hostnm,secret,shortnm) < 2)
			continue;
		ip = get_ipaddr(hostnm);
		if ((ip && ip == server_ip) ||
		     strcasecmp(hostnm, server_name) == 0 ||
		     strcasecmp(shortnm, server_name) == 0) {
			r = talloc(RADCLIENT);
			r->ipaddr = ip;
			strcpy(r->longname, hostnm);
			strcpy(r->shortname, shortnm);
			strNcpy(r->secret, secret, sizeof(r->secret));
			break;
		}
	}
	fclose(fp);
	return r;
}


/*
 *	Does this (remotely) look like "Tue Jan 23 06:55:48 2001" ?
 */
int isdateline(char *d)
{
	int	y;

	return sscanf(d, "%*s %*s %*d %*d:%*d:%*d %d", &y);
}


/*
 *	Lock / unlock file.
 */
int lockfd(int fd, int lock)
{
	int		r;

#if defined(F_LOCK) && !defined(BSD)
	r = lockf(fd, lock ? F_LOCK : F_ULOCK, 0);
#else
	r = flock(fd, lock ? LOCK_EX : LOCK_UN);
#endif
	return r;
}


/*
 *	Read one request from the detail file.
 *	Note that the file is locked during the read, and that
 *	we return *with the file locked* if we reach end-of-file.
 *
 *	STATE_EMPTY:	Slot is empty.
 *	STATE_BUSY1:	Looking for start of a detail record (timestamp)
 *	STATE_BUSY2:	Reading the A/V pairs of a detail record.
 *	STATE_FULL:	Read the complete record.
 *
 */
int read_one(FILE *fp, struct request *req)
{
	VALUE_PAIR	*vp;
	char		*s;
	char		buf[256];
	char		key[32], val[32];
	int		skip;

	/* Never happens */
	if (req->state == STATE_FULL)
		return 0;

	if (req->state == STATE_EMPTY) {
		req->state = STATE_BUSY1;
	}

	lockfd(fileno(fp), 1);
	while ((s = fgets(buf, sizeof(buf), fp)) != NULL) {
		if (req->state == STATE_BUSY1) {
			if (isdateline(buf)) {
				req->state = STATE_BUSY2;
			}
		} else if (req->state == STATE_BUSY2) {
			if (buf[0] != ' ' && buf[0] != '\t') {
				req->state = STATE_FULL;
				break;
			}
			/*
			 *	Found A/V pair, but we skip non-protocol
			 *	values.
			 */
			skip = 0;
			if (sscanf(buf, "%31s = %31s", key, val) == 2) {
				if (!strcasecmp(key, "Timestamp")) {
					req->timestamp = atoi(val);
					skip++;
				} else
				if (!strcasecmp(key, "Client-IP-Address")) {
					req->client_ip = get_ipaddr(val);
					skip++;
				} else
				if (!strcasecmp(key, "Request-Authenticator"))
					skip++;
			}
			if (!skip) {
				vp = NULL;
				if (userparse(buf, &vp, 1) >= 0 &&
				    (vp->attribute < 256 ||
				     vp->attribute > 65535) &&
				    vp->attribute != PW_VENDOR_SPECIFIC) {
					pairadd(&(req->pairs), vp);
					vp = NULL;
				}
				if (vp) pairfree(vp);
			}
		}
	}
	clearerr(fp);

	if (req->state == STATE_FULL) {
		/*
		 *	w00 - we just completed reading a record in full.
		 */
		if (req->timestamp == 0)
			req->timestamp = time(NULL);
		if ((vp = pairfind(req->pairs, PW_ACCT_DELAY_TIME)) != NULL) {
			req->timestamp -= vp->lvalue;
			vp->lvalue = 0;
		}
		req->id = radius_id & 0xff;
		radius_id++;
	}

	if (s == NULL) {
		/*
		 *	Apparently we reached end of file. If we didn't
		 *	partially read a record, we let the caller know
		 *	we're at end of file.
		 */
		if (req->state == STATE_BUSY1) {
			req->state = STATE_EMPTY;
		}
		if (req->state == STATE_EMPTY || req->state == STATE_FULL)
			return EOF;
	}

	lockfd(fileno(fp), 0);

	return 0;
}

/*
 *	Receive answers from the remote server.
 */
int do_recv(int sockfd, struct sockaddr_in *sin)
{
	AUTH_HDR		*a;
	AUTH_REQ		*req;
	struct sockaddr		saremote;
	struct sockaddr_in	*sar;
	struct request		*r;
	int			salen;
	int			rbuf[512];
	int			l, i;

	/*
	 *	Receive packet and validate it's lenght.
	 */
	salen = sizeof(saremote);
	sar = (struct sockaddr_in *)&saremote;
	l = recvfrom(sockfd, (char *)rbuf, sizeof(rbuf), 0, &saremote, &salen);
	if (l <= 0) return -1;
	a = (AUTH_HDR *)rbuf;
	if (l < ntohs(a->length)) return -1;

	/*printf("Received packet, len=%d, packlen=%d, code=%d, id=%d\n",
		l, ntohs(a->length), a->code, a->id);*/

	/*
	 *	Must be an accounting response.
	 *	FIXME: check if this is the right server!
	 */
	if (a->code != PW_ACCOUNTING_RESPONSE)
		return -1;

	/*
	 *	Decode packet into radius attributes.
	 */
	req = radrecv(sar->sin_addr.s_addr, ntohs(sar->sin_port),
		(char *)rbuf, l);
	if (req == NULL) return -1;

	/*
	 *	Now find it in the outstanding requests.
	 */
	for (i = 0; i < NR_SLOTS; i++) {
		r = slots + i;
		if (r->state == STATE_FULL && r->id == req->id) {
			/*
			 *	Got it. Clear slot.
			 *	FIXME: check reponse digest ?
			 */
			pairfree(r->pairs);
			memset(r, 0, sizeof(struct request));
			break;
		}
	}

	authfree(req);

	return 0;
}

/*
 *	Send accounting packet to remote server.
 */
int do_send(int sockfd, struct sockaddr_in *sin,char *secret,struct request *r)
{
	VALUE_PAIR	*vp;
	AUTH_HDR	*auth;
	time_t		now;
	int		sbuf[512];
	int		len;

	/*
	 *	Prevent loops.
	 */
	if (r->client_ip == ntohl(sin->sin_addr.s_addr)) {
		pairfree(r->pairs);
		memset(r, 0, sizeof(struct request));
		return 0;
	}

	/*
	 *	Has the time come for this packet ?
	 */
	now = time(NULL);
	if (r->retrans > now)
		return 0;
	r->retrans = now + 3;

	/*
	 *	Find the Acct-Delay-Time attribute. If it's
	 *	not there, add one.
	 */
	if ((vp = pairfind(r->pairs, PW_ACCT_DELAY_TIME)) == NULL) {
		vp = paircreate(PW_ACCT_DELAY_TIME, PW_TYPE_INTEGER);
		pairadd(&(r->pairs), vp);
	}
	vp->lvalue = (now - r->timestamp);

	/*
	 *	Rebuild the entire packet every time from
	 *	scratch - the signature changed because
	 *	Acct-Delay-Time changed.
	 */
	auth = (AUTH_HDR *)sbuf;
	memset((char *)auth, 0, sizeof(AUTH_HDR));
	auth->code = PW_ACCOUNTING_REQUEST;
	auth->id = r->id;
	len = rad_build_packet(auth, sizeof(sbuf), r->pairs, NULL, secret,
			       auth->vector);
	memcpy(r->vector, auth->vector, AUTH_VECTOR_LEN);

	/*printf("sending packet len=%d id=%d code=%d\n",
		len, auth->id, auth->code);*/

	/*
	 *	And send it out into the world.
	 */
	sendto(sockfd, (char *)auth, len, 0,
		(struct sockaddr *)sin, sizeof(struct sockaddr_in));

	return 1;
}

/*
 *	Rename a file, then recreate the old file with the
 *	same permissions and zero size.
 */
int detail_move(char *from, char *to)
{
	struct stat	st;
	int		n;
	int		oldmask;

	if (stat(from, &st) < 0)
		return -1;
	if (rename(from, to) < 0)
		return -1;

	oldmask = umask(0);
	if ((n = open(from, O_CREAT|O_RDWR, st.st_mode)) >= 0)
		close(n);
	umask(oldmask);

	return 0;
}


/*
 *	Open detail file, collect records, send them to the
 *	remote accounting server, yadda yadda yadda.
 *
 *	STATE_RUN:	Reading from detail file, sending to server.
 *	STATE_BACKLOG:	Reading from the detail.work file, for example
 *			after a crash or restart. Sending to server.
 *	STATE_WAIT:	Reached end-of-file, renamed detail to
 *			detail.work, waiting for all outstanding
 *			requests to be answered.
 */
void loop(int sockfd, struct sockaddr_in *sin, char *secret, char *detail)
{
	FILE			*fp = NULL;
	struct request		*r;
	struct timeval		tv;
	fd_set			readfds;
	char			work[256];
	time_t			now, last_rename = 0;
	int			i, n;
	int			state = STATE_RUN;

	strNcpy(work, detail, sizeof(work) - 6);
	strcat(work, ".work");

	while(1) {

		if (got_sigterm) state = STATE_SHUTDOWN;

		/*
		 *	Open detail file - if needed, and if we can.
		 */
		if (state == STATE_RUN && fp == NULL) {
			if ((fp = fopen(work, "r+")) != NULL)
				state = STATE_BACKLOG;
			else
				fp = fopen(detail, "r+");
		}

		/*
		 *	If "request_head" points to a free or not-completely-
		 *	filled slot, we can read from the detail file.
		 */
		r = &slots[request_head];
		if (fp && state != STATE_WAIT && state != STATE_SHUTDOWN &&
		    r->state != STATE_FULL) {
			if (read_one(fp, r) == EOF) do {

				/*
				 *	End of file. See if the file has
				 *	any size, and if we renamed less
				 *	than 10 seconds ago or not.
				 */
				now = time(NULL);
				if (ftell(fp) == 0 || now < last_rename + 10) {
					lockfd(fileno(fp), 0);
					break;
				}
				last_rename = now;

				/*
				 *	We rename the file
				 *	to <file>.work and create an
				 *	empty new file.
				 */
				if (state == STATE_RUN &&
				    detail_move(detail, work) == 0)
					state = STATE_WAIT;
				else if (state == STATE_BACKLOG)
					state = STATE_WAIT;
				lockfd(fileno(fp), 0);
			} while(0);
			if (r->state == STATE_FULL)
				request_head = (request_head + 1) % NR_SLOTS;
		}

		/*
		 *	Perhaps we can receive something.
		 */
		tv.tv_sec = 0;
		tv.tv_usec = 25000;
		FD_ZERO(&readfds);
		FD_SET(sockfd, &readfds);
		n = 0;
		while (select(sockfd + 1, &readfds, NULL, NULL, &tv) > 0) {
			do_recv(sockfd, sin);
			if (n++ >= NR_SLOTS) break;
		}

		/*
		 *	If we're in STATE_WAIT and all slots are
		 *	finally empty, we can copy the <detail>.work file
		 *	to the definitive detail file and resume.
		 */
		if (state == STATE_WAIT || state == STATE_SHUTDOWN) {
			for (i = 0; i < NR_SLOTS; i++)
				if (slots[i].state != STATE_EMPTY)
					break;
			if (i == NR_SLOTS) {
				if (fp) fclose(fp);
				fp = NULL;
				unlink(work);
				if (state == STATE_SHUTDOWN) {
#ifdef RADRELAY_PID
					(void) unlink(pid_file);
#endif
					exit(0);
				}
				state = STATE_RUN;
			}
		}

		/*
		 *	See if there's anything to send.
		 */
		for (i = 0; i < NR_SLOTS; i++)
			if (slots[i].state == STATE_FULL) {
				n += do_send(sockfd, sin, secret, &slots[i]);
				if (n > 1) ms_sleep(25);
				if (n > NR_SLOTS / 2)
					break;
			}
	}
}

void usage(void)
{
	fprintf(stderr, "Usage: %s [-P pidfile ] [-a accounting_dir] [-i local_ip] [-f] [-s secret]\n\tremote-server detailfile\n", progname);
	exit(1);
}

int main(int argc, char **argv)
{
#ifdef RADRELAY_PID
	FILE			*fp;
#endif
	RADCLIENT		*r;
	struct sockaddr		salocal;
	struct sockaddr		saremote;
	struct sockaddr_in	*sin;
	struct servent		*svp;
	char			tmp[256];
	char			*server_secret = NULL;
	char			*server_name;
	char			*detail;
	char			*p;
	char			*radius_dir = RADIUS_DIR;
	UINT4			server_ip;
	UINT4			local_ip = 0;
	int			server_port = 0;
	int			c;
	int			sockfd;
	int			dontfork = 0;

	/*
	 *	Make sure there are stdin/stdout/stderr fds.
	 */
	while ((c = open("/dev/null", O_RDWR)) < 3 && c >= 0)
		;
	if (c >= 3) close(c);

	/*
	 *	Process the options.
	 */
	while ((c = getopt(argc, argv, "P:a:d:fhi:s:")) != EOF) switch(c) {
#ifdef RADRELAY_PID
		case 'P':
			pid_file = optarg;
			break;
#endif
		case 'a':
			if (chdir(optarg) < 0) {
				fprintf(stderr, "%s: ", progname);
				perror(optarg);
				exit(1);
			}
			break;
		case 'd':
			radius_dir = optarg;
			break;
		case 'f':
			dontfork = 1;
			break;
		case 's':
			server_secret = optarg;
			break;
		case 'i':
			if ((local_ip = get_ipaddr(optarg)) == 0) {
				fprintf(stderr, "%s: unknown host %s\n",
					progname, optarg);
				exit(1);
			}
			break;
		case 'h':
		default:
			usage();
			break;
	}
	argc -= (optind - 1);
	argv += (optind - 1);
	if (argc != 3) usage();

	/*
	 *	Find servers IP address and shared secret.
	 */
	server_name = argv[1];
	if ((p = strrchr(server_name, ':')) != NULL) {
		*p++ = 0;
		server_port = atoi(p);
	}
	if (server_port == 0) {
		svp = getservbyname ("radacct", "udp");
		server_port = svp ? ntohs(svp->s_port) : PW_ACCT_UDP_PORT;
	}
	server_ip   = get_ipaddr(argv[1]);
	sprintf(tmp, "%.200s/%.50s", radius_dir, RADIUS_CLIENTS);
	r = find_client(tmp, server_name, server_ip);
	if (r != NULL) {
		if (r->ipaddr) server_ip = r->ipaddr;
		server_secret = r->secret;
	}
	if (server_ip == 0) {
		fprintf(stderr, "%s: %s: unknown host\n",
			progname, server_name);
		exit(1);
	}
	if (server_secret == NULL || server_secret[0] == 0) {
		fprintf(stderr, "%s: no secret available for server %s\n",
			progname, server_name);
		exit(1);
	}
	detail  = argv[2];

	/*
	 *	Initialize dictionary.
	 */
	if (dict_init(radius_dir, NULL) < 0)
		exit(1);

	/*
	 *	Open a socket to the remote server.
	 */
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		fprintf(stderr, "%s: ", progname);
		perror("socket");
		exit(1);
	}
	if (local_ip) {
		int		n;
		char		b[32];

		sin = (struct sockaddr_in *) &salocal;
		memset (sin, 0, sizeof (salocal));
		sin->sin_family = AF_INET;
		sin->sin_addr.s_addr = htonl(local_ip);

		for (n = 1024 + getpid() % 9999; n < 65000; n++) {
			sin->sin_port = htons(n);
			if (bind(sockfd, &salocal, sizeof(salocal)) == 0)
				break;
			if (errno != EADDRINUSE) {
				n = 65000;
				break;
			}
		}
		if (n == 65000) {
			ipaddr2str(b, local_ip);
			fprintf(stderr, "%s: can't bind to local address %s\n",
				progname, b);
			exit(1);
		}
	}

	sin = (struct sockaddr_in *) &saremote;
	memset (sin, 0, sizeof (saremote));
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = htonl(server_ip);
	sin->sin_port = htons(server_port);

	signal(SIGTERM, sigterm_handler);

	if (!dontfork) {
		if (fork() != 0)
			exit(0);
		close(0);
		close(1);
		close(2);
		(void)open("/dev/null", O_RDWR);
		dup(0);
		dup(0);
		signal(SIGHUP,  SIG_IGN);
		signal(SIGINT,  SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
#if defined(__linux__) || defined(__svr4__) || \
    defined(__hpux__)  || defined(__FreeBSD__)
		setsid();
#endif
#ifdef RADRELAY_PID
		if ((fp = fopen(pid_file, "w")) != NULL) {
			fprintf(fp, "%d\n", (int)getpid());
			fclose(fp);
		}
#endif
	}

	/*
	 *	Call main processing loop.
	 */
	loop(sockfd, sin, server_secret, detail);

	return 0;
}
