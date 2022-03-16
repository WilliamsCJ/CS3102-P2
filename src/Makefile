# Makefile

CC		=clang
#CC	=gcc
CC-flags		=-Wall #-g

LIB-files = UdpSocket.o RdtSocket.o

PROGRAMS = RdtServer

.SUFFIXES:	.c .o

.c.o:;	$(CC) $(CC-flags) -c $<

C-files = UdpSocket.c RdtServer.c RdtSocket.c

O-files	= $(C-files:%.c=%.o)

all:	$(PROGRAMS)

UdpSocket.c:	UdpSocket.h

RdtSocket.c: RdtSocket.h

RdtServer:	RdtServer.o $(LIB-files)
	$(CC) -o $@ $+

.PHONY:	clean

clean:;	rm -rf *.o $(PROGRAMS) *~
