CC = gcc

all:
	$(CC) -o md5 -O3 -lm md5.c
	$(CC) -o md5_sse -msse -mmmx -msse2 -O3 md5_sse.c

clean:
	rm md5 md5_sse

