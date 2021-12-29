CC=gcc
CFLAGS=-Wall -O2

.PHONY: all doc clean

all: echoclient echoserveri echoserverp echoservert

echoserveri: echoserveri.c net.c net.h common.c common.h
	$(CC) $(CFLAGS) -o $@ $^

echoserverp: echoserverp.c net.c net.h common.c common.h
	$(CC) $(CFLAGS) -o $@ $^

echoservert: echoservert.c net.c net.h common.c common.h
	$(CC) $(CFLAGS) -o $@ $^ -lpthread

echoclient: echoclient.c net.c net.h common.c common.h
	$(CC) $(CFLAGS) -o $@ $^

doc:
	doxygen doc/Doxyfile

clean:
	rm -rf echoclient echoserver{i,p,t} *.o 

mrproper: clean
	rm -rf doc/html
