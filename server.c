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
	char *host;
	int port;
	int socket;
	struct sockaddr_in address;
} udp = {
	"127.0.0.1",
	7654
};

struct sockaddr_in clients[MAXCLIENTSSIZE];

struct Message {
	char type[20]; // message, registration, command, leave ...
	char login[LOGSIZE];
	char message[MSGSIZE];
	char command[10]; // name, group, exit ...
} msg;

struct ServerMessage {
	char type[20]; // open, close
	char message[MSGSIZE];
} s_msg;

void Connect(struct Connection *);

void Closed();

void save_messages(const char *, const char *);

void load_messages(const char *, char *);

void save_client(const char *, struct sockaddr_in *, int);

void load_clients(const char *, struct sockaddr_in []);

void send_to_clients(int, struct sockaddr_in [], const char []);

void convert_to_string(void *, char [], int);

int main() {
	sigaction_init(SIGALRM, handler.sig_alarm);
	sigaction_init(SIGINT, Closed);
	
	int current_clients_size = 0;
	char incoming[BUFSIZE];
	char outgoing[BUFSIZE];
	struct sockaddr_in from_address;
	socklen_t from_addrlen = sizeof(struct sockaddr_in);
	
	// Очищаем файлы
	FILE *fc = fopen(FCLIENTS, "w");
	fclose(fc);
	FILE *fm = fopen(FMESSAGES, "w");
	fclose(fm);

	Connect(&udp);

	// ...
	printf("Server is working by PORT - %d\n\n", htons(udp.port));
	
	do {
		printf("\n================\n");
		printf("CLIENTS COUNT %d\n", current_clients_size);
		printf("================\n\n");
		if(current_clients_size > MAXCLIENTSSIZE) {
			// Достигнуто макс. кол-во клиентов
			// Отослать уведомление
		}
		load_clients(FCLIENTS, clients);

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

		if(!strcmp(msg.type, "message")) {
			printf("===CHAT===\n");
			multistrcat(outgoing, msg.login, ": ", msg.message, "\0");
			send_to_clients(udp.socket, clients, outgoing);
			save_messages(FMESSAGES, outgoing);
		} else if(!strcmp(msg.type, "register")) {
			printf("===REGISTER===\n");
			// Создаем параллельный дочерний процесс для нового участника
			if(fork() == 0) {
				// Регистрируем участника
				sigaction_init(SIGINT, handler.sig_stub);
				printf("Connection with %s (by port - %d)\n\n", msg.login, htons(from_address.sin_port));
				// Отправляем клиенту ответ 
				bzero(outgoing, BUFSIZE);
				sprintf(
					outgoing, 
					"\nConnection established by %s:%d\n", 
					udp.host,
					htons(udp.port)
				);
				// convert_to_string(&s_msg, outgoing, BUFSIZE);
				sendto(udp.socket, outgoing, BUFSIZE, 0, (struct sockaddr *)&from_address, from_addrlen);
				
				// ...и файл с предыдущими сообщениями
				bzero(outgoing, BUFSIZE);
				load_messages(FMESSAGES, outgoing);
				sendto(udp.socket, outgoing, BUFSIZE, 0, (struct sockaddr *)&from_address, from_addrlen);

				exit(0);
			} // ------ fork end ------ //
			// Отправляем всем участникам приветствие от нового...
			multistrcat(outgoing, "\t\t", msg.login, " join this chat\n", "\0");
			send_to_clients(udp.socket, clients, outgoing);
			// Сохраняем данные в файл
			save_client(FCLIENTS, &from_address, from_addrlen);
			// Увеличиваем счетчик клиентов
			current_clients_size++;
		}

	} while(1);

    close(udp.socket);

    return 0;
}

void Connect(struct Connection *conn) {
	conn->socket = socket(FAM, SOCK, 0);
	int opt = 1;
	setsockopt(conn->socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	
	if(conn->socket == -1) {
		handler.sys_error("Failed socket connection");
	}
	conn->address.sin_family = FAM;
	conn->address.sin_port = htons(conn->port);
	if(!inet_aton(conn->host, &(conn->address.sin_addr))) {
		handler.sys_error("Invalid address");
	}
	
	if(bind(conn->socket, (struct sockaddr *)&(conn->address), sizeof(conn->address)) == -1) {
		handler.sys_error("Failed bind UDP connection");
	}
}

void Closed() {
	// Если сервер отключился, отправить сообщение всем клиентам
	// и инициировать у них отключение и повторное подключение
	char notify[] = "\t\tConnection closed\n";
	send_to_clients(udp.socket, clients, notify);
	handler.sig_int(SIGINT);
}

void save_messages(const char *filename, const char *messages) {
	FILE *fm = fopen(filename, "ab");
	if(fm == NULL) {
		handler.sys_error("File");
	}
	fputs(messages, fm);
    fclose(fm);
}

void load_messages(const char *filename, char *outgoing) {
	FILE *fm = fopen(filename, "rb");
	char message[MSGSIZE];
	while(!feof(fm)) {
		fgets(message, MSGSIZE, fm);
		strcat(outgoing, message);
	}
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

void send_to_clients(int socket, struct sockaddr_in clients[], const char message[]) {
	int p = 0;
	printf(">>> SEND MESSAGE: %s", message);
	while(clients[p].sin_port) {
		printf("%d) TO %d\n", p, htons(clients[p].sin_port));
		sendto(socket, message, getsize(message), 0, (struct sockaddr *)&clients[p], sizeof(struct sockaddr));
		p++;
	}
}

void convert_to_string(void *msg, char request[], int size) {
	char *c;
	c = (char *)msg;
	// посимвольно записываем структуру в сообщение
	for (int i = 0; i < size; i++) {
		memset(request + i, *(c + i), 1);
	}
}
