all: clean client server
client: sender.c
	gcc -o myclient sender.c -I.
server: receiver.c
	gcc -o myserver receiver.c -I.
clean:
	rm -f myclient myserver *.o *.txt
