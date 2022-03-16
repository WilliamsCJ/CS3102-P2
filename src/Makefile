# Makefile

CC		=clang
#CC	=gcc
CC-flags		=-Wall -g

LIB-files = UdpSocket.o RdtSocket.o sigio.o

PROGRAMS = RdtServer RdtClient

.SUFFIXES:	.c .o

.c.o:;	$(CC) $(CC-flags) -c $<

C-files = UdpSocket.c RdtServer.c RdtClient.c RdtSocket.c sigio.c

O-files	= $(C-files:%.c=%.o)

all:	$(PROGRAMS)

UdpSocket.c:	UdpSocket.h

RdtSocket.c: RdtSocket.h

sigio.c: sigio.h

RdtServer:	RdtServer.o $(LIB-files)
	$(CC) -o $@ $+

RdtClient:	RdtClient.o $(LIB-files)
	$(CC) -o $@ $+

.PHONY:	clean

clean:;	rm -rf *.o $(PROGRAMS) *~
