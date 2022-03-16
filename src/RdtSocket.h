// Copyright 2022 190010906
//

#ifndef SRC_RDTSOCKET_H_
#define SRC_RDTSOCKET_H_

#include <inttypes.h>
#include "UdpSocket.h"

typedef struct RdtSocket_s {
  UdpSocket_t* local;
  UdpSocket_t* remote;
  UdpSocket_t receive;
} RdtSocket_t;

RdtSocket_t* setupRdtSocket_t(const char* hostname, const uint16_t port);

void closeRdtSocket_t(RdtSocket_t* socket);

void recvRdt(const RdtSocket_t* socket, const RdtPacket_t* packet);

void sendRdt(const RdtSocket_t* socket, const RdtPacket_t* packet, const uint8_t n);

#endif  // SRC_RDTSOCKET_H_

