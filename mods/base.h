#ifndef BASE_MODULE

#define BASE_MODULE

void printub(const char *);
int getsize(const char *);
void remove_nl(char *);
void multistrcat(char *, ...);

#define MAXCLIENTSSIZE 20
#define MSGSIZE 500
#define LOGSIZE 20
#define BUFSIZE (MSGSIZE + LOGSIZE + 30)
#define FILECLIENTS "./db/participants"
#define FILEMESSAGES "./db/messages"

struct ServerBuffer {
	char type[20]; // open, close
	char message[MSGSIZE];
	char zero[30]; // для конвертации строки к struct ServerBuffer
};

struct ClientBuffer {
	char type[20]; // message, registration, command, leave ...
	char login[LOGSIZE];
	char message[MSGSIZE];
	char command[10]; // name, group, exit ...
};

struct Connection {
	short socket;
	int port;
};

#endif