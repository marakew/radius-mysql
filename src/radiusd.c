/*
 * radiusd.c	Main loop of the server.
 *
 *		Copyright 1996-2001	Cistron Internet Services B.V.
 *		Copyright 2002		Cistron IP B.V.
 */

/* don't look here for the version, run radiusd -v or look in version.c */
char radiusd_rcsid[] =
"$Id: radiusd.c,v 1.24 2002/01/23 12:42:07 miquels Exp $";

#include	<sys/types.h>
#include	<sys/socket.h>
#include	<sys/time.h>
#include	<sys/file.h>
#include	<netinet/in.h>

#include	<stdio.h>
#include	<stdlib.h>
#include	<netdb.h>
#include	<fcntl.h>
#include	<time.h>
#include	<ctype.h>
#include	<unistd.h>
#include	<signal.h>
#include	<errno.h>
#include	<sys/wait.h>
#if defined(__linux__)
#  include	<malloc.h>
#  include	<getopt.h>
#endif

#include	"radiusd.h"

/*
 *	Global variables.
 */
char			*progname;
char			*radius_dir;
extern char		*radlog_dir;
char			*radutmp_path;
char			*radwtmp_path;
#ifdef USE_SYSLOG
extern int		syslog_facility;
#endif
char			*radacct_dir;
int			log_stripped_names;
int 			cache_passwd = 0;
int			use_dbm = 0;
int			use_wtmp = 1;
UINT4			myip = 0;
UINT4			warning_seconds;
char			*log_auth_detail;
int			log_auth = 0;
int			log_auth_badpass  = 0;
int			log_auth_goodpass  = 0;
int			auth_port;
int			acct_port;

/*
 *	Make sure recv_buffer is aligned properly.
 */
static int		i_recv_buffer[1024];
static char		*recv_buffer = (char *)i_recv_buffer;

static int		got_chld = 0;
static int		request_list_busy = 0;
static int		sockfd;
static int		acctfd;
static int		spawn_flag;
static int		acct_pid;
static int		radius_pid;
static int		need_reload = 0;
static time_t		start_time;
static AUTH_REQ		*first_request;


#if !defined(__linux__) && !defined(__GNU_LIBRARY__)
extern int	errno;
#endif

typedef		int (*FUNP)(AUTH_REQ *, int);

static int	config_init(void);
static void	usage(void);

static void	sig_fatal (int);
static void	sig_hup (int);

static int	radrespond (AUTH_REQ *, int);
static void	rad_spawn_child (AUTH_REQ *, int, FUNP);

/*
 *	Read config files.
 */
static void reread_config(int reload)
{
	int res = 0;
	int pid = getpid();

	if (!reload) {
		log(L_INFO, "Starting - reading configuration files ...");
	} else if (pid == radius_pid) {
		log(L_INFO, "Reloading configuration files.");
	}

	/* Initialize the dictionary */
	if (dict_init(radius_dir, NULL) != 0)
		res = -1;

	/* Initialize Configuration Values */
	if (res == 0 && config_init() != 0)
		res = -1;

	/* Read users file etc. */
	if (res == 0 && read_config_files() != 0)
		res = -1;
#ifdef USEMYSQL
	/* Read and init mysql config */
	if (res == 0 && sql_read_config() != 0){
/*		log(L_ERR,"MYSQL Error: MySQL could not be initialized"); */
		res = -1;
	}
#endif
	if (res != 0) {
		if (pid == radius_pid) {
			log(L_ERR|L_CONS,
				"Errors reading config file - EXITING");
			if (acct_pid) {
				signal(SIGCHLD, SIG_DFL);
				kill(acct_pid, SIGTERM);
			}
		}
		exit(1);
	}
}


