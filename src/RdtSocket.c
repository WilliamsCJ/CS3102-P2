// Copyright 2022 190010906
//
#include "RDT.h"
#include "RdtSocket.h"
#include "UdpSocket.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/*
 * Function: configure_timeout
 * ---------------------------
 * Configures the timeout for the local UDP socket.
 * Enables the program to handle dropped packets.
 *
 * returns: void
 */
int configure_timeout(RdtSocket_t* socket) {
  // Modified from: https://stackoverflow.com/questions/13547721/udp-socket-set-timeout
  // CITATION START
  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  if (setsockopt(socket->local->sd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
    perror("Couldn't set socket options");
    return(-1);
  }
  // CITATION END
  return 0;
}


/*
 * Function: setup_sockets
 * -----------------------
 *
 * Sets up and opens UDP sockets.
 *
 * hostname (char*): Name of host to open socket to.
 *
 * returns: void
 *
 */
RdtSocket_t* setupRdtSocket_t(const char* hostname, const uint16_t port) {
  RdtSocket_t* socket = (RdtSocket_t*) calloc(1, sizeof(RdtSocket_t));
  int error = 0;

  /* setup local UDP socket */
  socket->local = setupUdpSocket_t((char *) 0, port);
  if (socket->local == (UdpSocket_t *) 0) {
    perror("Couldn't setup local UDP socket");
    error = 1;
  }

  /* setup remote UDP socket */
  socket->remote = setupUdpSocket_t(hostname, port);
  if (socket->remote == (UdpSocket_t *) 0) {
    perror("Couldn't setup remote UDP socket");
    error = 1;
  }

  /* open local UDP socket */
  if (openUdp(socket->local) < 0) {
    perror("Couldn't open UDP socket");
    error = 1;
  }

  configure_timeout(socket);

  if (error) {
    free(socket);
    socket = (RdtSocket_t *) 0;
  }

  return socket;
}


/*
 * Function: cleanup_sockets
 * ---------------
 * Cleans up and closes the UDP sockets
 *
 * returns: void
 */
void closeRdtSocket_t(RdtSocket_t* socket) {
  closeUdp(socket->local);
  closeUdp(socket->remote);
  free(socket);
}

/*void recvRdt(const RdtSocket_t* socket, RdtPacket_t* packet) {
  recvUdp(socket->local, &(socket->receive), &udpBuffer);
  memcpy(packet, udpBuffer.bytes, sizeof(RdtPacket_t));
  *//* TODO: add return value*//*
}*/

void recvRdtAlt(UdpSocket_t* local, RdtPacket_t* packet) {
  UdpBuffer_t buffer;
  int r, error = 0;
  UdpSocket_t receive;

  r = recvUdp(local, &receive, &buffer);
  if (r < 0) {
    perror("Couldn't receive RDT packet");
    error = 1;
    return;
  }

  printf("Received %d / %d\n", buffer.n, r);
  memcpy(packet, buffer.bytes, buffer.n);
  /* TODO: add return value*/
}

void sendRdt(const RdtSocket_t* socket, const uint8_t n) {
  RdtPacket_t* packet = (RdtPacket_t *) calloc(1, sizeof(RdtSocket_t));
  UdpBuffer_t buffer;
  int r;

  packet->header.sequence = 11;
  packet->header.f_fin = 1;

  uint8_t bytes[sizeof(RdtSocket_t)];
  memcpy(bytes, packet, sizeof(RdtSocket_t));
  buffer.n = sizeof(RdtSocket_t);
  buffer.bytes = bytes;

  r = sendUdp(socket->local, socket->remote, &buffer);
  printf("Bytes sent %d\n", r);
  /* TODO: Check bytes sent. */
  free(packet);
}