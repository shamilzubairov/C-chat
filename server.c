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

struct sockaddr_in clients[MAXCLIENTSSIZE];

struct ClientBuffer {
	char type[20]; // message, registration, command, leave ...
	char login[LOGSIZE];
	char message[MSGSIZE];
	char command[10]; // name, group, exit ...
} client_buffer;

struct ServerBuffer {
	char type[20]; // open, close
	char message[MSGSIZE];
	char zero[30]; // для конвертации строки к struct ServerBuffer
} server_buffer;

void Connect(struct Connection *);

void Close_connection();

void save_messages(const char *, const char *);

void load_messages(const char *, char *);

void add_new_client(const char *, struct sockaddr_in *, int);

void save_clients(const char *, struct sockaddr_in []);

void load_clients(const char *, struct sockaddr_in []);

void send_to_clients(int, struct sockaddr_in [], const char []);

void convert_to_string(void *, char [], int);

void memdump(const char [], const int);

int main() {
	sigaction_init(SIGALRM, handler.sig_alarm);
	sigaction_init(SIGINT, Close_connection);
	
	int current_clients_size = 0;
	char incoming[BUFSIZE];
	char outgoing[BUFSIZE];
	struct sockaddr_in from_address;
	socklen_t from_addrlen = sizeof(struct sockaddr_in);
	
	// Очищаем файлы
	FILE *fc = fopen(FILECLIENTS, "w");
	FILE *fm = fopen(FILEMESSAGES, "w");
	fclose(fc);
	fclose(fm);

	memset(server_buffer.zero, '\0', 30);

	Connect(&udp);

	// ...
	printf("Server is working by PORT - %d\n\n", udp.port);
	
	do {
		if(current_clients_size > MAXCLIENTSSIZE) {
			// Достигнуто макс. кол-во клиентов
			// Отослать уведомление
		}
		load_clients(FILECLIENTS, clients);

		bzero(incoming, BUFSIZE);
		bzero(outgoing, BUFSIZE);
		bzero(&from_address, from_addrlen);
		
		recvfrom(udp.socket, incoming, BUFSIZE, 0, (struct sockaddr *)&from_address, &from_addrlen);
		strcpy(client_buffer.type, ((struct ClientBuffer *)incoming)->type);
		strcpy(client_buffer.login, ((struct ClientBuffer *)incoming)->login);
		strcpy(client_buffer.message, ((struct ClientBuffer *)incoming)->message);
		strcpy(client_buffer.command, ((struct ClientBuffer *)incoming)->command);

		memdump(client_buffer.message, getsize(client_buffer.message));
		
		strcpy(server_buffer.type, "open");

		if(!strcmp(client_buffer.type, "message")) {
			multistrcat(server_buffer.message, client_buffer.login, ": ", client_buffer.message, "\0");
			convert_to_string(&server_buffer, outgoing, BUFSIZE);
			send_to_clients(udp.socket, clients, outgoing);
			save_messages(FILEMESSAGES, server_buffer.message);
		} else if(!strcmp(client_buffer.type, "register")) {
			printf("\n===REGISTRATION OF NEW CLIENT===\n\n");
			// Создаем параллельный дочерний процесс для нового участника
			if(fork() == 0) {
				// Регистрируем участника
				sigaction_init(SIGINT, handler.sig_stub);
				printf("Connection with %s (by port - %d)\n\n", client_buffer.login, htons(from_address.sin_port));
				sprintf(
					server_buffer.message, 
					"\nConnection established with port %d\n",
					htons(from_address.sin_port)
				);
				convert_to_string(&server_buffer, outgoing, BUFSIZE);
				sendto(udp.socket, outgoing, BUFSIZE, 0, (struct sockaddr *)&from_address, from_addrlen);
				
				// Отправляем файл с предыдущими сообщениями
				bzero(server_buffer.message, MSGSIZE);
				load_messages(FILEMESSAGES, server_buffer.message);
				convert_to_string(&server_buffer, outgoing, BUFSIZE);
				sendto(udp.socket, outgoing, BUFSIZE, 0, (struct sockaddr *)&from_address, from_addrlen);

				exit(0);
			} // ------ fork end ------ //
			multistrcat(server_buffer.message, "\t\t", client_buffer.login, " join this chat\n", "\0");
			convert_to_string(&server_buffer, outgoing, BUFSIZE);
			send_to_clients(udp.socket, clients, outgoing);
			add_new_client(FILECLIENTS, &from_address, from_addrlen);
			current_clients_size++;
		} else if(!strcmp(client_buffer.type, "close")) {
			int i = 0, j = 0;
			while(clients[i].sin_port) {
				if(clients[i].sin_port == from_address.sin_port) {
					j = i;
					while(clients[j + 1].sin_port) {
						memcpy(&clients[j], &(clients[j + 1]), sizeof(struct sockaddr));
						j++;
					}
					memset(&clients[j], '\0', sizeof(struct sockaddr));
					break;
				}
				i++;
			}
			save_clients(FILECLIENTS, clients);
		} else {
			printf("NO MATCH TYPE IN BUFFER\n");
		}

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
	// if(!inet_aton(conn->host, &(host_address.sin_addr))) {
	// 	handler.sys_error("Invalid address");
	// }
	if(bind(conn->socket, (struct sockaddr *)&(host_address), sizeof(host_address)) == -1) {
		handler.sys_error("Failed bind UDP connection");
	}
}

void Close_connection() {
	// Если сервер отключился, отправить сообщение всем клиентам
	// и инициировать у них отключение и повторное подключение
	char notify[BUFSIZE];
	strcpy(server_buffer.type, "close");
	strcpy(server_buffer.message, "\t\tConnection closed\n");
	convert_to_string(&server_buffer, notify, BUFSIZE);
	send_to_clients(udp.socket, clients, notify);
	handler.sig_int(SIGINT);
}

void save_messages(const char *filename, const char *messages) {
	// Сохранять только последние 10 сообщений
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

void add_new_client(const char *filename, struct sockaddr_in *client, int clientsize) {
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

void save_clients(const char *filename, struct sockaddr_in clients[]) {
	char *c;
	FILE *fc = fopen(filename, "wb");
	if(fc == NULL) {
		handler.sys_error("File");
	}
	c = (char *)clients;
    // посимвольно записываем в файл структуру
	while (*c) {
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
	printf("SEND MESSAGE TO:\n");
	while(clients[p].sin_port) {
		printf("%d) %d\n", p, htons(clients[p].sin_port));
		sendto(socket, message, BUFSIZE, 0, (struct sockaddr *)&clients[p], sizeof(struct sockaddr));
		p++;
	}
	printf("\n");
}

void convert_to_string(void *buffer, char request[], int size) {
	char *c;
	c = (char *)buffer;
	// посимвольно записываем структуру в сообщение
	for (int i = 0; i < size; i++) {
		memset(request + i, *(c + i), 1);
	}
}

// Выводит дамп памяти в формате HEX
void memdump(const char buffer[], const int size) {
	printf("RECIEVED %d BYTES:\n", size);
	unsigned char byte; 
	unsigned int i, j;
	for(i = 0; i < size; i++) {
		printf("%02x ", buffer[i]); // Отображаем
		
		if(((i % 16) == 15) || (i == size - 1)) {
			for(j = 0; j < 15 - (i % 16); j++) {
				printf("   ");
			}
			printf("| ");
			for(j = (i - (i % 16)); j <= i; j++) {
				byte = buffer[j];
				if((byte > 31) && (byte < 127)) {
					printf("%c", byte);
				} else {
					printf(".");
				}
			}
			printf("\n");
		} // ------ if end ------ //
	} // ------ for end ------ //
}