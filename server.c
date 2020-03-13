#include "mods/inc.h"
#include "mods/base.h"
#include "mods/handlers.h"

struct SystemHandlers handler = {
	&sig_alarm,
	&sig_int,
	&sig_stub,
	&sys_error,
};

enum Family { FAM = AF_INET, SOCK = SOCK_DGRAM };

struct Connection udp = { -1, 7654 };

struct sockaddr_in clients[MAXCLIENTSSIZE];

struct Clients {
	struct sockaddr_in addr;
	char login[LOGSIZE];
} clnt;

struct Clients all_clients[MAXCLIENTSSIZE];

struct ClientBuffer client_buffer;

struct ServerBuffer server_buffer;

void open_connection(struct Connection *);

void close_connection();

void save_message(const char *, const char *);

void load_messages(const char *, char *);

void add_client(const char *, struct sockaddr_in *);

void load_clients(const char *, struct sockaddr_in []);

void send_to_clients(int, struct sockaddr_in [], const char []);

void convert_to_string(void *, char []);

void memdump(const char [], const int);

int main() {
	sigaction_init(SIGALRM, handler.sig_alarm);
	sigaction_init(SIGINT, close_connection);
	
	int current_clients_size = 0;
	char incoming[BUFSIZE];
	char outgoing[BUFSIZE];
	struct sockaddr_in from_address;
	socklen_t from_addrlen = sizeof(struct sockaddr_in);

	memset(server_buffer.zero, '\0', 30);

	open_connection(&udp);

	strcpy(server_buffer.type, "open");

	// ...
	printf("Server is working by PORT - %d\n\n", udp.port);
	
	do {
		if(current_clients_size > MAXCLIENTSSIZE) {
			// Достигнуто макс. кол-во клиентов
			// Отослать уведомление
		}

		bzero(incoming, BUFSIZE);
		bzero(outgoing, BUFSIZE);
		bzero(&from_address, from_addrlen);
		
		recvfrom(udp.socket, incoming, BUFSIZE, 0, (struct sockaddr *)&from_address, &from_addrlen);
		strcpy(client_buffer.type, ((struct ClientBuffer *)incoming)->type);
		strcpy(client_buffer.login, ((struct ClientBuffer *)incoming)->login);
		strcpy(client_buffer.message, ((struct ClientBuffer *)incoming)->message);

		memdump(client_buffer.message, getsize(client_buffer.message));

		if(!strcmp(client_buffer.type, "message")) {
			sprintf(server_buffer.message, "%s: %s", client_buffer.login, client_buffer.message);
			convert_to_string(&server_buffer, outgoing);
			send_to_clients(udp.socket, clients, outgoing);
			save_message(FILEMESSAGES, server_buffer.message);
		} else if(!strcmp(client_buffer.type, "register")) {
			printf("\n===REGISTRATION OF NEW CLIENT===\n\n");
			// Создаем параллельный дочерний процесс для нового участника
			if(fork() == 0) {
				sigaction_init(SIGINT, handler.sig_stub);
				// Регистрируем участника
				printf("Connection with %s (by port - %d)\n\n", client_buffer.login, htons(from_address.sin_port));
				
				sprintf(server_buffer.message, "\nConnection established with port %d\n", htons(from_address.sin_port));
				convert_to_string(&server_buffer, outgoing);
				sendto(udp.socket, outgoing, BUFSIZE, 0, (struct sockaddr *)&from_address, from_addrlen);
				
				// Отправляем файл с предыдущими сообщениями
				bzero(server_buffer.message, MSGSIZE);

				load_messages(FILEMESSAGES, server_buffer.message);
				convert_to_string(&server_buffer, outgoing);
				sendto(udp.socket, outgoing, BUFSIZE, 0, (struct sockaddr *)&from_address, from_addrlen);

				exit(0);
			} // ------ fork end ------ //
			sprintf(server_buffer.message, "\t\t%s join this chat\n", client_buffer.login);
			convert_to_string(&server_buffer, outgoing);
			send_to_clients(udp.socket, clients, outgoing);

			add_client(FILECLIENTS, &from_address);
			load_clients(FILECLIENTS, clients);
			current_clients_size++;
		} else if(!strcmp(client_buffer.type, "close")) {
			bzero(server_buffer.message, MSGSIZE);
			sprintf(server_buffer.message, "\t\t%s leave this chat\n", client_buffer.login);
			convert_to_string(&server_buffer, outgoing);
			send_to_clients(udp.socket, clients, outgoing);
			
			int i = 0;
			int desc = open(FILECLIENTS, O_TRUNC | O_RDONLY);
			close(desc);
			while(clients[i].sin_port) {
				if(clients[i].sin_port == from_address.sin_port) {
					printf("Removing port %d\n", htons(from_address.sin_port));
				} else {
					add_client(FILECLIENTS, &(clients[i]));
				}
				i++;
			}
			load_clients(FILECLIENTS, clients);
		} else if(!strcmp(client_buffer.type, "command")) {
			strcpy(client_buffer.command, ((struct ClientBuffer *)incoming)->command);
			strcpy(client_buffer.to_login, ((struct ClientBuffer *)incoming)->to_login);
			remove_nl(client_buffer.to_login);
			sprintf(server_buffer.message, "%s (ONLY FOR %s): %s", client_buffer.login, client_buffer.to_login, client_buffer.message);
			convert_to_string(&server_buffer, outgoing);
			sendto(udp.socket, outgoing, BUFSIZE, 0, (struct sockaddr *)&from_address, from_addrlen);
		} else {
			printf("NO MATCH TYPE IN BUFFER\n");
		}

	} while(1);

	shutdown(udp.socket, SHUT_RDWR);

    return 0;
}

