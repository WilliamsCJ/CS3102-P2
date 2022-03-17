// Copyright 2022 190010906
//
#include "RdtSocket.h"
#include "RDT.h"
#include <stdlib.h>
#include <stdio.h>
#include "sigio.h"
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "sigio.h"
#include "UdpSocket.h"

RdtSocket_t* G_socket;
UdpSocket_t receive;
int G_port;

RdtSocket_t* G_socket;

UdpSocket_t* G_local;
UdpSocket_t G_receive;

/*
 * Function: handleSIGIO
 * ---------------------
 * Handles SIGIO.
 *
 * Modified from handleSIGIO in udp-server-async.c by Saleem Bhatti.
 *
 * saleem, Jan2022
 * saleem, Jan2021
 * saleem, Jan2004
 * saleem, Nov2002
 */
void handleSIGIO(int sig) {
  if (sig == SIGIO) {
    /* protect the network and keyboard reads from signals */
    sigprocmask(SIG_BLOCK, &G_sigmask, (sigset_t *) 0);

    /* call the function passed */
    /* TODO: Move packet creation elsewhere? */
    RdtPacket_t* packet = (RdtPacket_t *) calloc(1, sizeof(RdtPacket_t));
    recvRdt(G_socket, packet);
    printf("Seq no. %d\n", packet->header.sequence);

    /* allow the signals to be delivered */
    sigprocmask(SIG_UNBLOCK, &G_sigmask, (sigset_t *) 0);
  }
  else {
    perror("handleSIGIO(): got a bad signal number");
    exit(1);
  }
}

int main(int argc, char* argv[]) {
  int error = 0;
  /* TODO: Args parsing */

  /* Set up port and sockets */
  G_port = getuid();
  G_socket = setupRdtSocket_t(argv[1], G_port);

  setupSIGIO(G_socket->local->sd, handleSIGIO);

  while(1) {
    (void) pause(); // Wait for signal
  }

  closeRdtSocket_t(G_socket);

  return 0;
}