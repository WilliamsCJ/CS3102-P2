// Copyright 2022 190010906
//

#ifndef SRC_RDTSOCKET_H_
#define SRC_RDTSOCKET_H_

#include <inttypes.h>
#include "UdpSocket.h"

#include <inttypes.h>

#define RDT_MAX_SIZE 8
#define RDT_TIMEOUT_200MS 200000

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

extern int G_state;
extern uint8_t* G_buf;
extern uint8_t G_buf_size;
extern uint16_t G_seq_no;

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
  SYN       = ((uint8_t) 0),
  SYN_ACK   = ((uint8_t) 1),
  DATA      = ((uint8_t) 2),
  DATA_ACK  = ((uint8_t) 3),
  FIN       = ((uint8_t) 4),
  FIN_ACK   = ((uint8_t) 5)
} RDTPacketType_t;


typedef struct RdtHeader_s {
  uint16_t            sequence;
  uint16_t            type;
  uint16_t            size;
  uint16_t            checksum;
} RdtHeader_t;

typedef struct RdtPacket_s {
  RdtHeader_t header;
  uint8_t     data[RDT_MAX_SIZE];  /* TODO: Should this be const */
} RdtPacket_t;

typedef struct RdtBuffer_s {
  uint16_t n;
  uint8_t* bytes;
} RdtBuffer_t;

typedef struct RdtSocket_s {
  UdpSocket_t* local;
  UdpSocket_t* remote;
  UdpSocket_t receive;
  int         state;
} RdtSocket_t;

RdtSocket_t* openRdtClient(const char* hostname, const uint16_t port);

RdtSocket_t* setupRdtSocket_t(const char* hostname, const uint16_t port);

void closeRdtSocket_t(RdtSocket_t* socket);

RdtPacket_t* recvRdtPacket(RdtSocket_t* socket);

int sendRdtPacket(const RdtSocket_t* socket, RdtPacket_t* packet, const uint8_t n);

int RdtTypeTypeToRdtEvent(RDTPacketType_t type);

void fsm(int input, RdtSocket_t* socket);

extern RdtPacket_t* received;

#endif  // SRC_RDTSOCKET_H_

