// Copyright 2022 190010906
//
#include "RdtSocket.h"
#include <stdlib.h>
#include <stdio.h>
#include "sigio.h"
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "sigio.h"
#include "UdpSocket.h"

int G_port;
int G_state = RDT_STATE_LISTEN;
int G_flag;
RdtSocket_t* G_socket;

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
    recvRdtPacket(G_socket, packet);
    int input = RdtTypeTypeToRdtEvent(packet->header.type);
    fsm(input, G_socket);

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

  while(G_state != RDT_STATE_CLOSED   ) {
    (void) pause(); // Wait for signal
  }

  closeRdtSocket_t(G_socket);

  return 0;
}