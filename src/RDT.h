// Copyright 2022 190010906
//

#ifndef SRC_RDT_H_
#define SRC_RDT_H_

#include <inttypes.h>

#define RDT_MAX_SIZE 20

#define DATA    0
#define ACK     1
#define SYN     2
#define SYNACK  3
#define FIN     4
#define FINACK  5

typedef enum {
    CLOSED      = 0;
    LISTEN      = 1;
    SYN_SENT    = 2;
    SYN_RCVD    = 3;
    ESTABLISHED = 4;
    FIN_SENT    = 5;
    FIN_RCVD    = 6;
} FSM;

typedef struct RdtHeader_s {
    uint8_t sequence;
    uint8_t flag;
    uint16_t checksum;
} RdtHeader_t;

typedef struct RdtPacket_s {
    RdtHeader_t header;
} RdtPacket_t;

typedef struct RdtBuffer_s {
    uint16_t n;
    uint8_t* bytes;
} RdtBuffer_t;

#endif  // SRC_RDT_H_
