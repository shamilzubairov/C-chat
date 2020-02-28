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
	7654,
	-1,
	0
};

struct sockaddr_in clients[MAXCLIENTSSIZE];

struct Message {
	char type[20]; // message, registration, command ...
	char login[LOGSIZE];
	char message[MSGSIZE];
	char command[10]; // name, group, exit ...
} msg;

void Connect(struct Connection *);

void save_messages(const char *, const char *);

void save_client(const char *, struct sockaddr_in *, int);

void load_clients(const char *, struct sockaddr_in []);

void send_to_clients(int, struct sockaddr_in [], int, const char []);

int main() {
	sigaction_init(SIGALRM, handler.sig_alarm);
	sigaction_init(SIGINT, handler.sig_int);
	
	int current_clients_size = 0;
	char incoming[BUFSIZE];
	char outgoing[BUFSIZE];
	struct sockaddr_in from_address;
	socklen_t from_addrlen = sizeof(struct sockaddr_in);
	
	// Очищаем файл
	FILE *fc = fopen(FCLIENTS, "w");
	fclose(fc);
	FILE *fm = fopen(FMESSAGES, "w");
	fclose(fm);

	Connect(&udp);

	// ...
	printf("Server is working by PORT - %d\n\n", udp.port);
	
	do {
		printf("\n================\n");
		printf("CLIENTS COUNT %d\n", current_clients_size);
		printf("================\n\n");
		if(current_clients_size > MAXCLIENTSSIZE) {
			// Достигнуто макс. кол-во клиентов
			// Отослать уведомление
		}
		bzero(incoming, BUFSIZE);
		bzero(outgoing, BUFSIZE);
		bzero(&from_address, from_addrlen);
		// Ожидаем сообщение от клиента
		recvfrom(udp.socket, incoming, BUFSIZE, 0, (struct sockaddr *)&from_address, &from_addrlen);
		// Ответ пришел, копируем в структуру
		strcpy(msg.type, ((struct Message *)incoming)->type);
		strcpy(msg.login, ((struct Message *)incoming)->login);
		strcpy(msg.message, ((struct Message *)incoming)->message);
		strcpy(msg.command, ((struct Message *)incoming)->command);

		load_clients(FCLIENTS, clients);
		if(!strcmp(msg.type, "message")) {
			printf("===CHAT===\n");
			multistrcat(outgoing, msg.login, ": ", msg.message, "\0");
			send_to_clients(udp.socket, clients, current_clients_size, outgoing);
			save_messages(FMESSAGES, outgoing);
		} else if(!strcmp(msg.type, "register")) {
			printf("===REGISTER===\n");
			current_clients_size++;
			if(fork() == 0) {
				// Регистрируем участника
				sigaction_init(SIGINT, handler.sig_stub);
				printf("Connection with %s (by port - %d)\n\n", msg.login, from_address.sin_port);

				// Отправляем клиенту ответ 
				// и файл с предыдущими сообщениями...
				bzero(outgoing, BUFSIZE);
				sprintf(
					outgoing, 
					"\nConnection established by %s:%d\n", 
					udp.host,
					udp.port
				);
				sendto(udp.socket, outgoing, BUFSIZE, 0, (struct sockaddr *)&from_address, from_addrlen);
				
				bzero(outgoing, BUFSIZE);
				FILE *fm = fopen(FMESSAGES, "rb");
				char message[MSGSIZE];
				while(!feof(fm)) {
					fgets(message, MSGSIZE, fm);
					strcat(outgoing, message);
				}
				sendto(udp.socket, outgoing, BUFSIZE, 0, (struct sockaddr *)&from_address, from_addrlen);

				// Отправляем всем клиентам приветствие ...
				multistrcat(outgoing, "\t\t", msg.login, " join this chat\n", "\0");
				send_to_clients(udp.socket, clients, current_clients_size, outgoing);

				// Сохраняем данные в файл
				save_client(FCLIENTS, &from_address, from_addrlen);

				exit(0);
			}
		}

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
	
	if(bind(conn->socket, (struct sockaddr *)&(conn->address), sizeof(conn->address)) == -1) {
		handler.sys_error("Failed bind UDP connection");
	}
}

void save_messages(const char *filename, const char *messages) {
	FILE *fm = fopen(filename, "ab");
	if(fm == NULL) {
		handler.sys_error("File");
	}
	fputs(messages, fm);
    fclose(fm);
}

void save_client(const char *filename, struct sockaddr_in *client, int clientsize) {
	char *c;
	FILE *fc = fopen(filename, "ab");
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

void load_clients(const char *filename, struct sockaddr_in clients[]) {
    char *c;
    int sym;
 
    FILE *fc = fopen(filename, "rb");
	if(fc == NULL) {
		handler.sys_error("File");
	}
    c = (char *)clients;
    while ((sym = getc(fc)) != EOF) {
        *c = sym;
        c++;
    }
    fclose(fc);
}

void send_to_clients(int socket, struct sockaddr_in clients[], int clientsize, const char message[]) {
	int p = 0;
	printf(">>> SEND MESSAGE: %s", message);
	for(;p < clientsize; p++) {
		printf("%d) TO %d\n", p, clients[p].sin_port);
		sendto(socket, message, getsize(message), 0, (struct sockaddr *)&clients[p], sizeof(struct sockaddr));
	}
}


