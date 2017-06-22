/*
 * radclient.c	Read valuepairs from stdin, send them to a
 *		radius server, wait for reply.
 *
 *		Copyright 2001		Cistron Internet Services B.V.
 */
char radtest_rcsid[] =
"$Id: radclient.c,v 1.5 2001/11/13 12:55:14 miquels Exp $";

#include	<sys/types.h>
#include	<sys/socket.h>
#include	<sys/time.h>
#include	<netinet/in.h>

#include	<stdio.h>
#include	<time.h>
#include	<unistd.h>
#include	<netdb.h>
#include	<pwd.h>
#include	<stdlib.h>
#include	<errno.h>
#include	<ctype.h>

#include	"radiusd.h"
#include	"conf.h"

int		send_buffer[512];
int		recv_buffer[512];
char		vector[AUTH_VECTOR_LEN];
char		*secretkey;
char		*progname;

/*
 *	Receive and print the result.
 */
int result_recv(UINT4 host, u_short udp_port, char *buffer, int length)
{
	AUTH_HDR	*auth;
	int		totallen;
	char		reply_digest[AUTH_VECTOR_LEN];
	char		calc_digest[AUTH_VECTOR_LEN];
	int		secretlen;
	AUTH_REQ	*authreq;
	VALUE_PAIR	*vp;

	auth = (AUTH_HDR *)buffer;
	totallen = ntohs(auth->length);

	if(totallen != length) {
		printf("Received invalid reply length from server (want %d/ got %d)\n", totallen, length);
		exit(1);
	}

	/* Verify the reply digest */
	memcpy(reply_digest, auth->vector, AUTH_VECTOR_LEN);
	memcpy(auth->vector, vector, AUTH_VECTOR_LEN);
	secretlen = strlen(secretkey);
	memcpy(buffer + length, secretkey, secretlen);
	md5_calc(calc_digest, (char *)auth, length + secretlen);

	if(memcmp(reply_digest, calc_digest, AUTH_VECTOR_LEN) != 0) {
		printf("Warning: Received invalid reply digest from server\n");
	}

	debug_flag = 1;
	authreq = radrecv(htonl(host), udp_port, buffer, length);
	debug_flag = 2;
	for(vp = authreq->request; vp; vp = vp->next) {
		if (vp->flags.encrypt) {
			/* Note: vector is the one from the request we sent */
			decrypt_attr(secretkey, vector, vp);
		}
	}
	debug_flag = 0;

	if (auth->code == PW_AUTHENTICATION_REJECT) {
		printf("Access denied.\n");
		return -1;
	}
	return 0;
}




/*
 *	Print usage message and exit.
 */
void usage(void)
{
	fprintf(stderr, "Usage: %s [-d raddb] [-f file] [-i source_ip] server acct|auth secret\n",
		progname);
	exit(1);
}


int getport(char *svc, int def)
{
	struct	servent		*svp;
	int			n;

	if ((n = atoi(svc)) > 0) return n;

	svp = getservbyname (svc, "udp");
	return svp ? ntohs((u_short)svp->s_port) : def;
}

/*
 *	Calculate the encoded chap password.
 */
static int rad_chapencode(char *in, char *out, int id, char *vector)
{
	char    buf[256];
	int     len, maxlen;

	maxlen = sizeof(buf) - 18;
	if (strlen(in) > maxlen)
		in[maxlen] = 0;
	len = strlen(in);
	buf[0] = id;
	memcpy(buf + 1, in, len);
	memcpy(buf + 1 + len, vector, 16);
	len += 17;

	out[0] = id;
	md5_calc(out + 1, buf, len);

	return 17;
}

