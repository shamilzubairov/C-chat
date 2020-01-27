#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h> // sockaddr_in
#include <arpa/inet.h> // htons

#define HOST "127.0.0.1"
#define PORT 7654

char TOKEN[32] = "LOGIN::";

void printub(const char *);
int getsize(const char *);
void remove_nl(char *);

void sig_alarm(int);
void sig_int(int);
void sig_stub(int);

int sys_error(char *);

enum Family { FAM = AF_INET, SOCK = SOCK_DGRAM };

struct Commands {
	char name[6];
	char mail[6]; // сообщение нескольким адресатам
	char exit[6];
} cmd = {
	"::name",
	"::mail",
	"::exit"
};

struct UserData {
	char login[20];
} user;

struct Messages {
	char incoming[100]; // входящее сообщение (макс. длина 100 байт)
	char outgoing[100]; // исходящее сообщение (макс. длина 100 байт)
} msg;

struct Connection {
	int socket; // сокет
	struct sockaddr_in s_addr;
} conn;

struct System {
	void (*sig_alarm) ();
	void (*sig_int) ();
	void (*sig_stub) ();
	int (*sys_error) (char *);
} hdl = {
	&sig_alarm,
	&sig_int,
	&sig_stub,
	&sys_error
};

int has_connect(struct Connection, struct Messages, char *token);

void send_message_to(struct Connection, const char *);

// -- SIGACTION --
// struct sigaction act;
// memset(&act, 0, sizeof(act));
// sigset_t set; 
// sigemptyset(&set);
// sigaddset(&set, SIGINT);
// act.sa_mask = set;
// act.sa_handler = sigint_handler;
// sigaction(SIGINT, &act, 0);
// -------------------------------- //
// -------------------------------- //
// -------------------------------- //
// -------------------------------- //
// -------------------------------- //

int main() {
	alarm(120);
	signal(SIGALRM, sig_alarm);
	signal(SIGINT, sig_int);

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
	conn.s_addr.sin_port = htons(PORT);
	if(!inet_aton(HOST, &conn.s_addr.sin_addr)) {
		hdl.sys_error("Invalid address");
	}
	conn.socket = socket(FAM, SOCK, 0);
	if(conn.socket == -1) {
		hdl.sys_error("Socket");
	} else {
		// Неблокирующая работа сокета (для recv)
		fcntl(conn.socket, F_SETFL, fcntl(conn.socket, F_GETFL) | O_NONBLOCK);
		strcat(TOKEN, user.login);
		if(has_connect(conn, msg, TOKEN)) {
			printf("\nConnection with %s:%d established\n", HOST, PORT);
			printf("You start chatting by name - %s\n\n", user.login);
			printub("============ CHAT RIGHT NOW ============\n\n");

			char message[40] = "\t\t";
			strcat(message, user.login);
			strcat(message, " join this chat\n");

			send_message_to(conn, message);
		} else {
			hdl.sys_error("Socket connection");
		}
	}

	// Принимаем сообщения чепез дочерний процесс
	if(fork() == 0) {
		// Чтоб сообщение о выходе из чата не задваивалось 
		// при перехвате сигнала, добавляем перехватчик и в дочернем процессе 
		signal(SIGINT, sig_stub); 
		
		int max_messages = 0;
		do {
			if(max_messages > 100) break;
			int r_bytes =  recv(conn.socket, msg.incoming, sizeof(msg.incoming), 0);
			if(r_bytes == -1) {
				sleep(1);
				continue;
			} else {
				printub(msg.incoming);
			}
			max_messages++;
		} while(1);

		exit(0);
	}
	
	do {
		bzero(msg.outgoing, 100);
		// >>
		read(0, msg.outgoing, sizeof(msg.outgoing));
		// <<
		if(!strncmp(msg.outgoing, cmd.name, 6)) {
			// Сообщение направлено конкретному адресату
			
		}
		if(!strncmp(msg.outgoing, cmd.exit, 6)) {
			// Пользователь покинул чат
			
		}

		char full_message[sizeof(user.login) + sizeof(msg.outgoing) + 2];
		bzero(full_message, sizeof(user.login) + sizeof(msg.outgoing) + 2);
		strcat(full_message, user.login);
		strcat(full_message, ": ");
		strcat(full_message, msg.outgoing);

		send_message_to(conn, full_message);
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

// Изменить на sigaction
void sig_alarm(int sig) {
	printub("Alarm::socket didn\'t get any response\n");
	exit(1);
}

void sig_int(int sig) {
	char message[40] = "\t\t";
	strcat(message, user.login);
	strcat(message, " leave this chat\n");
	sendto(conn.socket, message, sizeof(message), 0, (struct sockaddr*)&conn.s_addr, sizeof(conn.s_addr));
	exit(0);
}

void sig_stub(int sig) {
	exit(0);
}

int sys_error(char * msg) {
	perror(msg);
	return(1);
}

int has_connect(struct Connection conn, struct Messages msg, char * TOKEN) {
	int timer = 0;
	while(timer < 100 && !msg.incoming[0]) {
		// Тестовый запрос для регистрации и отправка логина
		// 100 раз отправляем тестовое сообщение
		send_message_to(conn, TOKEN);
		// ...и ждем ответа
		recv(conn.socket, msg.incoming, sizeof(msg.incoming), 0);
		timer++;
		sleep(1);
	}
	return !(timer == 100 && !msg.incoming[0]);
}

void send_message_to(struct Connection conn, const char * message) {
	int s = sendto(conn.socket, message, getsize(message), 0, (struct sockaddr*)&conn.s_addr, sizeof(conn.s_addr));
	if(s == -1) {
		hdl.sys_error("Send");
	}
}