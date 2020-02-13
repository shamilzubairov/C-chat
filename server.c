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

// platform detection
#define OS_WINDOWS 1
#define OS_MAC 2
#define OS_UNIX 3

#if defined(_WIN32)
#define OS OS_WINDOWS
#elif defined(__APPLE__)
#define OS OS_MAC
#else
#define OS OS_UNIX
#endif

#if OS == OS_WINDOWS
#include <winsock2.h>
#elif OS == OS_MAC || OS == OS_UNIX
#include <sys/socket.h>
#include <netinet/in.h> // sockaddr_in
#include <fcntl.h>
#endif

#if OS == OS_WINDOWS
#pragma comment( lib, "wsock32.lib" )
#endif

#if OS == OS_WINDOWS
typedef int socklen_t;
#endif

#define MAXCLIENTSSIZE 10
#define MESSAGESSIZE 100
#define SMALLMESSAGESSIZE 50
#define LOGINSIZE 20

void printub(const char *);
int getsize(const char *);
void remove_nl(char *);
void multistrcat(char *, ...);

void sig_alarm(int);
void sig_int(int);
void sig_stub(int);
int sys_error(char *);

enum Family { FAM = AF_INET, SOCK = SOCK_DGRAM };

struct UDPServer {
	char * host;
	int port;
	int socket; // сокет для подключения
	struct sockaddr_in address;
} udp = {
	"127.0.0.1",
	7666,
	-1,
	0,
};

struct Clients {
	struct sockaddr_in c_addr[MAXCLIENTSSIZE];
	int cur_client_size;
	int attempt;
	char login[MAXCLIENTSSIZE][LOGINSIZE];
} client = {
	0,
	0,
	10
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

void send_to_clients(int, int, struct Clients, const char *);

void send_notify(int, struct Clients, const char *);

inline int InitializeSockets() {
	#if OS == OS_WINDOWS
	WSADATA WsaData;
	return WSAStartup( MAKEWORD(2,2), &WsaData ) == 1;
	#else
	return 1;
	#endif
}

inline void ShutdownSockets() {
	#if OS == OS_WINDOWS
	WSACleanup();
	#endif
}

int main() {
	sigaction_init(SIGALRM, handler.sig_alarm);
	sigaction_init(SIGINT, handler.sig_int);

	int opt = 1;
	
	udp.socket = socket(FAM, SOCK, 0);
	if(udp.socket == -1) {
		handler.sys_error("Failed socket connection");
	}
	udp.address.sin_family = FAM;
	udp.address.sin_port = htons(udp.port);
	if(!inet_aton(udp.host, &udp.address.sin_addr)) {
		handler.sys_error("Invalid address");
	}
	setsockopt(udp.socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if(bind(udp.socket, (struct sockaddr *)&udp.address, sizeof(udp.address)) == -1) {
		handler.sys_error("Failed bind connection");
	} else {
		printf("Server is working by PID %d\n----------------------\n\n", getpid());
	}
	
	do {
		if(client.cur_client_size > MAXCLIENTSSIZE) {
			// Достигнуто макс. кол-во клиентов
			// Отослать уведомление
		}
		struct sockaddr_in from_address;
		socklen_t from_addrlen = sizeof(from_address);
		
		char request[SMALLMESSAGESSIZE];
		bzero(request, SMALLMESSAGESSIZE);
		recvfrom(udp.socket, request, SMALLMESSAGESSIZE, 0, (struct sockaddr *)&from_address, &from_addrlen);
		// int register_port = ntohs(from_address.sin_port);
		int register_port = from_address.sin_port;

		if(!strncmp(request, "REG::", 3)) {
			// Установка соединения
			// Значит пришел токен, отправляем обратно
			sendto(udp.socket, request, SMALLMESSAGESSIZE, 0, (struct sockaddr *)&from_address, from_addrlen);
			char sync[3];
			int messagecount = 0;
			printf("ATT - %d\n", client.attempt);
			while(messagecount < 10) {
				recvfrom(udp.socket, sync, 3, 0, (struct sockaddr *)&from_address, &from_addrlen);
				if(!strncmp(sync, "SYN", 3)) {
					break;
				}
				messagecount++;
			}
			if(messagecount == 10 && strcmp(sync, "SYN")) {
				continue;
			} else {
				// Регистрируем участника
				// Ожидаем логин
				recvfrom(udp.socket, client.login[client.cur_client_size], LOGINSIZE, 0, (struct sockaddr *)&from_address, &from_addrlen);
				client.c_addr[client.cur_client_size] = from_address;
				
				printf("Connection with %d by login %s\n", 
					client.c_addr[client.cur_client_size].sin_port, 
					client.login[client.cur_client_size]);
				
				// Рассылаем приветствие
				char greeting[MESSAGESSIZE];
				bzero(greeting, MESSAGESSIZE);
				multistrcat(greeting, "\t\t", client.login[client.cur_client_size], " join this chat\n", "\0");
				send_to_clients(udp.socket, client.c_addr[client.cur_client_size].sin_port, client, greeting);
				
				client.cur_client_size++;
			}
		} else {
			// ЧАТ
			send_to_clients(udp.socket, register_port, client, request);
		}

	} while(1);

	#if OS == OS_MAC || OS == OS_UNIX
    close(udp.socket);
    #elif OS == OS_WINDOWS
    closesocket(udp.socket);
    #endif
	return 0;
}

void send_to_clients(int socket, int register_port, struct Clients client, const char * incoming) {
	int j = 0;
	printf("PORT %d\n", register_port);
	for(;j < client.cur_client_size; j++) {
		if(client.c_addr[j].sin_port == register_port) continue;
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
	// Если сервер отключился, отправить сообщение всем клиентам
	// и инициировать у них отключение и повторное подключение
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