int main(int argc, char **argv)
{
	int			salen;
	int			result;
	struct	sockaddr	salocal;
	struct	sockaddr	saremote;
	struct	sockaddr_in	*sin;
	struct	servent		*svp;
	AUTH_REQ		*authreq;
	int			argval;
	int			t;
	int			pid;
	fd_set			readfds;
	int			status;
	int			dontfork = 0;
	int			radius_port = 0;
	int			check_config = 0;
#ifdef RADIUS_PID
	char			*pid_file = RADIUS_PID;
	FILE			*fp;
#endif

	/*
	 *	Make sure we have stdin/stdout/stderr reserved.
	 */
	while((t = open("/dev/null", O_RDWR)) < 3 && t >= 0)
		;
	if (t >= 3) close(t);

#ifdef OSFC2
	set_auth_parameters(argc,argv);
#endif

	if ((progname = strrchr(argv[0], '/')) == NULL)
		progname = argv[0];
	else
		progname++;

	debug_flag = 0;
	spawn_flag = 1;
	radacct_dir = RADACCT_DIR;
	radlog_dir = RADLOG_DIR;
	radius_dir = RADIUS_DIR;
	radutmp_path = RADUTMP;
	radwtmp_path = RADWTMP;
	start_time = time(NULL);

	signal(SIGHUP, sig_hup);
	signal(SIGINT, sig_fatal);
	signal(SIGQUIT, sig_fatal);
	signal(SIGTRAP, sig_fatal);
	signal(SIGIOT, sig_fatal);
	signal(SIGTERM, sig_fatal);
	signal(SIGCHLD, sig_cleanup);
#if 0
	signal(SIGFPE, sig_fatal);
	signal(SIGSEGV, sig_fatal);
	signal(SIGILL, sig_fatal);
#endif

	/*
	 *	Close unused file descriptors.
	 */
	for (t = 32; t >= 3; t--)
			close(t);

	/*
	 *	Process the options.
	 */
	while((argval = getopt(argc, argv, "A:CSDF:P:W:Za:ci:l:d:bfp:g:su:vwxyz")) != EOF) {

		switch(argval) {

		case 'A':
			log_auth_detail = optarg;
			break;

		case 'a':
			radacct_dir = optarg;
			break;
		
#ifdef USE_DBX
		case 'b':
			use_dbm++;
			break;
#endif
		case 'c':
			cache_passwd = 1;
			break;

		case 'C':
			check_config = 1;
			break;

		case 'D':
			use_dns = 0;
			break;

		case 'd':
			radius_dir = optarg;
			break;
		
		case 'F':
			rad_add_detail(optarg);
			break;

		case 'f':
			dontfork = 1;
			break;

		case 'i':
			if ((myip = get_ipaddr(optarg)) == 0) {
				fprintf(stderr, "radiusd: %s: host unknown\n",
					optarg);
				exit(1);
			}
			break;
		
		case 'l':
			radlog_dir = optarg;
			break;
		
#ifdef USE_SYSLOG
		case 'g':
			syslog_facility = -1;
			if (strncmp(optarg, "local", 5) == 0 && !optarg[6])
				syslog_facility = optarg[5] - '0';
			if (syslog_facility < 0 || syslog_facility > 7) {
				fprintf(stderr,
				"radiusd: unknown syslog facility: %s\n",
				optarg);
				exit(1);
			}
#endif

		case 'S':
			log_stripped_names++;
			break;

		case 'p':
			radius_port = atoi(optarg);
			break;

#ifdef RADIUS_PID
		case 'P':
			pid_file = optarg;
			break;
#endif

		case 's':	/* Single process mode */
			spawn_flag = 0;
			break;

		case 'u':
			radutmp_path = optarg;
			break;

		case 'v':
			version();
			break;

		case 'W':
			radwtmp_path = optarg;
			break;

		case 'w':
			use_wtmp = 0;
			break;

		case 'x':
			debug_flag++;
			break;
		
		case 'y':
			log_auth = 1;
			log_auth_goodpass = 0;
			log_auth_badpass = 1;
			break;

		case 'z':
			log_auth_badpass = 1;
			log_auth_goodpass = 1;
			break;

		case 'Z':
			log_auth_badpass = 0;
			log_auth_goodpass = 0;
			break;

		default:
			usage();
			break;
		}
	}

#ifdef USE_SYSLOG
	if (strcmp(radlog_dir, "syslog") == 0 && syslog_facility < 0) {
		fprintf(stderr,
			"radiusd: -l syslog needs -g syslog_facility\n");
		exit(1);
	}
#endif

	if (check_config) {
		debug_flag = 1;
		dup2(1, 2);
		reread_config(0);
		printf("Configuration OK\n");
		return 0;
	}

	/*
	 *	Open Authentication socket.
	 */
	svp = getservbyname ("radius", "udp");
	if (radius_port)
		auth_port = radius_port;
	else if (svp != (struct servent *) 0)
		auth_port = ntohs(svp->s_port);
	else
		auth_port = PW_AUTH_UDP_PORT;

	sockfd = socket (AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		perror("auth socket");
		exit(1);
	}

	sin = (struct sockaddr_in *) & salocal;
        memset ((char *) sin, '\0', sizeof (salocal));
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = myip ? htonl(myip) : INADDR_ANY;
	sin->sin_port = htons(auth_port);

	result = bind (sockfd, & salocal, sizeof (*sin));
	if (result < 0) {
		perror ("auth bind");
		exit(1);
	}

	/*
	 *	Open Accounting Socket.
	 */
	svp = getservbyname ("radacct", "udp");
	if (radius_port || svp == (struct servent *) 0)
		acct_port = auth_port + 1;
	else
		acct_port = ntohs(svp->s_port);
	
	acctfd = socket (AF_INET, SOCK_DGRAM, 0);
	if (acctfd < 0) {
		perror ("acct socket");
		exit(1);
	}

	sin = (struct sockaddr_in *) & salocal;
        memset ((char *) sin, '\0', sizeof (salocal));
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = myip ? htonl(myip) : INADDR_ANY;
	sin->sin_port = htons(acct_port);

	result = bind (acctfd, & salocal, sizeof (*sin));
	if (result < 0) {
		perror ("acct bind");
		exit(1);
	}

	/*
	 *	Read config files.
	 */
	reread_config(0);

	/*
	 *	Disconnect from session
	 */
	if (debug_flag == 0 && dontfork == 0) {
		pid = fork();
		if(pid < 0) {
			log(L_ERR|L_CONS, "Couldn't fork");
			exit(1);
		}
		if(pid > 0) {
			exit(0);
		}
		close(0);
		close(1);
		close(2);
		(void)open("/dev/null", O_RDWR);
		dup(0);
		dup(0);
#if defined(__linux__) || defined(__svr4__) || \
    defined(__hpux__)  || defined(__FreeBSD__)
		setsid();
#endif
	}
	radius_pid = getpid();
#ifdef RADIUS_PID
	if ((fp = fopen(pid_file, "w")) != NULL) {
		fprintf(fp, "%d\n", radius_pid);
		fclose(fp);
	}
#endif

	/*
	 *	Use linebuffered or unbuffered stdout if
	 *	the debug flag is on.
	 */
	if (debug_flag) setlinebuf(stdout);

#if !defined(M_UNIX) && !defined(__linux__)
	/*
	 *	Open system console as stderr
	 */
	if (!debug_flag && strcmp(radlog_dir, "stderr") != 0) {
		t = open("/dev/console", O_WRONLY | O_NOCTTY);
		if (t != 2) {
			dup2(t, 2);
			close(t);
		}
	}
#endif
	/*
	 *	If we are in forking mode, we will start a child
	 *	to listen for Accounting requests.  If not, we will 
	 *	listen for them ourself.
	 */
	if (spawn_flag) {
		acct_pid = fork();
		if(acct_pid < 0) {
			log(L_ERR|L_CONS, "Couldn't fork");
			exit(1);
		}
		if(acct_pid > 0) {
			close(acctfd);
			acctfd = -1;
			log(L_INFO, "Ready to process requests.");
		}
		else {
			close(sockfd);
			sockfd = -1;
		}
	} else
		log(L_INFO, "Ready to process requests.");


	/*
	 *	Receive user requests
	 */
	sin = (struct sockaddr_in *) & saremote;

	for(;;) {

		if (need_reload) {
			reread_config(1);
			need_reload = 0;
			if (getpid() == radius_pid && acct_pid)
				kill(acct_pid, SIGHUP);
		}

		FD_ZERO(&readfds);
		if (sockfd >= 0) {
			FD_SET(sockfd, &readfds);
		}
		if (acctfd >= 0) {
			FD_SET(acctfd, &readfds);
		}

		status = select(32, &readfds, NULL, NULL, NULL);
		if (status == -1) {
			if (errno == EINTR)
				continue;
			sig_fatal(101);
		}
		if (sockfd >= 0 && FD_ISSET(sockfd, &readfds)) {
			salen = sizeof (saremote);
			result = recvfrom (sockfd, (char *) recv_buffer,
				(int) sizeof(i_recv_buffer),
				(int) 0, &saremote, &salen);

			if (result > 0) {
				authreq = radrecv(
					ntohl(sin->sin_addr.s_addr),
					ntohs(sin->sin_port),
					recv_buffer, result);
				radrespond(authreq, sockfd);
			}
			else if (result < 0 && errno == EINTR) {
				result = 0;
			}
		}
		if (acctfd >=0 && FD_ISSET(acctfd, &readfds)) {
			salen = sizeof (saremote);
			result = recvfrom (acctfd, (char *) recv_buffer,
				(int) sizeof(i_recv_buffer),
				(int) 0, &saremote, &salen);

			if (result > 0) {
				authreq = radrecv(
					ntohl(sin->sin_addr.s_addr),
					ntohs(sin->sin_port),
					recv_buffer, result);
				radrespond(authreq, acctfd);
			}
			else if (result < 0 && errno == EINTR) {
				result = 0;
			}
		}
	}
}


