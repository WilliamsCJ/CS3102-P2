//
// 190010906, March 2022.
//
#include <inttypes.h>
#include <time.h>

#include "rto.h"

/* GLOBAL VARIABLES (using lecture notation) START */
uint32_t T_rto, t_n, r_n, s_n, v_n;
uint32_t T_rto = 0;
/* GLOBAL VARIABLES END */


/**
 * Calculates current RTO from last measured RTT.
 *
 * CITATION: Modified from rto.c by Saleem Bhatti, Jan 2022, Jan 2021.
 *
 * Values are in microseconds (us)
 *
 * @param r Measured rtt.
 * @return uint32_t current RTT.
 */
uint32_t calculateRTO(uint32_t r) {
  if (T_rto == 0) {
    /*
      The first measurement of RTT
      RFC6298(PS) Section 2.2
    */
    s_n = r_n;
    v_n = r_n >> 1; // r divide by 2
  }

  else {
    /*
       After the first measurement of RTT.
       RFC6298(PS) Section 2.3

       alpha = 1/8 = 0.001_2, beta = 1/4 = 0.01_2
       (1 - alpha) = 7/8 = 0.111_2, (1 - beta) = 3/4 = 0.11_2
    */

    // v <- (1 - beta) * v   +  beta * |s - r|
    v_n = (v_n >> 1) + (v_n >> 2) + ((s_n > r_n ? s_n - r_n : r_n - s_n) >> 2);

    // s <- (1 - alpha) * s                    +  alpha * r
    s_n = (s_n >> 1) + (s_n >> 2) + (s_n >> 3) + (r_n >> 3);
  }

  /*
    RFC6298(PS) Sections 2.3 and 2.4
    t_n = s_n + Kv_n,   K = 4 = 10_2
  */
  t_n = s_n + (v_n << 2);

  T_rto = t_n   < MIN_RTO ? MIN_RTO : t_n;   // RFC6298(PS) Section 2.4
  T_rto = T_rto > MAX_RTO ? MAX_RTO : T_rto; // RFC6298(PS) Section 2.5

  return T_rto;
}

/**
 * Calculate RTT from previous timestamp in microseconds
 * @param timestamp from packet send.
 * @return uint32_t RTT in microseconds
 */
uint32_t calculateRTT(struct timespec* timestamp) {
  struct timespec current;
  if (clock_gettime(CLOCK_REALTIME, &current)) {
    perror("Couldn't get current timestamp for RTT calculation");
  }

  uint32_t sec = current.tv_nsec - timestamp->tv_sec;

  uint32_t rtt = (current.tv_nsec - timestamp->tv_nsec) / 1000;
  if (rtt > 999) {
    sec += 1;
    rtt = 0;
  }

  rtt += sec * 1e6;
  return rtt;
}