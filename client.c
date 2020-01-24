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

#define handle_error(msg) \
do { \
	perror(msg); \
	return(1); \
} while(0)

#define HOST "127.0.0.1"
#define PORT 7654

enum Family { FAM = AF_INET, SOCK = SOCK_DGRAM };

char login[20];
char message[100];
char income[100];

struct sockaddr_in s_addr;

void printraw(const char *);

void sig_handler(int sig) {
	printraw("Alarm::socket didn\'t get any response\n");
	exit(1);
}

int main() {
	signal(SIGALRM, sig_handler);

	printf("\n*****************************************************\n");
	printf("*********** Welcome to Cool Little Chatt! ***********\n");
	printf("*****************************************************\n");
	printf("*****************************************************\n");
	printf("\nTo start chatting you must set a name to your friends recognize you\n");
	printf("-\n");
	printraw("Print your chat-name here: ");
	bzero(login, sizeof(login));
	read(0, login, sizeof(login));
	
	int l = 0;
	while(login[l]) l++;
	login[--l] = '\0'; // Убираем перенос строки на конце
	
	printf("Wait for connection...\n");

	s_addr.sin_family = FAM;
	s_addr.sin_port = htons(PORT);
	if(!inet_aton(HOST, &s_addr.sin_addr)) {
		handle_error("Invalid address");
	}

	int sd = socket(FAM, SOCK, 0);
	if(sd == -1) {
		handle_error("Socket");
	} else {
		fcntl(sd, F_SETFL, fcntl(sd, F_GETFL) | O_NONBLOCK);
		
		char test[27] = "LOGIN::";
		strcat(test, login);
		
		int timer = 0;
		while(timer < 10 && !income[0]) {
			// Тестовый запрос для регистрации и отправка логина
			// 10 раз отправляем тестовое сообщение
			sendto(sd, test, sizeof(test), 0, (struct sockaddr*)&s_addr, sizeof(s_addr));
			// ...и ждем ответа
			recv(sd, income, sizeof(income), 0);
			timer++;
			sleep(1);
		}
		// Если ответ не пришел через 10 сек. закрываем соединение
		if(timer == 10 && !income[0]) {
			handle_error("Socket connection");
		}

		// Иначе установлено
		printf("\nConnection with %s:%d established\n", HOST, PORT);
		printf("You start chatting by name - %s\n\n", login);
		printf("============CHAT RIGTH NOW============\n\n");
	}

	int i = 0;
	do {
		alarm(60);
		// Принимаем сообщения
		if(fork() == 0) {
			int r_bytes = 0;
			int max_messages = 0;
			do {
				if(max_messages > 100) break;
				r_bytes = recvfrom(sd, income, sizeof(income), 0, NULL, 0);
				if(r_bytes == -1) {
					sleep(1);
					continue;
				} else {
					printf("%s", income);
				}
				max_messages++;
			} while(1);

			exit(0);
		}
		// Отправляем сообщения
		bzero(message, 100);
		read(0, message, sizeof(message));
		
		if(!strncmp(message, "name::", 6)) {
			// Сообщение направлено конкретному адресату
		}

		char full_message[sizeof(login) + sizeof(message) + 2];
		bzero(full_message, sizeof(login) + sizeof(message) + 2);
		strcat(full_message, login);
		strcat(full_message, ": ");
		strcat(full_message, message);

		int s = sendto(sd, full_message, sizeof(full_message), 0, (struct sockaddr*)&s_addr, sizeof(s_addr));
		if(s == -1) {
			handle_error("Send");
		}

		i++;
	} while(i < 100);

	close(sd);
	return 0;
}

void printraw(const char *message) {
	int size = 0;
	while(message[size]) size++;
	write(1, message, size);
}