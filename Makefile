all:
	gcc client.c -lpthread -o client
	gcc server.c -lpthread -o server
clean:
	rm -rf *~ client.o server.o
