lab1: appserver.c
	gcc -c Bank.c
	gcc -c appserver.c
	gcc -c appserver-coarse.c
	gcc -o appserver-coarse appserver-coarse.o Bank.o -lpthread
	gcc -o appserver appserver.o Bank.o -lpthread
