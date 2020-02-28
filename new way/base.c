#include "inc.h"
#include "base.h"

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