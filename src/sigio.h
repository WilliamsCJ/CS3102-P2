//
// Created by CJ Williams on 20/02/2022.
//
#include <signal.h>


#ifndef CS3102_P1_SIGIO_H
#define CS3102_P1_SIGIO_H

extern sigset_t G_sigmask;

int setAsyncFd(int fd);
void setupSIGIO(int net, void(*handler)(int));

#endif //CS3102_P1_SIGIO_H