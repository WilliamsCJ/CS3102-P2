
//
// Created by CJ Williams on 20/02/2022.
//
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "sigio.h"
#include "UdpSocket.h"

void perror(const char *s); // TODO: Do we need this?

/* signal action / handler hook */
struct sigaction G_sigio;
sigset_t G_sigmask;


/*
 * Function: setAsyncFd
 * --------------------
 * Copied from setAsyncFd in udp-server-async.c by Saleem Bhatti.
 *
 * saleem, Jan2022
 * saleem, Jan2021
 * saleem, Jan2004
 * saleem, Nov2002
 */
int setAsyncFd(int fd) {
  int r, flags = O_NONBLOCK | O_ASYNC; // man 2 fcntl

  if (fcntl(fd, F_SETOWN, getpid()) < 0) {
    perror("setAsyncFd(): fcntl(fd, F_SETOWN, getpid()");
    exit(0);
  }

  if ((r = fcntl(fd, F_SETFL, flags)) < 0) {
    perror("setAsyncFd(): fcntl() problem");
    exit(0);
  }

  return r;
}


/*
 * Function: handleSIGIO
 * ---------------------
 * Sets up SIGIO.
 *
 * Modified from setupSIGIO in udp-server-async.c by Saleem Bhatti.
 *
 * saleem, Jan2022
 * saleem, Jan2021
 * saleem, Jan2004
 * saleem, Nov2002
 */
void setupSIGIO(int net, void(*handler)(int)) {
  sigaddset(&G_sigmask, SIGIO);

  G_sigio.sa_handler = handler;
  G_sigio.sa_flags = 0;

  if (sigaction(SIGIO, &G_sigio, (struct sigaction *) 0) < 0) {
    perror("setupSIGIO(): sigaction() problem");
    exit(1);
  }

  if (setAsyncFd(net) < 0) {
    perror("setupSIGIO(): setAsyncFd(G_net) problem");
    exit(1);
  }

}