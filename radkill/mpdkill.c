#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>


int main(int argc, char *argv[]){
/*
	Usage: mpd.kill nasip nasport userip
*/

	int sock, port = -1;
	int nasport = 5005;
	u_int32_t host;
	struct hostent *hp;
	struct sockaddr_in addr;
	char emsg[32];
	int size;
	
	if (argc < 3){
		printf("%s: nasip nasport userip\n", argv[0]);
		return -1;
	}

	if ((host = inet_addr(argv[1])) == -1){
		if ((hp = gethostbyname(argv[1])) == NULL){
			printf("unknown host: %s\n", argv[1]);
			return -1;
		} else {
			host = *(u_int32_t *)hp->h_addr;
		}
	}
	

	if ((port = atoi(argv[2])) < 0){
		printf("unknown port: %s\n", argv[2]);
		return -1;
	}

	
	
	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0){
		fprintf(stderr, "Can't create socket\n");
		return -1;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(nasport);
	addr.sin_addr.s_addr = host;
	bzero(&(addr.sin_zero), 8);


	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0){
		fprintf(stderr, "Can't connect to %s\n", argv[1]);
		close(sock);
		return -1;
	}

/*
	if (recv(sock, &emsg, sizeof(emsg), 0) == -1){
		printf("recv: Error\n");
	}
	printf("%s", emsg);
*/
	size = sprintf(emsg, "link pptp%d\n", port);
	if (send(sock, emsg, size, 0) == -1){
		printf("send: Error\n");
	}

	strcpy(emsg, "close\n");
	if (send(sock, emsg, 6, 0) == -1){
		printf("send: Error\n");
	}

	strcpy(emsg, "exit\n");
	if (send(sock, emsg, 5, 0) == -1){
		printf("send: Error\n");
	}

	close(sock);

	return 0;
}
