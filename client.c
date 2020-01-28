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
#define TOKENSIZE 32
#define CMDSIZE 6

char TOKEN[TOKENSIZE] = "LOGIN::";

void printub(const char *);
int getsize(const char *);
void remove_nl(char *);
void multistrcat(char *, ...);

void sig_alarm(int);
void sig_int(int);
void sig_stub(int);

int sys_error(char *);

enum Family { FAM = AF_INET, SOCK = SOCK_DGRAM };

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
	int socket; // сокет
	struct sockaddr_in s_addr;
} conn = {
	"127.0.0.1",
	7654
};

struct SystemHandlers {
	void (*sig_alarm) ();
	void (*sig_int) ();
	void (*sig_stub) ();
	int (*sys_error) (char *);
} hdl = {
	&sig_alarm,
	&sig_int,
	&sig_stub,
	&sys_error,
};

void sigaction_init(int sig, void handler());

int has_connect(struct Connection, char *token);

void send_message_to(struct Connection, const char *);

int main() {
	sigaction_init(SIGINT, hdl.sig_int);

	printub("\n*****************************************************\n");
	printub("******** Welcome to CLC (Cool Little Chat)! *********\n");
	printub("*****************************************************\n");
	printub("*****************************************************\n");
	printub("\nTo start chatting you must set a name to your friends recognize you\n");
	printub("-\n");
	printub("Print your chat-name here: ");
	bzero(user.login, sizeof(user.login));
	// >>
	read(0, user.login, sizeof(user.login));
	// <<
	remove_nl(user.login);
	printub("Wait for connection...\n");

	conn.s_addr.sin_family = FAM;
	conn.s_addr.sin_port = htons(conn.port);
	if(!inet_aton(conn.host, &conn.s_addr.sin_addr)) {
		hdl.sys_error("Invalid address");
	}
	conn.socket = socket(FAM, SOCK, 0);
	if(conn.socket == -1) {
		hdl.sys_error("Socket");
	} else {
		// Неблокирующая работа сокета (для recv)
		fcntl(conn.socket, F_SETFL, fcntl(conn.socket, F_GETFL) | O_NONBLOCK);
		strcat(TOKEN, user.login);
		if(has_connect(conn, TOKEN)) {
			printf("\nConnection with %s:%d established\n", conn.host, conn.port);
			printf("You start chatting by name - %s\n\n", user.login);
			printub("============ CHAT RIGHT NOW ============\n\n");
			char notify[SMALLMESSAGESSIZE];
			multistrcat(notify, "\t\t", user.login, " join this chat\n", "\0");
			send_message_to(conn, notify);
		} else {
			hdl.sys_error("Socket connection");
		}
	}

	// Принимаем сообщения чепез дочерний процесс
	if(fork() == 0) {
		// Чтоб сообщение о выходе из чата не задваивалось 
		// при перехвате сигнала, добавляем перехватчик и в дочернем процессе 
		sigaction_init(SIGINT, hdl.sig_stub);
		char incoming[MESSAGESSIZE];
		int max_messages = 0;
		int timeout = 0;
		do {
			bzero(incoming, MESSAGESSIZE);
			if(timeout > 120) {
				// Убить текущий дочерний процесс и создать новый
			};
			int r_bytes =  recv(conn.socket, incoming, sizeof(incoming), 0);
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
		// >>
		read(0, reading, sizeof(reading));
		// <<
		if(!strncmp(reading, cmd.name, CMDSIZE)) {
			// Сообщение направлено конкретному адресату
			
		}
		if(!strncmp(reading, cmd.exit, CMDSIZE)) {
			// Пользователь покинул чат
			
		}
		char outgoing[MESSAGESSIZE + LOGINSIZE];
		multistrcat(outgoing, user.login, ": ", reading, "\0");
		send_message_to(conn, outgoing);
	} while(1);
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
	send_message_to(conn, notify);
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
	return(1);
}

int has_connect(struct Connection conn, char * TOKEN) {
	char incoming[MESSAGESSIZE];
	bzero(incoming, MESSAGESSIZE);
	int messagecount = 0;
	while(messagecount < MESSAGESSIZE && !incoming[0]) {
		// Тестовый запрос для регистрации и отправка логина
		// 100 раз отправляем тестовое сообщение
		send_message_to(conn, TOKEN);
		// ...и ждем ответа
		recv(conn.socket, incoming, sizeof(incoming), 0);
		messagecount++;
		sleep(1);
	}
	return !(messagecount == MESSAGESSIZE && !incoming[0]);
}

void send_message_to(struct Connection conn, const char * message) {
	int s = sendto(conn.socket, message, getsize(message), 0, (struct sockaddr*)&conn.s_addr, sizeof(conn.s_addr));
	if(s == -1) {
		hdl.sys_error("Send");
	}
}