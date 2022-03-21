//
// Created by CJ Williams on 20/02/2022.
//
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

#include "sigalrm.h"
#include "udp_probe.h"

/* signal action / handler hook */
struct sigaction G_sigalrm;

/* timer value */
struct itimerval G_timer;


/*
 * Function: setITIMER
 * --------------------
 * Copied from timer.c:
 *
 * saleem, Jan2022
 * saleem, Jan2021
 * saleem, Jan2004
 * saleem, Nov2002
 */
void setITIMER(unsigned int sec, unsigned int usec) {
  G_timer.it_interval.tv_sec = sec;
  G_timer.it_interval.tv_usec = usec;
  G_timer.it_value.tv_sec = sec;
  G_timer.it_value.tv_usec = usec;
  if (setitimer(ITIMER_REAL, &G_timer, (struct itimerval *) 0) != 0) {
    perror("setITIMER(): setitimer() problem");
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
    send_probe();

    /* protect handler actions from signals */
    sigprocmask(SIG_UNBLOCK, &G_sigmask, (sigset_t *) 0);
  }
  else {
    perror("handleSIGALRM() got a bad signal");
    exit(1);
  }
}


/*
 * Function: setupSIGALARM
 * ------------------------
 * Copied from timer.c:
 *
 * saleem, Jan2022
 * saleem, Jan2021
 * saleem, Jan2004
 * saleem, Nov2002
 */
void setupSIGALRM() {
  sigaddset(&G_sigmask, SIGALRM);

  G_sigalrm.sa_handler = handleSIGALRM;
  G_sigalrm.sa_flags = 0;

  if (sigaction(SIGALRM, &G_sigalrm, (struct sigaction *) 0) < 0) {
    perror("setupSIGALRM(): sigaction() problem");
    exit(0);
  }
  else {
    setITIMER(G_ITIMER_S, G_ITIMER_US);
  }
}
