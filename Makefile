all:
	gcc -o server server.c -pthread -lpthread
	strip -s server
	mv -f server ./bin/

