#include "inc.h"
#include "handlers.h"

void sig_alarm(int sig) {
	printf("Alarm::socket didn\'t get any response\n");
	exit(1);
}

void sig_int(int sig) {
	exit(0);
}

void sig_stub(int sig) {
	exit(0);
}

int sys_error(char *msg) {
	perror(msg);
	exit(1);
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
