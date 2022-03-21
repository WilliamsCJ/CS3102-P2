#ifndef CS3102_P1_SIGALRM_H
#define CS3102_P1_SIGALRM_H

#define G_ITIMER_S   ((unsigned int) 1) // seconds
#define G_ITIMER_US  ((unsigned int) 0) // microseconds

void setITIMER(unsigned int sec, unsigned int usec);
void handleSIGALRM(int sig);
void setupSIGALRM();

#endif //CS3102_P1_SIGALRM_H