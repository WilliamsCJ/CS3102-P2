//
// Created by CJ Williams on 20/02/2022.
//
#include <inttypes.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

#include "sigalrm.h"

/* signal action / handler hook */
struct sigaction G_sigalrm;

/* timer value */
struct itimerval G_timer;


/*
 * Function: setITIMER
 * --------------------
 * Modified from setITIMER from timer.c:
 *
 * saleem, Jan2022
 * saleem, Jan2021
 * saleem, Jan2004
 * saleem, Nov2002
 */
int setITIMER(uint32_t sec, uint32_t usec) {
  G_timer.it_interval.tv_sec = 0;
  G_timer.it_interval.tv_usec = 0;
  G_timer.it_value.tv_sec = sec;
  G_timer.it_value.tv_usec = usec;

  return setitimer(ITIMER_REAL, &G_timer, (struct itimerval *) 0);
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
void setupSIGALRM(void(*handler)(int)) {
  sigaddset(&G_sigmask, SIGALRM);

  G_sigalrm.sa_handler = handler;
  G_sigalrm.sa_flags = 0;

  /* TODO: Change error handling */
  if (sigaction(SIGALRM, &G_sigalrm, (struct sigaction *) 0) < 0) {
    perror("setupSIGALRM(): sigaction() problem");
    exit(0);
  }
}
