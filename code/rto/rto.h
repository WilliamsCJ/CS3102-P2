//
// Created by cjdw1 on 03/04/2022.
//

#ifndef CS3102_P2_RTO_H
#define CS3102_P2_RTO_H

#include <inttypes.h>
#include <time.h>

#define HANDSHAKE_RTO ((uint32_t)  200000) // 200ms in microseconds
#define MIN_RTO ((uint32_t)  1000000) //  1s in microseconds
#define MAX_RTO ((uint32_t) 60000000) // 60s in microseconds
#define RTO_TO_SEC(v_) ((uint32_t) v_ / 1000000)
#define RTO_TO_USEC(v_) ((uint32_t) v_ % 1000000)
#define US_TO_MS(v_) ((float) v_ / (float) 1000.0) // us to ms

extern uint32_t T_rto;

uint32_t calculateRTO(uint32_t r);
uint32_t calculateRTT(struct timespec* timestamp);

#endif //CS3102_P2_RTO_H
