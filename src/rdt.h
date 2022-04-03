// Copyright 2022 190010906
//

#ifndef SRC_RDTSOCKET_H_
#define SRC_RDTSOCKET_H_

#include <inttypes.h>
#include "UdpSocket/UdpSocket.h"

#define RDT_MAX_SIZE              ((uint16_t) 1300)
#define RDT_MAX_RETRIES           ((int) 5)
#define RDT_TIMEOUT_200MS         (200000)

#define RDT_INVALID               ((int)  0)

#define RDT_INPUT_ACTIVE_OPEN     ((int) 1)
#define RDT_INPUT_PASSIVE_OPEN    ((int) 2)
#define RDT_INPUT_CLOSE           ((int) 3)
#define RDT_INPUT_SEND            ((int) 4)

#define RDT_ACTION_SND_SYN        ((int)  5)
#define RDT_ACTION_SND_SYN_ACK    ((int)  6)
#define RDT_ACTION_SND_DATA       ((int)  7)
#define RDT_ACTION_SND_ACK        ((int)  8)
#define RDT_ACTION_SND_FIN        ((int)  9)
#define RDT_ACTION_SND_FIN_ACK    ((int) 10)

#define RDT_EVENT_RCV_SYN         ((int) 11)
#define RDT_EVENT_RCV_SYN_ACK     ((int) 12)
#define RDT_EVENT_RCV_DATA        ((int) 13)
#define RDT_EVENT_RCV_ACK         ((int) 14)
#define RDT_EVENT_RCV_FIN         ((int) 15)
#define RDT_EVENT_RCV_FIN_ACK     ((int) 16)
#define RDT_EVENT_TIMEOUT_2MSL    ((int) 17)

#define RDT_STATE_CLOSED          ((int) 18)
#define RDT_STATE_LISTEN          ((int) 19)
#define RDT_STATE_SYN_SENT        ((int) 20)
#define RDT_STATE_SYN_RCVD        ((int) 21)
#define RDT_STATE_ESTABLISHED     ((int) 22)
#define RDT_STATE_DATA_SENT       ((int) 23)
#define RDT_STATE_FIN_SENT        ((int) 24)
#define RDT_STATE_FIN_RCV         ((int) 25)

static const char* fsm_strings[] = {
    "---",
    "active OPEN",
    "passive OPEN",
    "CLOSE",
    "SEND",
    "snd SYN",
    "snd SYN_ACK",
    "snd DATA",
    "snd ACK",
    "snd FIN",
    "snd FIN_ACK",
    "rcv SYN",
    "rcv SYN_ACK",
    "rcv DATA",
    "rcv ACK",
    "rcv FIN",
    "rcv FIN_ACK",
    "timeout=2msl",
    "CLOSE",
    "LISTEN",
    "SYN_SENT",
    "SYN_RCVD",
    "ESTABLISHED",
    "DATA_SENT",
    "FIN_SENT",
    "FIN_RCV"
};


typedef enum {
  SYN       = ((uint16_t) 0),
  SYN_ACK   = ((uint16_t) 1),
  DATA      = ((uint16_t) 2),
  DATA_ACK  = ((uint16_t) 3),
  FIN       = ((uint16_t) 4),
  FIN_ACK   = ((uint16_t) 5)
} RDTPacketType_t;


typedef struct RdtHeader_s {
  uint32_t            sequence;
  uint16_t            type;
  uint16_t            checksum;
  uint16_t            size;
  uint16_t            padding;
} RdtHeader_t;

typedef struct RdtPacket_s {
  RdtHeader_t header;
  uint8_t     data[RDT_MAX_SIZE];  /* TODO: Should this be const */
} RdtPacket_t;

typedef struct RdtSocket_s {
  UdpSocket_t* local;
  UdpSocket_t* remote;
  UdpSocket_t receive;
  int         state;
} RdtSocket_t;

extern uint8_t* G_buf;
extern uint32_t G_buf_size;

// TODO Docstrings

RdtSocket_t* openRdtSocket(const char* hostname, const uint16_t port);

RdtSocket_t* setupRdtSocket_t(const char* hostname, const uint16_t port);

void closeRdtSocket_t(RdtSocket_t* socket);

void rdtSend(RdtSocket_t* socket, const void* buf, uint32_t n);

void rdtListen(RdtSocket_t* socket);


#endif  // SRC_RDTSOCKET_H_

