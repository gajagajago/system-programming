CC=gcc
CFLAGS=-std=c99 -g

TARGET=fibonacci

$(TARGET): main.c fib.c fib.h
	$(CC) $(CFLAGS) -o $(TARGET) main.c fib.c

clean:
	rm -f $(TARGET) 
