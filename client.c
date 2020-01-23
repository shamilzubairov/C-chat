#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h> // sockaddr_in
#include <arpa/inet.h> // htons

#define handle_error(msg) \
do { \
	perror(msg); \
	return(1); \
} while(0)

#define HOST "127.0.0.1"
#define PORT 7654

enum Family { FAM = AF_INET, SOCK = SOCK_DGRAM };

char req[100];
char res[100];

struct sockaddr_in s_addr;

void sig_handler(int sig) {
	char msg[] = "alarm::socket didn\'t get any response\n";
	write(1, msg, sizeof(msg));
	exit(1);
}

int main() {
	signal(SIGALRM, sig_handler);

	s_addr.sin_family = FAM;
	s_addr.sin_port = htons(PORT);
	if(!inet_aton(HOST, &s_addr.sin_addr)) {
		handle_error("invalid address");
	}

	int sd = socket(FAM, SOCK, 0);
	if(sd == -1) {
		handle_error("socket");
	} else {
		printf("connection with %s:%d established\n", HOST, PORT);
		printf("you can start chatting\n");
		sendto(sd, " ", 1, 0, (struct sockaddr*)&s_addr, sizeof(s_addr));
	}

	int i = 0;
	do {
		alarm(60);
		if(fork() == 0) {
			int i = 0;
			do {
				recvfrom(sd, res, sizeof(res), 0, NULL, 0);
				printf("\t%s", res);
				i++;
			} while(i < 100);

			exit(0);
		}
		bzero(req, 100);
		read(0, req, sizeof(req));

		int s = sendto(sd, req, sizeof(req), 0, (struct sockaddr*)&s_addr, sizeof(s_addr));
		if(s == -1) {
			handle_error("send");
		}

		i++;
	} while(i < 100);

	close(sd);
	return 0;
}
