//
// Created by cjdw1 on 03/04/2022.
//

#ifndef CS3102_P2_RTO_H
#define CS3102_P2_RTO_H

#include <inttypes.h>
#include <time.h>

#define MIN_RTO ((uint32_t)  1000000) //  1s in microseconds
#define MAX_RTO ((uint32_t) 60000000) // 60s in microseconds

extern uint32_t T_rto;

uint32_t calculateRTO(uint32_t r);
uint32_t calculateRTT(struct timespec* timestamp);

#endif //CS3102_P2_RTO_H
