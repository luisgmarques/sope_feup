CC=gcc
CFLAGS=-I.

server: server.c log.c
	$(CC) -Wall -o server server.c log.c -lpthread -lrt

user: user.c log.c
	$(CC) -Wall -o user user.c log.c -lpthread -lrt
