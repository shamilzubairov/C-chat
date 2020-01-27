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

char income[100];
int total_clients = 10;
int total_messages = 100;

struct sockaddr_in addr;
struct sockaddr_in to_addr;

struct sockaddr_in Clients[10]; // Храним максимум 10 подключенных

void send_to_Clients(struct sockaddr_in *, struct sockaddr_in, int socket, socklen_t);

int add_to_Clients(struct sockaddr_in*, struct sockaddr_in);

void printraw(const char *);

void sig_handler(int sig) {
	printraw("alarm::server didn\'t get any message\n");
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
		printraw("Server is working...\n------------------\n\n");
	}

	bzero(Clients, sizeof(Clients));

	int circle = 0;
	do {
		alarm(60);
		socklen_t to_addrlen = sizeof(to_addr);
		recvfrom(sd, income, sizeof(income), 0, (struct sockaddr*)&to_addr, &to_addrlen);

		if(!strncmp(income, "LOGIN::", 7)) {
			static int count = 0;
			if((count = add_to_Clients(Clients, to_addr)) <= total_clients) {
				// Клиента добавляем в массив
				Clients[count] = to_addr;
				// Отправляем контрольный бит
				sendto(sd, "1", 1, 0, (struct sockaddr*)&to_addr, to_addrlen);
				count++;
			}
			continue;
		}
		if(!strncmp(income, "::name", 6)) {
			printf("%s\n", income);
			continue;
		}
		if(!strncmp(income, "::exit", 6)) {
			printf("%s\n", income);
			continue;
		}
		send_to_Clients(Clients, to_addr, sd, to_addrlen);

		bzero(income, sizeof(income));
		circle++;
	} while(circle < total_messages); // 100 сообщений
	
	close(sd);
	return 0;
}

void send_to_Clients(struct sockaddr_in * Clients, struct sockaddr_in to_addr, int sd, socklen_t to_addrlen) {
	int j = 0;
	int total_message_size = sizeof(income) + 30; 
	for(;j <= total_clients && (Clients[j].sin_port); j++) {
		if(Clients[j].sin_port == to_addr.sin_port) continue;
		char message[total_message_size];
		memset(message, '\0',  total_message_size);
		strcat(message, income);
		if(sendto(sd, message, sizeof(message), 0, (struct sockaddr*)&Clients[j], to_addrlen) == -1) {
			perror("not sent");
		}
	}
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

void printraw(const char *message) {
	int size = 0;
	while(message[size]) size++;
	write(1, message, size);
}
