#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/wait.h>
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
#define LOGINSIZE 20
#define FILENAME "./list"

void printub(const char *);
int getsize(const char *);
void remove_nl(char *);
void multistrcat(char *, ...);

void sig_alarm(int);
void sig_int(int);
void sig_stub(int);
int sys_error(char *);

enum Family { FAM = AF_INET, SOCKD = SOCK_DGRAM, SOCKS = SOCK_STREAM };

struct Connection {
	char *host;
	int port;
	int socket;
	struct sockaddr_in address;
} tcp = {
	"127.0.0.1",
	7660,
	-1,
	0
}, udp = {
	"127.0.0.1",
	7661,
	-1,
	0
};

struct Clients {
	int cur_client_size;
	struct sockaddr_in c_addr[MAXCLIENTSSIZE];
	char login[MAXCLIENTSSIZE][LOGINSIZE];
} client = {
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

void save_client(struct Clients *);

void load_client(struct Clients *);

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
	// TCP
	tcp.socket = socket(FAM, SOCKS, 0);
	if(tcp.socket == -1) {
		handler.sys_error("Failed socket connection");
	}
	tcp.address.sin_family = FAM;
	tcp.address.sin_port = htons(tcp.port);
	if(!inet_aton(tcp.host, &tcp.address.sin_addr)) {
		handler.sys_error("Invalid address");
	}
	setsockopt(tcp.socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if(bind(tcp.socket, (struct sockaddr *)&tcp.address, sizeof(tcp.address)) == -1) {
		handler.sys_error("Failed bind TCP connection");
	}
	// UDP
	udp.socket = socket(FAM, SOCKD, 0);
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
		handler.sys_error("Failed bind UDP connection");
	}
	// ...
	printf("Server is working\n\n");

	// Создаем дочерний процесс под общение
	if(fork() == 0) {
		sigaction_init(SIGINT, handler.sig_stub);
		close(tcp.socket);
		struct sockaddr_in from_address;
		socklen_t from_addrlen = sizeof(from_address);
		while(1) {
			memset(&from_address, '\0', from_addrlen);
			char request[MESSAGESSIZE];
			memset(request, '\0', MESSAGESSIZE);
			recvfrom(udp.socket, request, MESSAGESSIZE, 0, (struct sockaddr *)&from_address, &from_addrlen);
			load_client(&client);
			send_to_clients(udp.socket, from_address.sin_port, client, request);
		}
		close(udp.socket);
		exit(0);
	}
	
	listen(tcp.socket, 5);
	int acc;
	while(1) {
		// Ожидаем подключения
		acc = accept(tcp.socket, 0, 0);
		// ...
		// Клиент подключился
		// Создаем данные под сохранение
		char login[LOGINSIZE];
		struct sockaddr_in from_address;
		socklen_t from_addrlen = sizeof(from_address);
		memset(login, '\0', LOGINSIZE);
		memset(&from_address, '\0', from_addrlen);
		// Ожидаем логина
		recvfrom(udp.socket, login, LOGINSIZE, 0, (struct sockaddr *)&from_address, &from_addrlen);
		// Сохраняем данные в памяти для последующего сохранения в файл
		memcpy(client.login[client.cur_client_size], login, LOGINSIZE);
		memcpy(&client.c_addr[client.cur_client_size], &from_address, from_addrlen);
		
		printf("Connection with %s\n", client.login[client.cur_client_size]);
		// Сохраняем данные в файл
		save_client(&client);
		// Увеличиваем счетчик клиентов
		client.cur_client_size++;
	}

	#if OS == OS_MAC || OS == OS_UNIX
	close(acc);
	close(tcp.socket);
    close(udp.socket);
    #elif OS == OS_WINDOWS
	closesocket(acc);
    closesocket(tcp.socket);
    closesocket(udp.socket);
    #endif
	return 0;
}

void save_client(struct Clients *client) {
	char *c;
    int clientsize = sizeof(struct Clients);
	FILE *fc = fopen(FILENAME, "wb");
	if(fc == NULL) {
		handler.sys_error("File");
	}
	c = (char *)client;
    // посимвольно записываем в файл структуру
    for (int i = 0; i < clientsize; i++) {
        putc(*c++, fc);
    }
    fclose(fc);
}

void load_client(struct Clients *client) {
    char *c;
    int sym;
    int clientsize = sizeof(struct Clients);
 
    FILE *fc = fopen(FILENAME, "rb");
	if(fc == NULL) {
		handler.sys_error("File");
	}
    c = (char *)client;
    while ((sym = getc(fc)) != EOF) {
        *c = sym;
        c++;
    }
    fclose(fc);
}

void send_to_clients(int socket, int register_port, struct Clients client, const char *incoming) {
	int j = 0;
	int s_bytes;
	for(;j <= client.cur_client_size; j++) {
		if(client.c_addr[j].sin_port == register_port) continue;
		if((s_bytes = sendto(socket, incoming, getsize(incoming), 0, (struct sockaddr *)&client.c_addr[j], sizeof(struct sockaddr))) == -1) {
			handler.sys_error("Send to clients");
		}
	}
}

void send_notify(int socket, struct Clients client, const char *incoming) {
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

int sys_error(char *msg) {
	#if OS == OS_MAC || OS == OS_UNIX
	close(tcp.socket);
    close(udp.socket);
    #elif OS == OS_WINDOWS
    closesocket(tcp.socket);
    closesocket(udp.socket);
    #endif
	perror(msg);
	exit(1);
}