/*
 *	Process and reply to a server-status request.
 *	Like rad_authenticate and rad_accounting this should
 *	live in it's own file but it's so small we don't bother.
 */
int rad_status_server(AUTH_REQ *authreq, int activefd)
{
	char		reply_msg[64];
	time_t		t;

	/*
	 *	Reply with an ACK. We might want to add some more
	 *	interesting reply attributes, such as server uptime.
	 */
	t = time(NULL) - start_time;
	sprintf(reply_msg, "Cistron Radius up %d day%s, %02d:%02d",
		(int)(t / 86400), (t / 86400) == 1 ? "" : "s",
		(int)((t / 3600) % 24), (int)(t / 60) % 60);
	rad_send_reply(PW_AUTHENTICATION_ACK, authreq,
			NULL, reply_msg, activefd);
	authfree(authreq);

	return 0;
}


/*
 *	Respond to supported requests:
 *
 *		PW_AUTHENTICATION_REQUEST - Authentication request from
 *				a client network access server.
 *
 *		PW_ACCOUNTING_REQUEST - Accounting request from
 *				a client network access server.
 *
 *		PW_AUTHENTICATION_ACK
 *		PW_AUTHENTICATION_REJECT
 *		PW_ACCOUNTING_RESPONSE - Reply from a remote Radius server.
 *				Relay reply back to original NAS.
 *
 *		PW_SERVER_STATUS - Usually an "alive" check.
 *
 */
