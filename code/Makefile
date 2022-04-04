# Makefile

CC		=clang
#CC	=gcc
CC-flags		=-Wall -g

LIB = checksum.o sigio.o sigalrm.o UdpSocket.o d_print.o rdt.o rto.o
PROGRAMS = RdtServer RdtClient RdtServerRTT RDTClientRTT

.PHONY: clean

RdtServerRTT: RdtServerRTT.o $(LIB)
	$(CC) -o $@ $+

RdtClientRTT: RdtClientRTT.o $(LIB)
	$(CC) -o $@ $+

RdtServer: RdtServer.o $(LIB)
	$(CC) -o $@ $+

RdtClient: RdtClient.o $(LIB)
	$(CC) -o $@ $+

RdtClientRTT.o: RdtClient.c
	$(CC) -c ./RdtClientRTT.c

RdtServerRTT.o: RdtServer.c
	$(CC) -c ./RdtServerRTT.c

RdtClient.o: RdtClient.c
	$(CC) -c ./RdtClient.c

RdtServer.o: RdtServer.c
	$(CC) -c ./RdtServer.c

rdt.o: rdt.c rdt.h
	$(CC) -c ./rdt.c

rto.o: ./rto/rto.c ./rto/rto.h
	$(CC) -c ./rto/rto.c

checksum.o: ./checksum/checksum.c ./checksum/checksum.h d_print.o
	$(CC) -c ./checksum/checksum.c

sigio.o: ./sigio/sigio.c ./sigio/sigio.h
	$(CC) -c ./sigio/sigio.c

sigalrm.o: ./sigalrm/sigalrm.c ./sigalrm/sigalrm.h
	$(CC) -c ./sigalrm/sigalrm.c

UdpSocket.o: ./UdpSocket/UdpSocket.c ./UdpSocket/UdpSocket.h
	$(CC) -c ./UdpSocket/UdpSocket.c

d_print.o: ./d_print/d_print.c ./d_print/d_print.h
	$(CC) -c ./d_print/d_print.c

clean:
    clean:;	rm -rf *.o $(PROGRAMS) *~