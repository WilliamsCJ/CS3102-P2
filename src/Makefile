# Makefile

CC		=clang
#CC	=gcc
CC-flags		=-Wall #-g

LIB-files = UdpSocket.o

PROGRAMS	=

.SUFFIXES:	.c .o

.c.o:;	$(CC) $(CC-flags) -c $<

C-files = UdpSocket.c

O-files	= $(C-files:%.c=%.o)

all:	$(PROGRAMS)

UdpSocket.c:	UdpSocket.h

.PHONY:	clean

clean:;	rm -rf *.o $(PROGRAMS) *~