int radrespond(AUTH_REQ *authreq, int activefd)
{
	FUNP fun;
	VALUE_PAIR *namepair;
	RADCLIENT *cl;
	int dospawn;
	int e;

	dospawn = 0;
	fun = NULL;

	/*
	 *      See if we know this client.
	 */
        if ((cl = client_find(authreq->ipaddr)) == NULL) {
                log(L_ERR, "packet from unknown client/host: %s",
                        client_name(authreq->ipaddr));
                return -1;
        }
	strNcpy(authreq->secret, cl->secret, sizeof(authreq->secret));

#ifdef USE_DBX
	/*
	 *	When using the DBM code, read or re-read the
	 *	DBM database. We do this in the parent, before
	 *	we fork, so that we don't have to reopen the
	 *	database for every request.
	 */
	if (use_dbm) rad_dbm_open();
#endif

	/*
	 *	First, see if we need to proxy this request.
	 */
	switch(authreq->code) {

	case PW_AUTHENTICATION_REQUEST:
		/*
		 *	Check request against hints and huntgroups.
		 */
		if ((e = rad_auth_init(authreq, activefd)) < 0)
			return e;
		/*FALLTHRU*/
	case PW_ACCOUNTING_REQUEST:
		namepair = pairfind(authreq->request, PW_USER_NAME);
		if (namepair == NULL)
			break;
		/*
		 *	We always call proxy_send, it returns non-zero
		 *	if it did actually proxy the request.
		 */
		if (proxy_send(authreq, activefd) != 0)
			return 0;
		break;

	case PW_AUTHENTICATION_ACK:
	case PW_AUTHENTICATION_REJECT:
	case PW_ACCOUNTING_RESPONSE:
		if (proxy_receive(authreq, activefd) < 0)
			return 0;
		break;
	}

	/*
	 *	Select the required function and indicate if
	 *	we need to fork off a child to handle it.
	 */
	switch(authreq->code) {

	case PW_AUTHENTICATION_REQUEST:
		dospawn = spawn_flag;
		fun = rad_authenticate;
		break;
	
	case PW_ACCOUNTING_REQUEST:
		fun = rad_accounting;
		break;
	
	case PW_PASSWORD_REQUEST:
		/*
		 *	FIXME: print an error message here.
		 *	We don't support this anymore.
		 */
		/* rad_passchange(authreq, activefd); */
		log(L_ERR, "Unsupported change password request packet "
			   "from nas [%s]", client_name(authreq->ipaddr));
		break;

	case PW_STATUS_SERVER:
		fun = rad_status_server;
		break;

	default:
		log(L_ERR, "Unknown packet type [%d] from nas [%s]",
			authreq->code, client_name(authreq->ipaddr));
		break;
	}

	/*
	 *	If we did select a function, execute it
	 *	(perhaps through rad_spawn_child)
	 */
	if (fun) {
		if (dospawn)
			rad_spawn_child(authreq, activefd, fun);
		else
			(*fun)(authreq, activefd);
	} else {
		authfree(authreq);
	}

	return 0;
}


