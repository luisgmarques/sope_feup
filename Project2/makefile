all: user.c server.c
	 gcc -Wall user.c log.c -o user -D_REENTRANT -lpthread -lrt
	 gcc -Wall server.c log.c -o server -D_REENTRANT -lpthread -lrt

clean:
	rm -f user
	rm -f server
	
