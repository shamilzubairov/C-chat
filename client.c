#include "inc.h"
#include "base.h"
#include "handlers.h"

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

enum Family { FAM = AF_INET, SOCK = SOCK_DGRAM };

struct Connection {
	char *host;
	int port;
	int socket;
	struct sockaddr_in address;
} udp = {
	"127.0.0.1",
	7654
};

struct UserData {
	char login[LOGSIZE];
} user;

struct Message {
	char type[20];
	char login[LOGSIZE];
	char message[MSGSIZE];
	char command[10];
} msg;

void Connect(struct Connection *);

void convert_to_string(void *, char [], int);

int main() {
    sigaction_init(SIGINT, handler.sig_int);

	char reading[BUFSIZE];
	char incoming[BUFSIZE];
	char outgoing[BUFSIZE];

	bzero(user.login, LOGSIZE);
	bzero(reading, BUFSIZE);
	bzero(incoming, BUFSIZE);
	bzero(outgoing, BUFSIZE);

	printf("\n*****************************************************\n");
	printf("******** Welcome to CLC (Cool Little Chat)! *********\n");
	printf("*****************************************************\n");
	printf("*****************************************************\n");
	printf("-\n");
	
	Connect(&udp);

	// Приветствуем
	// Отправляем логин
	printf("To start chatting you must input your login so that your friends could write you\n");
	printf("-\n");
	printub("Print your login here: ");

	read(0, user.login, sizeof(user.login));
	remove_nl(user.login);
	
	strcpy(msg.type, "register");
	strcpy(msg.login, user.login);

	convert_to_string(&msg, outgoing, BUFSIZE); // void (src, dest, size_of_dest)
    write(udp.socket, outgoing, BUFSIZE);

	printf("Wait for connection...\n");

	// Ждать ответа от сервера ...
	// alarm(60);
	// sigaction_init(SIGALRM, handler.sig_alarm);
	read(udp.socket, incoming, BUFSIZE);

	printub(incoming);
	
	printf("\n============ HELLO, %s ============\n\n", user.login);

	if(fork() == 0) {
		sigaction_init(SIGINT, handler.sig_stub);
		do {
			bzero(incoming, BUFSIZE);
			read(udp.socket, incoming, BUFSIZE);
            printub(incoming);
		} while(1);
		exit(0);
	}

	strcpy(msg.type, "message");
	do {
		bzero(reading, BUFSIZE);
		bzero(outgoing, BUFSIZE);
		read(0, reading, sizeof(reading));
		strcpy(msg.message, reading);

		convert_to_string(&msg, outgoing, BUFSIZE);
		write(udp.socket, outgoing, BUFSIZE);
	} while(1);

	close(udp.socket);

    return 0;
}

void Connect(struct Connection *conn) {
	conn->socket = socket(FAM, SOCK, 0);
	if(conn->socket == -1) {
		handler.sys_error("Failed socket connection");
	}
	conn->address.sin_family = FAM;
	conn->address.sin_port = htons(conn->port);
	if(!inet_aton(conn->host, &(conn->address.sin_addr))) {
		handler.sys_error("Invalid address");
	}

	int opt = 1;
	setsockopt(conn->socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	connect(conn->socket, (struct sockaddr*)&(conn->address), sizeof(conn->address));
}

void convert_to_string(void *msg, char request[], int size) {
	char *c;
	c = (char *)msg;
	// посимвольно записываем структуру в сообщение
	for (int i = 0; i < size; i++) {
		memset(request + i, *(c + i), 1);
	}
}