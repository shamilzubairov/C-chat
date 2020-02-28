CC = gcc # по-умолчанию cc
CFLAGS = -g -Wall -O0
SRCMODS = base.c handlers.c
OBJMODS = $(SRCMODS:.c=.o)

# $@ - имя текущей цели
# $< - имя первой цели из списка зависимостей
# $^ - весь список зависимостей

%.o: %.c %.h
	$(CC)$(CFLAGS) -c $< -o $@

client: client.c $(OBJMODS)
	$(CC)$(CFLAGS) $^ -o $@

server: server.c $(OBJMODS)
	$(CC)$(CFLAGS) $^ -o $@
	mkdir db

compile: server client

runServ: server
	./server
	
clean:
	rm -f *.o server client
	rm -rf db