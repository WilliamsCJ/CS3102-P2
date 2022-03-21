# Makefile

CC		=clang
#CC	=gcc
CC-flags		=-Wall -g

LIB-files = UdpSocket.o RdtSocket.o sigio.o sigalrm.o

PROGRAMS = RdtServer RdtClient

.SUFFIXES:	.c .o

.c.o:;	$(CC) $(CC-flags) -c $<

C-files = UdpSocket.c RdtServer.c RdtClient.c RdtSocket.c sigio.c sigalrm.c

O-files	= $(C-files:%.c=%.o)

all:	$(PROGRAMS)

UdpSocket.c:	UdpSocket.h

RdtSocket.c: RdtSocket.h

sigio.c: sigio.h

sigalrm.c: sigalrm.h

RdtServer:	RdtServer.o $(LIB-files)
	$(CC) -o $@ $+

RdtClient:	RdtClient.o $(LIB-files)
	$(CC) -o $@ $+

.PHONY:	clean

clean:;	rm -rf *.o $(PROGRAMS) *~
