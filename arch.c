// SERVER

if(!strcmp(request, "HELLO")) {
			if(fork() == 0) {
				sigaction_init(SIGINT, handler.sig_stub);
				
				char response[5];
				int messagecount = 0;
				// Установка соединения
				// Значит пришел токен, отправляем обратно
				sendto(udp.socket, request, 6, 0, (struct sockaddr *)&from_address, from_addrlen);
				while(messagecount < 10) {
					recvfrom(udp.socket, response, 5, 0, (struct sockaddr *)&from_address, &from_addrlen);
					if(!strcmp(response, "DONE")) break;
					messagecount++;
				}
				if(messagecount == 10 || strcmp(response, "DONE")) {
					exit(0);
				} else {
					// Регистрируем участника
					// Ожидаем логин
					recvfrom(udp.socket, client.login[client.current], LOGSIZE, 0, (struct sockaddr *)&from_address, &from_addrlen);
					// Сохраняем данные в памяти для последующего сохранения в файл
					memcpy(&client.address[client.current], &from_address, from_addrlen);
					// Сохраняем данные в файл
					save_client(FILENAME, &client);
					
					printf("Connection with %s\n", client.login[client.current]);
					
					// Рассылаем приветствие
					char greeting[BUFSIZE];
					bzero(greeting, BUFSIZE);
					multistrcat(greeting, "\t\t", client.login[client.current], " join this chat\n", "\0");
					send_to_clients(udp.socket, from_address.sin_port, client, greeting);
				}
				exit(0);
			} // <--- fork
			wait(NULL);
		} else {
			// ЧАТ
			load_client(FILENAME, &client);
			send_to_clients(udp.socket, from_address.sin_port, client, request);
		}

// CLIENT

// Установка соединения
	int r_bytes = -1;
	int messagecount = 0;
	char response[BUFSIZE];
	while(messagecount < 10) {
	    bzero(response, BUFSIZE);
		write(udp.socket, "HELLO", 6);
		sleep(1);
		r_bytes = read(udp.socket, response, BUFSIZE);
		if(!strcmp("HELLO", response)) {
			if(write(udp.socket, "DONE", 5) == -1) {
				handler.sys_error("No synchronize with server");
			};
			break;
		}
		messagecount++;
	}

	if(messagecount == 10 && strcmp(response, "DONE")) {
		handler.sys_error("Server didn\'t send any response");
	} else {
		printf("\nConnection with %s:%d established by PID - %d\n", udp.host, udp.port, getpid());
		// Приветствуем
		// Отправляем логин
		printub("\nTo start chatting you must set a name to your friends recognize you\n");
		printub("-\n");
		printub("Print your chat-name here: ");
		
		bzero(user.login, sizeof(user.login));
		read(0, user.login, sizeof(user.login));
		remove_nl(user.login);
		write(udp.socket, user.login, LOGSIZE);
		
		printf("\n\n============ HELLO, %s ============\n\n", user.login);
	}

if(fork() == 0) {
		sigaction_init(SIGINT, handler.sig_stub);
		char incoming[BUFSIZE];
		int max_messages = 0;
		int timeout = 0;
		do {
			bzero(incoming, BUFSIZE);
			if(timeout > 120) {
				// Убить текущий дочерний процесс и создать новый
			};
			int r_bytes = read(udp.socket, incoming, sizeof(incoming));
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


udp.socket = socket(FAM, SOCK, 0);
if(udp.socket == -1) {
	handler.sys_error("Failed socket connection");
}
udp.address.sin_family = FAM;
udp.address.sin_port = htons(udp.port);
if(!inet_aton(udp.host, &udp.address.sin_addr)) {
	handler.sys_error("Invalid address");
}

int opt = 1;
setsockopt(udp.socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

if(bind(udp.socket, (struct sockaddr *)&udp.address, sizeof(udp.address)) == -1) {
	handler.sys_error("Failed bind UDP connection");
}


udp.socket = socket(FAM, SOCK, 0);
if(udp.socket == -1) {
	handler.sys_error("Failed socket connection");
}
udp.address.sin_family = FAM;
udp.address.sin_port = htons(udp.port);
if(!inet_aton(udp.host, &udp.address.sin_addr)) {
	handler.sys_error("Invalid address");
}

connect(udp.socket, (struct sockaddr*)&udp.address, sizeof(udp.address));

// fcntl(udp.socket, F_SETFL, fcntl(udp.socket, F_GETFL) | O_NONBLOCK);