int main(int argc, char **argv)
{
	AUTH_HDR		*auth;
	VALUE_PAIR		*req, *vp, *tp;
	FILE			*fp;
	UINT4			local_ip = 0;
	struct	sockaddr	salocal;
	struct	sockaddr	saremote;
	struct	sockaddr_in	*sin;
	struct timeval		tv;
	fd_set			readfds;
	char			buf[256];
	char			*filename;
	char			*p;
	char			*radius_dir;
	UINT4			server_ip;
	int			port;
	int			code;
	int			sockfd;
	int			salen;
	int			result;
	int			length;
	int			i;
	int			chapid;

	if ((progname = strrchr(argv[0], '/')) == NULL)
		progname = argv[0];
	else
		progname++;
	radius_dir = RADIUS_DIR;
	filename = NULL;
	port = 0;

	/*
	 *	Process the options.
	 */
	while ((i = getopt(argc, argv, "d:f:i:h")) != EOF) switch (i) {
		case 'd':
			radius_dir = optarg;
			break;
		case 'f':
			filename = optarg;
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
	if (argc < 3) usage();
	secretkey = argv[3] ? argv[3] : "";

	/*
	 *	Initialize dictionary.
	 */
	if (dict_init(radius_dir, NULL) < 0)
		exit(1);

        /*
         *      Strip port from hostname if needed.
         */
        if ((p = strchr(argv[1], ':')) != NULL) {
                *p++ = 0;
                port = getport(p, 0);
        }

        /*
         *      See what kind of request we want to send.
         */
        if (strcmp(argv[2], "auth") == 0) {
                if (port == 0) port = getport("radius", PW_AUTH_UDP_PORT);
                code = PW_AUTHENTICATION_REQUEST;
        } else if (strcmp(argv[2], "acct") == 0) {
                if (port == 0) port = getport("radacct", PW_ACCT_UDP_PORT);
                code = PW_ACCOUNTING_REQUEST;
        } else if (isdigit(argv[2][0])) {
                if (port == 0) port = getport("radius", PW_AUTH_UDP_PORT);
                if (port == 0) port = PW_AUTH_UDP_PORT;
                code = atoi(argv[2]);
        } else {
                usage();
        }

        /*
         *      Resolve hostname.
         */
	server_ip = get_ipaddr(argv[1]);
	if (server_ip == 0) {
		fprintf(stderr, "%s: unknown host %s\n", progname, argv[1]);
		exit(1);
	}

	/*
	 *	Read valuepairs.
	 */
	if (filename && strcmp(filename, "-") != 0) {
		if ((fp = fopen(filename, "r")) == NULL) {
			fprintf(stderr, "%s: ", progname);
			perror(filename);
			exit(1);
		}
	} else
		fp = stdin;
	req = NULL;
	while(fgets(buf, sizeof(buf), fp) != NULL) {
		vp = NULL;
		if (userparse(buf, &vp, 1) != 0) {
			fprintf(stderr, "%s: cannot parse %s",
				progname, buf);
			exit(1);
		}
		pairadd(&req, vp);
	}

	/*
	 *	Open a socket.
	 */
	sockfd = socket (AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		perror ("socket");
		exit(1);
	}
	if (local_ip) {
		int		n;
		char		b[32];

		sin = (struct sockaddr_in *) &salocal;
       		memset (sin, 0, sizeof (salocal));
		sin->sin_family = AF_INET;
		sin->sin_addr.s_addr = htonl(local_ip);

		for (n = 1024; n < 65000; n++) {
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
	sin->sin_port = htons(port);

	/*
	 *	Set up AUTH structure.
	 */
	memset(send_buffer, 0, sizeof(send_buffer));
	auth = (AUTH_HDR *)send_buffer;
	auth->code = code;
	if (code == PW_AUTHENTICATION_REQUEST ||
	    code == PW_STATUS_SERVER)
		random_vector(auth->vector);
	auth->id = getpid() & 255;

	/*
	 *	Find password and encode it.
	 */
	for (vp = req; vp; vp = vp->next) {
		if (vp->attribute == PW_PASSWORD) {
			vp->length = rad_pwencode(vp->strvalue,
				vp->strvalue, secretkey, auth->vector);
		}
		if (vp->attribute == PW_CHAP_PASSWORD) {
			if ((tp = pairfind(req, PW_CHAP_CHALLENGE)) != NULL)
				p = tp->strvalue;
			else
				p = auth->vector;
			chapid = (getppid() ^ getpid()) & 0xff;
			vp->length = rad_chapencode(vp->strvalue,
				vp->strvalue, chapid, p);
		}
	}

	/*
	 *	Build final radius packet.
	 */
	length = rad_build_packet(auth, sizeof(send_buffer),
		req, NULL, secretkey, auth->vector);
	memcpy(vector, auth->vector, sizeof(vector));

	/*
	 *	Send the request we've built.
	 */
	printf("Sending request to server %s, port %d.\n", argv[1], port);
	for (i = 0; i < 10; i++) {
		if (i > 0) printf("Re-sending request.\n");
		sendto(sockfd, (char *)auth, length, 0,
			&saremote, sizeof(struct sockaddr_in));

		tv.tv_sec = 3;
		tv.tv_usec = 0;
		FD_ZERO(&readfds);
		FD_SET(sockfd, &readfds);
		if (select(sockfd + 1, &readfds, NULL, NULL, &tv) == 0)
			continue;
		salen = sizeof (saremote);
		result = recvfrom (sockfd, (char *)recv_buffer,
			sizeof(recv_buffer), 0, &saremote, &salen);
		if (result >= 0)
			break;
		sleep(tv.tv_sec);
	}
	if (result > 0 && i < 10) {
		result_recv(sin->sin_addr.s_addr, sin->sin_port,
			(char *)recv_buffer, result);
		exit(0);
	}
	printf("No answer.\n");
	close(sockfd);
	exit(1);
}
