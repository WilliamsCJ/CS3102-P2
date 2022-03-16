// Copyright 2022 190010906
//

#include "RdtSocket.h"
#include "UdpSocket.h"
#include <stdio.h>
#include <unistd.h>

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
int setupRdtSocket_t(const char* hostname, const uint16_t port, RdtSocket_t* socket) {
  /* setup local UDP socket */
  socket->local = setupUdpSocket_t((char *) 0, port);
  if (socket->local == (UdpSocket_t *) 0) {
    perror("Couldn't setup local UDP socket");
    return(-1);
  }

  /* setup remote UDP socket */
  socket->remote = setupUdpSocket_t(hostname, port);
  if (socket->remote == (UdpSocket_t *) 0) {
    perror("Couldn't setup remote UDP socket");
    return(-1);
  }

  /* open local UDP socket */
  if (openUdp(socket->local) < 0) {
    perror("Couldn't open UDP socket");
    return(-1);
  }

  return configure_timeout(socket);
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
}