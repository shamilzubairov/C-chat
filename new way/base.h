#ifndef BASE_MODULE

#define BASE_MODULE

void printub(const char *);
int getsize(const char *);
void remove_nl(char *);
void multistrcat(char *, ...);

#define MAXCLIENTSSIZE 20
#define MSGSIZE 100
#define LOGSIZE 20
#define BUFSIZE (MSGSIZE + LOGSIZE + 30)
#define FCLIENTS "./participants"
#define FMESSAGES "./messages"

#endif