/*
 *	Spawns child processes to perform authentication/accounting
 *	and respond to RADIUS clients.  This functions also
 *	cleans up complete child requests, and verifies that there
 *	is only one process responding to each request (duplicate
 *	requests are filtered out).
 */
static void rad_spawn_child(AUTH_REQ *authreq, int activefd, FUNP fun)
{
	AUTH_REQ	*curreq;
	AUTH_REQ	*prevreq;
	VALUE_PAIR	*vp;
	UINT4		curtime;
	int		request_count;
	int		child_pid;

	curtime = (UINT4)time(NULL);
	request_count = 0;
	curreq = first_request;
	prevreq = (AUTH_REQ *)NULL;

	/*
	 *	When mucking around with the request list, we block
	 *	asynchronous access (through the SIGCHLD handler) to
	 *	the list - equivalent to sigblock(SIGCHLD).
	 */
	request_list_busy = 1;

	while(curreq != (AUTH_REQ *)NULL) {
		if (curreq->child_pid == -1 &&
		    curreq->timestamp + CLEANUP_DELAY <= curtime) {
			/*
			 *	Request completed, delete it
			 */
			if (prevreq == (AUTH_REQ *)NULL) {
				first_request = curreq->next;
				authfree(curreq);
				curreq = first_request;
			} else {
				prevreq->next = curreq->next;
				authfree(curreq);
				curreq = prevreq->next;
			}
		} else if (curreq->ipaddr == authreq->ipaddr &&
			   curreq->udp_port == authreq->udp_port &&
			   curreq->id == authreq->id) {
			/*
			 *	This is a duplicate request - we are still
			 *	processing it, or sent a reply less than
			 *	3 seconds ago. Just drop it.
			 */
			vp = pairfind(authreq->request, PW_USER_NAME);
			log(L_ERR,
			"Duplicate authentication packet: [%s] (%s/ID:%d)",
				vp ? vp->strvalue : "",
				auth_name(authreq, 0), authreq->id);
			authfree(authreq);
			request_list_busy = 0;
			sig_cleanup(SIGCHLD);

			return;
		} else {
			if (curreq->timestamp + MAX_REQUEST_TIME <= curtime &&
			    curreq->child_pid != -1) {
				/*
				 *	This request seems to have hung -
				 *	kill it
				 */
				child_pid = curreq->child_pid;
				log(L_ERR,
					"Killing unresponsive child pid %d",
								child_pid);
				curreq->child_pid = -1;
				kill(child_pid, SIGTERM);
			}
			prevreq = curreq;
			curreq = curreq->next;
			request_count++;
		}
	}

	/*
	 *	This is a new request
	 */
	if (request_count > MAX_REQUESTS) {
		log(L_ERR,
		"Dropping packet (too many): from host %s:%d - ID: %d",
			client_name(authreq->ipaddr), authreq->udp_port,
			authreq->id);
		authfree(authreq);

		request_list_busy = 0;
		sig_cleanup(SIGCHLD);

		return;
	}

	/*
	 *	Add this request to the list
	 */
	authreq->next = (AUTH_REQ *)NULL;
	authreq->child_pid = -1;
	authreq->timestamp = curtime;

	if (prevreq == (AUTH_REQ *)NULL)
		first_request = authreq;
	else
		prevreq->next = authreq;

	/*
	 *	fork our child
	 */
	if ((child_pid = fork()) < 0) {
		log(L_ERR, "Fork failed for request from nas %s - ID: %d",
				nas_name2(authreq), authreq->id);
	}
	if (child_pid == 0) {
		/*
		 *	This is the child, it should go ahead and respond
		 */
		request_list_busy = 0;
		signal(SIGCHLD, SIG_DFL);
		(*fun)(authreq, activefd);
		exit(0);
	}

	/*
	 *	Register the Child
	 */
	authreq->child_pid = child_pid;

	request_list_busy = 0;
	sig_cleanup(SIGCHLD);
}


