#include "mods/inc.h"
#include "mods/base.h"
#include "mods/handlers.h"

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
	short socket;
	int port;
} udp = {
	-1,
	7654
};

struct UserData {
	char login[LOGSIZE];
} user;

struct ClientBuffer {
	char type[20];
	char login[LOGSIZE];
	char message[MSGSIZE];
	char command[10];
} client_buffer;

struct ServerBuffer {
	char type[20]; // open, close
	char message[MSGSIZE];
	char zero[30]; // для конвертации строки к struct ServerBuffer
} server_buffer;

void Connect(struct Connection *);

void Close_connection();

void convert_to_string(void *, char [], int);

int main() {
	sigaction_init(SIGINT, Close_connection);

	char reading[BUFSIZE];
	char incoming[BUFSIZE];
	char outgoing[BUFSIZE];

	bzero(user.login, LOGSIZE);
	bzero(reading, BUFSIZE);
	bzero(incoming, BUFSIZE);
	bzero(outgoing, BUFSIZE);
	memset(server_buffer.zero, '\0', 30);

	printf("\n*****************************************************\n");
	printf("******** Welcome to CLC (Cool Little Chat)! *********\n");
	printf("*****************************************************\n");
	printf("*****************************************************\n");
	printf("-\n");

	
	Connect(&udp);

	printf("To start chatting you must input your login so that your friends could write you\n");
	printf("-\n");
	printub("Print your login here: ");

	read(0, user.login, sizeof(user.login));
	remove_nl(user.login);
	
	strcpy(client_buffer.type, "register");
	strcpy(client_buffer.login, user.login);
	convert_to_string(&client_buffer, outgoing, BUFSIZE); // void (src, dest, size_of_dest)
	write(udp.socket, outgoing, BUFSIZE);

	printf("Wait for connection...\n");

	if(read(udp.socket, incoming, BUFSIZE) == -1) {
		handler.sys_error("Server connection");
	}
	strcpy(server_buffer.type, ((struct ServerBuffer *)incoming)->type);
	strcpy(server_buffer.message, ((struct ServerBuffer *)incoming)->message);

	printub(server_buffer.message);
	
	printf("\n============ HELLO, %s ============\n\n", user.login);

	if(fork() == 0) {
		sigaction_init(SIGINT, handler.sig_stub);
		sigaction_init(SIGALRM, handler.sig_alarm);
		do {
			bzero(incoming, BUFSIZE);
			read(udp.socket, incoming, BUFSIZE);
			strcpy(server_buffer.type, ((struct ServerBuffer *)incoming)->type);
			strcpy(server_buffer.message, ((struct ServerBuffer *)incoming)->message);
			if(!strcmp(server_buffer.type, "open")) {
				printub(server_buffer.message);
			} else if(!strcmp(server_buffer.type, "close")) {
				printub(server_buffer.message);
				alarm(1);
			}
		} while(1);
		exit(0);
	}

	strcpy(client_buffer.type, "message");
	do {
		bzero(reading, BUFSIZE);
		bzero(outgoing, BUFSIZE);
		read(0, reading, sizeof(reading));
		strcpy(client_buffer.message, reading);
		convert_to_string(&client_buffer, outgoing, BUFSIZE);
		write(udp.socket, outgoing, BUFSIZE);
	} while(1);

	shutdown(udp.socket, SHUT_RDWR);

    return 0;
}

void Connect(struct Connection *conn) {
	struct sockaddr_in host_address;
	conn->socket = socket(FAM, SOCK, 0);
	int opt = 1;
	setsockopt(conn->socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if(conn->socket == -1) {
		handler.sys_error("Failed socket connection");
	}
	host_address.sin_family = FAM;
	host_address.sin_port = htons(conn->port);
	host_address.sin_addr.s_addr = INADDR_ANY;
	memset(&(host_address.sin_zero), '\0', 8);
	connect(conn->socket, (struct sockaddr*)&host_address, sizeof(host_address));
}

void Close_connection() {
	// Если сервер отключился, отправить сообщение всем клиентам
	// и инициировать у них отключение и повторное подключение
	char notify[BUFSIZE];
	strcpy(client_buffer.type, "close");
	convert_to_string(&client_buffer, notify, BUFSIZE);
	write(udp.socket, notify, BUFSIZE);
	handler.sig_int(SIGINT);
}

void convert_to_string(void *buffer, char request[], int size) {
	char *c;
	c = (char *)buffer;
	// посимвольно записываем структуру в сообщение
	for (int i = 0; i < size; i++) {
		memset(request + i, *(c + i), 1);
	}
}