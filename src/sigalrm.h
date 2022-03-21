#ifndef SRC_SIGALRM_H
#define SRC_SIGALRM_H

#define G_ITIMER_S   ((unsigned int) 0) // seconds
#define G_ITIMER_US  ((unsigned int) 0) // microseconds

extern sigset_t G_sigmask;

int setITIMER(unsigned int sec, unsigned int usec);
void setupSIGALRM();

#endif //SRC_SIGALRM_H