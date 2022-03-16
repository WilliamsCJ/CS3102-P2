// Copyright 2022 190010906
//

#ifndef SRC_RDT_H_
#define SRC_RDT_H_

#include <inttypes.h>

typedef struct RdtHeader_s {
    uint8_t sequence;
    uint8_t f_syn;
    uint8_t f_ack;
    uint8_t f_fin;
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