/*ARGSUSED*/
void sig_cleanup(int sig)
{
	int		status;
        pid_t		pid;
	AUTH_REQ	*curreq;
 
	/*
	 *	request_list_busy is a lock on the request list
	 */
	if (request_list_busy) {
		got_chld = 1;
		return;
	}
	got_chld = 0;

	/*
	 *	There are reports that this line on Solaris 2.5.x
	 *	caused trouble. Should be fixed now that Solaris
	 *	[defined(sun) && defined(__svr4__)] has it's own
	 *	sun_signal() function.
	 */
	signal(SIGCHLD, sig_cleanup);

        for (;;) {
		pid = waitpid((pid_t)-1, &status, WNOHANG);
                if (pid <= 0)
                        return;

#if defined (aix) /* Huh? */
		kill(pid, SIGKILL);
#endif

		if (pid == acct_pid)
			sig_fatal(100);

		curreq = first_request;
		while (curreq != (AUTH_REQ *)NULL) {
			if (curreq->child_pid == pid) {
				curreq->child_pid = -1;
				/*
				 *	FIXME: UINT4 ?
				 */
				curreq->timestamp = (UINT4)time(NULL);
				break;
			}
			curreq = curreq->next;
		}
        }
}

/*
 *	Display the syntax for starting this program.
 */
static void usage(void)
{
	fprintf(stderr,
#ifdef USE_DBX
		"Usage: %s [-a acct_dir] [-d db_dir] [-l logdir] [-bcsxYyz]\n",
#else
		"Usage: %s [-a acct_dir] [-d db_dir] [-l logdir] [-csxYyz]\n",
#endif
		progname);
	exit(1);
}


/*
 *	Intializes configuration values:
 *
 *		warning_seconds - When acknowledging a user authentication
 *			time remaining for valid password to notify user
 *			of password expiration.
 *
 *	These values are read from the SERVER_CONFIG part of the
 *	dictionary (of all places!)
 */
int config_init()
{
	DICT_VALUE	*dval;

	warning_seconds = 0;
	dval = dict_valfind("Password-Warning", "Server-Config");
	if (dval != NULL)
		warning_seconds = dval->value * (UINT4)SECONDS_PER_DAY;

	return 0;
}



/*
 *	We got a fatal signal. Clean up and exit.
 */
static void sig_fatal(int sig)
{
	char *me = "MASTER: ";

	if (radius_pid == getpid()) {
		/*
		 *      FIXME: kill all children, not only the
		 *      accounting process. Oh well..
		 */
		if (acct_pid > 0)
			kill(acct_pid, SIGKILL);
	} else {
		me = "CHILD: ";
	}

	switch(sig) {
		case 100:
			log(L_ERR, "%saccounting process died - exit.", me);
			break;
		case 101:
			log(L_ERR, "%sfailed in select() - exit.", me);
			break;
		case SIGTERM:
			log(L_INFO, "%sexit.", me);
			break;
		default:
			log(L_ERR, "%sexit on signal (%d)", me, sig);
			break;
	}

	exit(sig == SIGTERM ? 0 : 1);
}


/*
 *	We got the hangup signal.
 *	Re-read the configuration files.
 */
/*ARGSUSED*/
static void sig_hup(int sig)
{
	signal(SIGHUP, sig_hup);
	need_reload = 1;
}

