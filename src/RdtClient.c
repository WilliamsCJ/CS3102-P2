// Copyright 2022 190010906
//
#include "RdtSocket.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "sigio.h"
#include "sigalrm.h"

int G_port;
int G_state = RDT_STATE_CLOSED;
int G_flag;
RdtSocket_t* G_socket;
uint8_t* G_buf;
uint8_t G_buf_size;

void handleSIGIO(int sig) {
  if (sig == SIGIO) {
    /* protect the network and keyboard reads from signals */
    sigprocmask(SIG_BLOCK, &G_sigmask, (sigset_t *) 0);

    received = recvRdtPacket(G_socket);

    int input = RdtTypeTypeToRdtEvent(received->header.type);

    fsm(input, G_socket);

    free(received);

    /* allow the signals to be delivered */
    sigprocmask(SIG_UNBLOCK, &G_sigmask, (sigset_t *) 0);
  }
  else {
    perror("handleSIGIO(): got a bad signal number");
    exit(1);
  }
}

/*
 * Function: handleSIGALARM
 * ------------------------
 * Copied from timer.c:
 *
 * saleem, Jan2022
 * saleem, Jan2021
 * saleem, Jan2004
 * saleem, Nov2002
 */
void handleSIGALRM(int sig) {
  if (sig == SIGALRM) {
    /* protect handler actions from signals */
    sigprocmask(SIG_BLOCK, &G_sigmask, (sigset_t *) 0);

    /* Send a packet */
    fsm(RDT_EVENT_TIMEOUT_2MSL, G_socket);

    /* protect handler actions from signals */
    sigprocmask(SIG_UNBLOCK, &G_sigmask, (sigset_t *) 0);
  }
  else {
    perror("handleSIGALRM() got a bad signal");
    exit(1);
  }
}

void rdtOpen(RdtSocket_t* socket) {
  /* Setup SIGIO to handle network events. */
  setupSIGIO(socket->local->sd, handleSIGIO);
  setupSIGALRM(handleSIGALRM);

  fsm(RDT_INPUT_ACTIVE_OPEN, socket);

  while(G_state != RDT_STATE_ESTABLISHED) {
    (void) pause(); // Wait for signal
  }
}

void rdtSend(RdtSocket_t* socket, const void* buf, int n) {
  G_buf = (uint8_t*) buf;
  G_buf_size = n;
  fsm(RDT_INPUT_SEND, socket);

  // TODO: Check that the buffer has been completely sent
  while(G_seq_no != G_buf_size) {
    (void) pause(); // Wait for signal
  }

  // Close
}

void rdtClose(RdtSocket_t* socket) {
  fsm(RDT_INPUT_CLOSE, socket);
}

int main(int argc, char* argv[]) {
  G_port = getuid();

  G_socket = openRdtClient(argv[1], G_port);
  if (G_socket < 0 ) {
    exit(-1);
  }

  char data[] = "Pizzazz and shazam";

  rdtOpen(G_socket);
  rdtSend(G_socket, &data, sizeof(data));
  rdtClose(G_socket);

  closeRdtSocket_t(G_socket);

  return 0;
}