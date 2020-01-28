#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netinet/in.h> // sockaddr_in
#include <arpa/inet.h> // htons

#define TOTALCLIENTS 10
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

struct Connection {
	char *host;
	int port;
	int socket; // сокет
	struct sockaddr_in addr;
} conn = {
	"127.0.0.1",
	7654
};

struct Clients {
	struct sockaddr_in addr[TOTALCLIENTS];
	char login[TOTALCLIENTS];
} clnt;

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

// -------------------------
// -------------------------
// -------------------------
// -------------------------
// -------------------------
// -------------------------

int add_client(struct Clients, struct sockaddr_in);

void send_to_clients(struct Connection, struct Clients, struct sockaddr_in, socklen_t);

int main() {
	sigaction_init(SIGALRM, hdl.sig_alarm);

	conn.addr.sin_family = FAM;
	conn.addr.sin_port = htons(conn.port);
	if(!inet_aton(conn.host, &conn.addr.sin_addr)) {
		hdl.sys_error("Invalid address");
	}
	conn.socket = socket(FAM, SOCK, 0);
	if(conn.socket == -1) {
		hdl.sys_error("Socket connection");
	}
	int bnd = bind(conn.socket, (struct sockaddr*)&conn.addr, sizeof(conn.addr));
	if(bnd == -1) {
		hdl.sys_error("Bind connection");
	} else {
		printub("Server is working...\n----------------------\n\n");
	}

	char incoming[MESSAGESSIZE];
	do {
		bzero(incoming, MESSAGESSIZE);
		struct sockaddr_in from_addr;
		socklen_t addrlen = sizeof(from_addr);
		alarm(120);
		recvfrom(conn.socket, incoming, sizeof(incoming), 0, (struct sockaddr*)&from_addr, &addrlen);
#ifdef DEBUG 
	// [-DDEBUG=1 cmd]
	printf("%s\n", incoming);
#endif
		if(!strncmp(incoming, TOKEN, 7)) { 
			// Уязвимость -> LOGIN:: может быть введено пользователем вручную
			// Как вариант можно отправлять хешируемое значение для логирования
			static int count = 0;
			if((count = add_client(clnt, from_addr)) <= TOTALCLIENTS) {
				// Клиента добавляем в массив
				clnt.addr[count] = from_addr;
				// Отправляем контрольный бит
				sendto(conn.socket, "1", 1, 0, (struct sockaddr*)&from_addr, addrlen);
				count++;
			}
			continue;
		}
		char * istr;
		if((istr = strstr(incoming, ": ::name"))) {
			int ind = (int)(istr - incoming);
			char log[ind];
			strncpy(log, incoming, ind);
			printf("%s\n", log);
			continue;
		}
		if(strstr(incoming, ": ::exit")) {
			printf("%s\n", incoming);
			continue;
		}

		send_to_clients(conn, clnt, from_addr, addrlen);
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
	// multistrcat(notify, "\t\t", user.login, " leave this chat\n", "\0");
	// send_message_to(conn, notify);
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

int add_client(struct Clients clnt, struct sockaddr_in from_addr) {
	int k = 0;
	for(;k <= 10; k++) {
		if(clnt.addr[k].sin_port == from_addr.sin_port) {
			return 11;
		} else if (!clnt.addr[k].sin_port) {
			return k;
		}
	}
	return 11; // Массив полон, клиентов добавлять нельзя
}

void send_to_clients(struct Connection conn, struct Clients clnt, struct sockaddr_in from_addr, socklen_t addrlen) {
	int j = 0;
	int total_message_size = 30;
	for(;j <= TOTALCLIENTS && (clnt.addr[j].sin_port); j++) {
		if(clnt.addr[j].sin_port == from_addr.sin_port) continue;
		char message[total_message_size];
		bzero(message, total_message_size);
		// strcat(message, income);
		if(sendto(conn.socket, message, sizeof(message), 0, (struct sockaddr*)&clnt.addr[j], addrlen) == -1) {
			perror("not sent");
		}
	}
}

