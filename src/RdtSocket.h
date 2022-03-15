// Copyright 2022 190010906
//

#ifndef SRC_RDTSOCKET_H_
#define SRC_RDTSOCKET_H_

#include <inttypes.h>
#include <netinet/in.h>

typedef struct RdtSocket_s {
  int sd;
  struct sockaddr_in addr;
} RdtSocket_t;

RdtSocket_t* setupRdtSocket_t(const char* hostname, const uint64_t port);

#endif  // SRC_RDTSOCKET_H_
