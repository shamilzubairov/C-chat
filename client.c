#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netinet/in.h> // sockaddr_in
#include <arpa/inet.h> // htons

#define MESSAGESSIZE 100
#define SMALLMESSAGESSIZE 50
#define LOGINSIZE 20
#define CMDSIZE 6

void printub(const char *);
int getsize(const char *);
void remove_nl(char *);
void multistrcat(char *, ...);

void sig_alarm(int);
void sig_int(int);
void sig_stub(int);

int sys_error(char *);

enum Family { FAM = AF_INET, SOCK = SOCK_STREAM, SOCK_UDP = SOCK_DGRAM };

struct Commands {
	char name[CMDSIZE];
	char mail[CMDSIZE]; // сообщение нескольким адресатам
	char exit[CMDSIZE];
} cmd = {
	"::name",
	"::mail",
	"::exit"
};

struct UserData {
	char login[LOGINSIZE];
} user;

struct Connection {
	char *host;
	int port;
	int attempt; // Кол-во попыток подключения
	int socket;
	struct sockaddr_in s_addr;
} conn = {
	"127.0.0.1",
	7654,
	20,
	-1,
	0
};

struct UDPConnection {
	char *host;
	int port;
	int socket;
	struct sockaddr_in s_addr;
} udp = {
	"127.0.0.1",
	7660,
	-1,
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

int main() {
	sigaction_init(SIGINT, handler.sig_int);

	printub("\n*****************************************************\n");
	printub("******** Welcome to CLC (Cool Little Chat)! *********\n");
	printub("*****************************************************\n");
	printub("*****************************************************\n");
	printub("\nTo start chatting you must set a name to your friends recognize you\n");
	printub("-\n");
	printub("Print your chat-name here: ");
	bzero(user.login, sizeof(user.login));
	read(0, user.login, sizeof(user.login));
	remove_nl(user.login);
	printub("Wait for connection...\n");

	conn.socket = socket(FAM, SOCK, 0);
	if(conn.socket == -1) {
		handler.sys_error("Socket stream connection");
	}
	conn.s_addr.sin_family = FAM;
	conn.s_addr.sin_port = htons(conn.port);
	if(!inet_aton(conn.host, &conn.s_addr.sin_addr)) {
		handler.sys_error("Invalid address");
	}
	int trytoconnect = 0,
		connection;
	do {
		connection = connect(conn.socket, (struct sockaddr*)&conn.s_addr, sizeof(conn.s_addr));
		if(connection != -1) break;
		// Ждем -> Закрываем и заново открываем сокет
		sleep(1);
		close(conn.socket);
		conn.socket = socket(FAM, SOCK, 0);
		trytoconnect++;
	} while (trytoconnect < conn.attempt);
	if(connection == -1) {
		handler.sys_error("Connection stream failed");
	} else {
		// Регистрация
		write(conn.socket, user.login, LOGINSIZE);
		printf("\nConnection with %s:%d established by PID - %d\n", conn.host, conn.port, getpid());
		printf("You start chatting by name - %s\n\n", user.login);
		printub("============ CHAT RIGHT NOW ============\n\n");
	}

// >>>>>>>>>>>>>>>>>> UDP >>>>>>>>>>>>>>>>>>>>>
	
	udp.socket = socket(FAM, SOCK_UDP, 0);
	if(udp.socket == -1) {
		handler.sys_error("UDP: socket connection");
	}
	udp.s_addr.sin_family = FAM;
	udp.s_addr.sin_port = htons(udp.port);
	if(!inet_aton(udp.host, &udp.s_addr.sin_addr)) {
		handler.sys_error("Invalid address");
	}
	if(connect(udp.socket, (struct sockaddr*)&udp.s_addr, sizeof(udp.s_addr)) == -1) {
		handler.sys_error("UDP connection");
	}

	if(fork() == 0) {
		sigaction_init(SIGINT, handler.sig_stub);

		char greeting[SMALLMESSAGESSIZE];
		multistrcat(greeting, "\t\t", user.login, " join this chat\n", "\0");
		if(write(udp.socket, greeting, SMALLMESSAGESSIZE) == -1) {
			handler.sys_error("Write (greeting)");
		}

		char incoming[MESSAGESSIZE];
		do {
			bzero(incoming, MESSAGESSIZE);
			read(udp.socket, incoming, sizeof(incoming));
			printub(incoming);
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

	close(udp.socket);
	close(conn.socket);
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
