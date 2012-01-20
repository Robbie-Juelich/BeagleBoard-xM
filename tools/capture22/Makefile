#  V4L2 Application makefile
#

CC	:= gcc
capture22: capture22.o
	$(CC) -o capture22 capture22.o


capture22.o: capture22.c
	$(CC) -c -g capture22.c


clean:
	rm -f capture22 *.o

