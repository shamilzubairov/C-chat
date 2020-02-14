#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <stdarg.h>
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

#define MESSAGESSIZE 100
#define SMALLMESSAGESSIZE 50
#define LOGINSIZE 20

void printub(const char *);
int getsize(const char *);
void remove_nl(char *);
void multistrcat(char *, ...);
void itoa(int, char *);
void reverse(char *);

void sig_alarm(int);
void sig_int(int);
void sig_stub(int);

int sys_error(char *);

enum Family { FAM = AF_INET, SOCK = SOCK_DGRAM };

struct Connection {
	char * host;
	int port;
	int socket;
	struct sockaddr_in address;
} udp = {
	"127.0.0.1",
	7666,
	-1,
	0
};

// struct Commands {
// 	char name[CMDSIZE];
// 	char mail[CMDSIZE]; // сообщение нескольким адресатам
// 	char exit[CMDSIZE];
// } cmd = {
// 	"name",
// 	"mail",
// 	"exit"
// };

struct UserData {
	char login[LOGINSIZE];
} user;

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
	sigaction_init(SIGINT, handler.sig_int);

	printub("\n*****************************************************\n");
	printub("******** Welcome to CLC (Cool Little Chat)! *********\n");
	printub("*****************************************************\n");
	printub("*****************************************************\n");
	printub("-\n");
	
	udp.socket = socket(FAM, SOCK, 0);
	if(udp.socket == -1) {
		handler.sys_error("Failed socket connection");
	}
	udp.address.sin_family = FAM;
	udp.address.sin_port = htons(udp.port);
	if(!inet_aton(udp.host, &udp.address.sin_addr)) {
		handler.sys_error("Invalid address");
	}
	connect(udp.socket, (struct sockaddr*)&udp.address, sizeof(udp.address));
	#if OS == OS_MAC || OS == OS_UNIX
	fcntl(udp.socket, F_SETFL, fcntl(udp.socket, F_GETFL) | O_NONBLOCK);
	#elif OS == OS_WINDOWS
	DWORD nonBlocking = 1;
	ioctlsocket(handle, FIONBIO, &nonBlocking);
	#endif

	printub("Wait for connection...\n");
	
	// Установка соединения
	// srand(time(NULL));
	// int random_token = rand();

	char response[SMALLMESSAGESSIZE];
	bzero(response, SMALLMESSAGESSIZE);
	int r_bytes = -1;
	int attempt = 10;
	while(attempt > 0) {
		write(udp.socket, "SYN", 3);
		attempt--;
		sleep(1);
		r_bytes = read(udp.socket, response, SMALLMESSAGESSIZE);
		if(!strcmp("ACK", response)) {
			if(write(udp.socket, "SYN-ACK", 7) == -1) {
				handler.sys_error("No synchronize with server");
			};
			break;
		}
	}
	if(attempt == 0 && r_bytes < 0 && strcmp("ACK", response)) {
		handler.sys_error("Server didn\'t send any response");
	} else {
		printf("\nConnection with %s:%d established by PID - %d\n", udp.host, udp.port, getpid());
		// Приветствуем
		// Отправляем логин
		printub("\nTo start chatting you must set a name to your friends recognize you\n");
		printub("-\n");
		printub("Print your chat-name here: ");
		
		bzero(user.login, sizeof(user.login));
		read(0, user.login, sizeof(user.login));
		remove_nl(user.login);
		write(udp.socket, user.login, LOGINSIZE);
		
		printf("You start chatting by name - %s\n\n", user.login);
		printub("============ CHAT RIGHT NOW ============\n\n");
	}

	if(fork() == 0) {
		sigaction_init(SIGINT, handler.sig_stub);
		char incoming[MESSAGESSIZE];
		int max_messages = 0;
		int timeout = 0;
		do {
			bzero(incoming, MESSAGESSIZE);
			if(timeout > 120) {
				// Убить текущий дочерний процесс и создать новый
			};
			int r_bytes = read(udp.socket, incoming, sizeof(incoming));
			if(r_bytes == -1) {
				timeout++;
				usleep(500);
				continue;
			} else {
				printub(incoming);
			}
			max_messages++;
		} while(1);
		exit(0);
	}

	char reading[MESSAGESSIZE];
	do {
		bzero(reading, MESSAGESSIZE);
		read(0, reading, sizeof(reading));
		char outgoing[MESSAGESSIZE + LOGINSIZE];
		multistrcat(outgoing, user.login, ": ", reading, "\0");
		write(udp.socket, outgoing, sizeof(outgoing));
	} while(1);

	#if OS == OS_MAC || OS == OS_UNIX
    close(udp.socket);
    #elif OS == OS_WINDOWS
    closesocket(udp.socket);
    #endif
	return 0;
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

void itoa(int n, char s[]) {
    int i, sign;
    if((sign = n) < 0) /* сохраняем знак */
        n = -n; /* делаем n положительным */
    i = 0;
    do { /* генерируем цифры в обратном порядке */
        s[i++] = n % 10 + '0'; /* следующая цифра */
    } while ((n /= 10) > 0); /* исключить ее */
    if (sign < 0)
        s[i++] = '-';
    s[i] = '\0';
    reverse(s);
}

void reverse(char s[]) {
	int c, i, j;
	for (i = 0, j = getsize(s) - 1; i < j; i++, j--) {
		c = s[i];
		s[i] = s[j];
		s[j] = c;
	} 
}

// Изменить на sigaction
void sig_alarm(int sig) {
	printub("Alarm::socket didn\'t get any response\n");
	exit(1);
}

void sig_int(int sig) {
	char notify[SMALLMESSAGESSIZE];
	multistrcat(notify, "\t\t", user.login, " leave this chat\n", "\0");
	write(udp.socket, notify, sizeof(notify));
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
