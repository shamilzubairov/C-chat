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
struct sockaddr_in cl_addr;
struct sockaddr_in from_addr;

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
	cl_addr.sin_family = FAM;

	int sd = socket(FAM, SOCK, 0);
	if(sd == -1) {
		handle_error("socket");
	}

	int i = 0;
	do {
		bzero(req, 100);
		write(1, "> ", 2);
		read(0, req, sizeof(req));

		int s = sendto(sd, req, sizeof(req), 0, (struct sockaddr*)&s_addr, sizeof(s_addr));
		if(s == -1) {
			handle_error("send");
		}
		// Создать fork для асинхронного получения ответов
		alarm(60);
		socklen_t from_addrlen = sizeof(from_addr);
		recvfrom(sd, res, sizeof(res), 0, (struct sockaddr*)&from_addr, &from_addrlen);
		write(1, res, 100);

		i++;
	} while(i < 100);

	close(sd);
	return 0;
}
