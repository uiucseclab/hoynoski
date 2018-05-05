all: client server

client.o: client.c
	gcc -c client.c -o client.o -std=c99

client: client.o
	gcc client.o -o client.exe  -lws2_32

server.o: server.o
	gcc -c server.c -o server.o -std=c99

server: server.o
	gcc server.o -o server.exe -lws2_32

clean:
	del client.o 
	del server.o
	del client.exe
	del server.exe 