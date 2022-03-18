// Copyright 2022 190010906
//
#include "RdtSocket.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "sigio.h"

int G_port;
int G_state = RDT_STATE_CLOSED;
int G_flag;
RdtSocket_t* G_socket;

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

void rdtSend(RdtSocket_t* socket) {
  int seq_no = 0;

  /* Setup SIGIO to handle network events. */
  setupSIGIO(socket->local->sd, handleSIGIO);

  // TODO: Sync
  fsm(RDT_INPUT_ACTIVE_OPEN, socket);

  printf("%d\n", G_state);

  while(G_state != RDT_STATE_CLOSED) {
    (void) pause(); // Wait for signal
  }

  // Close
}

int main(int argc, char* argv[]) {
  G_port = getuid();

  G_socket = openRdtClient(argv[1], G_port);
  if (G_socket < 0 ) {
    exit(-1);
  }

  rdtSend(G_socket);

  closeRdtSocket_t(G_socket);

  return 0;
}