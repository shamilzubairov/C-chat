#ifndef HANDLER_MODULE

#define HANDLER_MODULE

void sig_alarm(int);
void sig_int(int);
void sig_stub(int);
int sys_error(char *);
void sigaction_init(int, void ());

#endif