void open_connection(struct Connection *conn) {
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

void close_connection() {
	// Если сервер отключился, отправить сообщение всем клиентам
	// и инициировать у них отключение и повторное подключение
	char notify[BUFSIZE];
	strcpy(server_buffer.type, "close");
	strcpy(server_buffer.message, "\t\tConnection closed\n");
	convert_to_string(&server_buffer, notify);
	send_to_clients(udp.socket, clients, notify);

	int desc;
  	desc = open(FILECLIENTS, O_TRUNC | O_RDONLY);
	close(desc);
  	desc = open(FILEMESSAGES, O_TRUNC | O_RDONLY);
	close(desc);

	shutdown(udp.socket, SHUT_RDWR);

	handler.sig_int(SIGINT);
}

void save_message(const char *filename, const char *messages) {
	static int lines = 1;
	// Сохранять только последние 10 сообщений
	FILE *fm = fopen(filename, "a");
	flock(fileno(fm), LOCK_SH);
	if(fm == NULL) {
		handler.sys_error("File");
	}
	if(lines > 10) {
		// Удалять первое сообщение
		fseek(fm, 0, SEEK_SET);
		fseek(fm, 0, SEEK_END);
	}
	fputs(messages, fm);
	lines++;
	flock(fileno(fm), LOCK_UN);
    fclose(fm);
}

void load_messages(const char *filename, char *outgoing) {
	FILE *fm = fopen(filename, "r");
	if(fm == NULL) {
		handler.sys_error("File");
	}
	char message[MSGSIZE];
	while(!feof(fm) && fgets(message, MSGSIZE, fm)) {
		strcat(outgoing, message);
	}
}

void add_client(const char *filename, struct sockaddr_in *client) {
	char *c;
	FILE *fc = fopen(filename, "ab");
	flock(fileno(fc), LOCK_SH);
	if(fc == NULL) {
		handler.sys_error("File");
	}
	c = (char *)client;
    // посимвольно записываем в файл структуру
    for (int i = 0; i < sizeof(struct sockaddr_in); i++) {
        putc(*c++, fc);
    }
	flock(fileno(fc), LOCK_UN);
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

void convert_to_string(void *struct_buffer, char string[]) {
	char *c;
	c = (char *)struct_buffer;
	// посимвольно записываем структуру в сообщение
	for (int i = 0; i < BUFSIZE; i++) {
		memset(string++, *(c++), 1);
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