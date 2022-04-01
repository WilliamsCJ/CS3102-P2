// Copyright 2022 190010906
//
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

#include "d_print/d_print.h"
#include "sigio/sigio.h"
#include "sigalrm/sigalrm.h"
#include "RdtSocket.h"

int G_port;
int G_state = RDT_STATE_LISTEN;
int G_flag;
RdtSocket_t* G_socket;
uint8_t* G_buf;
uint8_t G_buf_size;

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
    received = recvRdtPacket(G_socket);

    int input = rdtTypeToRdtEvent(received->header.type);


    if (input == RDT_EVENT_RCV_DATA) {
      if (G_buf == NULL) {
        G_buf_size = received->header.size;
        G_buf = (uint8_t*) calloc(1, G_buf_size);
        memcpy(G_buf, &(received->data), G_buf_size);
      } else {
        G_buf = (uint8_t*) realloc(G_buf, G_buf_size + received->header.size);
        memcpy(G_buf+G_buf_size, &(received->data), received->header.size);
        G_buf_size = G_buf_size + received->header.size;
      }
    }

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

int main(int argc, char* argv[]) {
  int error = 0;
  /* TODO: Args parsing */

  /* Set up port and sockets */
  G_port = getuid();
  G_socket = setupRdtSocket_t(argv[1], G_port);

  setupSIGIO(G_socket->local->sd, handleSIGIO);
  setupSIGALRM(handleSIGALRM);

  while(G_state != RDT_STATE_CLOSED) {
    (void) pause(); // Wait for signal
  }

  /* Write your buffer to disk. */
  FILE* pFile = fopen("dog.jpg","wb");

  if (pFile){
    fwrite(G_buf, G_buf_size, 1, pFile);
    puts("Wrote to file!");
  }
  else{
    puts("Something wrong writing to File.");
  }

  fclose(pFile);

  closeRdtSocket_t(G_socket);

  return 0;
}