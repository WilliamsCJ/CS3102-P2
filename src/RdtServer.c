// Copyright 2022 190010906
//
#include "RdtSocket.h"
#include "RDT.h"
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
  int port = atoi(argv[2]);

  RdtSocket_t* socket = setupRdtSocket_t(argv[1], port);

  if (socket < 0) {
    perror("Couldn't setup RDT socket");
    return(-1);
  }

  RdtPacket_t packet;

  recvRdt(socket, const &packet);

  printf("Seq no: %d\n", packet.header.sequence);

  closeRdtSocket_t(socket);

  return 0;
}