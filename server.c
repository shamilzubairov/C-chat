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

char message[] = "Hello Mr. Client\n";
char income[100];

struct sockaddr_in addr;
struct sockaddr_in to_addr;

int add_to_Clients(struct sockaddr_in*, struct sockaddr_in);

void send_to_Clients(int, struct sockaddr_in*, struct sockaddr_in, int socket, socklen_t addrlen);

void sig_handler(int sig) {
	char msg[] = "alarm::server didn\'t get any message\n";
	write(1, msg, sizeof(msg));
	exit(1);
}

int main() {
	signal(SIGALRM, sig_handler);

	addr.sin_family = FAM;
	addr.sin_port = htons(PORT);
	if(!inet_aton(HOST, &addr.sin_addr)) {
		handle_error("invalid address");
	}
	// либо заменить на -  
	// addr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	// FAM семейство адресации
	// SOCK тип взаимодействия
	// Третий параметр - тип протокола, 
	// в данном случае тип определяется двумя первыми параметрами;
	// можно использовать IPPPORTO_UDP / IPPPORTO_TCP
	int sd = socket(FAM, SOCK, 0);
	if(sd == -1) {
		handle_error("socket");
	}
	
	int bnd = bind(sd, (struct sockaddr*)&addr, sizeof(addr));
	if(bnd == -1) {
		handle_error("bind");
	} else {
		const char message[] = 
			"Server is working...\nYou can start chatting!\n------------------\n\n";
		write(1, message, sizeof(message));
	}

	int i = 0;
	struct sockaddr_in Clients[10]; // Храним максимум 10 подключенных
	bzero(Clients, sizeof(Clients));
	do {
		alarm(60);
		socklen_t to_addrlen = sizeof(to_addr);
		recvfrom(sd, income, sizeof(income), 0, (struct sockaddr*)&to_addr, &to_addrlen);
		
		if(i == 0) Clients[i] = to_addr; // Клиента с первым сообщением добавляем сразу

		int k = 0;
		if((k = add_to_Clients(Clients, to_addr)) <= 10) Clients[k] = to_addr;

		send_to_Clients(i, Clients, to_addr, sd, to_addrlen);

		bzero(income, sizeof(income));
		i++;
	} while(i < 100); // 100 сообщений
	
	close(sd);
	return 0;
}

int add_to_Clients(struct sockaddr_in *Clients, struct sockaddr_in to_addr) {
	int k = 0;
	for(;k <= 10; k++) {
		if(Clients[k].sin_port == to_addr.sin_port) {
			return 11;
		} else if (!Clients[k].sin_port) {
			return k;
		}
	}
	return 11; // Массив полон, клиентов добавлять нельзя
}

void send_to_Clients(int index, struct sockaddr_in *Clients, struct sockaddr_in to_addr, int sd, socklen_t to_addrlen) {
	int j = 0;
	for(;j <= index && (Clients[j].sin_port); j++) {
		if(Clients[j].sin_port == to_addr.sin_port) continue;
		if(sendto(sd, income, sizeof(income), 0, (struct sockaddr*)&Clients[j], to_addrlen) == -1) {
			perror("not sent");
		}
	}
}
