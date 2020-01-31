#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netinet/in.h> // sockaddr_in
#include <arpa/inet.h> // htons

#define MAXCLIENTSSIZE 10
#define MESSAGESSIZE 100
#define LOGINSIZE 20

void printub(const char *);
int getsize(const char *);
void remove_nl(char *);
void multistrcat(char *, ...);

void sig_alarm(int);
void sig_int(int);
void sig_stub(int);
int sys_error(char *);

enum Family { FAM = AF_INET, SOCK = SOCK_STREAM, SOCK_UDP = SOCK_DGRAM };

struct Server {
	char * host;
	int port;
	int socket; // сокет для подключения
	struct sockaddr_in s_addr;
} server = {
	"127.0.0.1",
	7654,
	-1,
	0
};

struct ServerUDP {
	char * host;
	int port;
	int socket; // сокет для подключения
	struct sockaddr_in s_addr;
} udp = {
	"127.0.0.1",
	7660,
	-1,
	0
};

struct Clients {
	int c_socket[MAXCLIENTSSIZE];
	struct sockaddr_in c_addr[MAXCLIENTSSIZE];
	int cur_client_size;
	char * login[LOGINSIZE];
} client = {
	-1,
	0,
	0
};

struct SystemHandlers {
	void (*sig_alarm) ();
	void (*sig_int) ();
	void (*sig_stub) ();
	int (*sys_error) (char *);
} handler = {
	&sig_alarm,
	&sig_int,
	&sig_stub,
	&sys_error,
};

void sigaction_init(int sig, void handler());

void send_to_clients(int, struct Clients, int, const char *);

void send_notify(int, struct Clients, const char *);

int main() {
	sigaction_init(SIGALRM, handler.sig_alarm);
	sigaction_init(SIGINT, handler.sig_int);

// >>>>>>>>>>>>>>>>>> TCP >>>>>>>>>>>>>>>>>>>>>

	server.socket = socket(FAM, SOCK, 0);
	if(server.socket == -1) {
		handler.sys_error("Socket connection");
	}
	server.s_addr.sin_family = FAM;
	server.s_addr.sin_port = htons(server.port);
	if(!inet_aton(server.host, &server.s_addr.sin_addr)) {
		handler.sys_error("Invalid address");
	}
	int opt = 1;
	setsockopt(server.socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if(bind(server.socket, (struct sockaddr *)&server.s_addr, sizeof(server.s_addr)) == -1) {
		handler.sys_error("Bind connection");
	} else {
		printf("Server is working by PID - %d\n----------------------\n\n", getpid());
	}

// >>>>>>>>>>>>>>>>>> UDP >>>>>>>>>>>>>>>>>>>>>

	udp.socket = socket(FAM, SOCK_UDP, 0);
	if(udp.socket == -1) {
		handler.sys_error("UDP socket connection");
	}
	udp.s_addr.sin_family = FAM;
	udp.s_addr.sin_port = htons(udp.port);
	if(!inet_aton(udp.host, &udp.s_addr.sin_addr)) {
		handler.sys_error("UDP Invalid address");
	}
	setsockopt(udp.socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if(bind(udp.socket, (struct sockaddr *)&udp.s_addr, sizeof(udp.s_addr)) == -1) {
		handler.sys_error("UDP bind connection");
	}

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	listen(server.socket, 5);

	int socket_accept;
	do {
		if(client.cur_client_size > MAXCLIENTSSIZE) {
			// Достигнуто макс. кол-во клиентов
			// Отослать уведомление
		}

		char login[MESSAGESSIZE];
		bzero(login, MESSAGESSIZE);
		struct sockaddr_in from_addr;
		socklen_t from_addrlen = sizeof(from_addr);
		
		alarm(120);
		socket_accept = accept(server.socket, (struct sockaddr*)&from_addr, &from_addrlen);
		if(socket_accept == -1) {
			handler.sys_error("Accepted socket error");
		} else {
			read(socket_accept, login, LOGINSIZE);
			// Регистрируем участника
			client.login[client.cur_client_size] = malloc(LOGINSIZE);
			strcpy(client.login[client.cur_client_size], login);
			client.c_socket[client.cur_client_size] = socket_accept;
			client.c_addr[client.cur_client_size] = from_addr;
			printf("Connection with %d by login %s\n", 
				client.c_addr[client.cur_client_size].sin_port, 
				client.login[client.cur_client_size]);
			client.cur_client_size++;
		}
		
		if(fork() == 0) {
			sigaction_init(SIGALRM, handler.sig_alarm);
			sigaction_init(SIGINT, handler.sig_stub);

			close(server.socket);
			close(socket_accept);

			int register_port;
			char incoming[MESSAGESSIZE];
			struct sockaddr_in from_addr;
			socklen_t from_addrlen = sizeof(from_addr);
			do {
				bzero(incoming, MESSAGESSIZE);
				alarm(120);
				recvfrom(udp.socket, incoming, MESSAGESSIZE, 0, (struct sockaddr *)&from_addr, &from_addrlen);
				register_port = from_addr.sin_port;
				send_to_clients(udp.socket, client, register_port, incoming);
			} while(1);
			exit(0);
		}
// >>>>>>>>>>>>>>>>>>>>>>>>
	} while(1);

	close(udp.socket);
	close(server.socket);
	return 0;
}

void send_to_clients(int socket, struct Clients client, int register_port, const char * incoming) {
	int j = 0;
	for(;j < client.cur_client_size; j++) {
		if(sendto(socket, incoming, getsize(incoming), 0, (struct sockaddr *)&client.c_addr[j], sizeof(client.c_addr[j])) == -1) {
			handler.sys_error("Send to clients");
		}
	}
}

void send_notify(int socket, struct Clients client, const char * incoming) {
	int j = 0;
	for(;j < client.cur_client_size; j++) {
		if(sendto(socket, incoming, getsize(incoming), 0, (struct sockaddr *)&client.c_addr[j], sizeof(client.c_addr[j])) == -1) {
			handler.sys_error("Send notify");
		}
	}
}

// Низкоуровневая ф-ция вывода, нужна для вывода без буферизации
void printub(const char *str) {
	write(1, str, getsize(str));
}

// Чтоб не передавать параметр размера в функцию;
// у указателя размер всегда равен 4 / 8 байтам,
// а нужен реальный размер массива 
int getsize(const char *str) {
	int len = 0;
	while(str[len]) len++;
	return len;
}

void remove_nl(char *str) {
	str[getsize(str) - 1] = '\0';
}

void multistrcat(char *str, ...) {
	va_list args;
    va_start(args, str);
	bzero(str, getsize(str));
	char *s_arg;
	while (strcmp(s_arg = va_arg(args, char *), "\0")) {
		strcat(str, s_arg);
    }
	va_end(args);
}

// Изменить на sigaction
void sig_alarm(int sig) {
	printub("Alarm::socket didn\'t get any response\n");
	exit(1);
}

void sig_int(int sig) {
	char notify[] = "\t\tConnection closed\n";
	send_notify(udp.socket, client, notify);
	exit(0);
}

void sig_stub(int sig) {
	exit(0);
}

void sigaction_init(int sig, void handler()) {
	struct sigaction act;
	memset(&act, 0, sizeof(act));
	
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, sig);
	act.sa_mask = set;
	act.sa_handler = handler;
	sigaction(sig, &act, 0);
}

int sys_error(char * msg) {
	perror(msg);
	exit(1);
}

void close_sockets(int num, ...) {
	va_list args;
    va_start(args, num);
	int sock;
	while ((sock = va_arg(args, int)) && num != 0) {
		close(sock);
		num--;
    }
	va_end(args);
}