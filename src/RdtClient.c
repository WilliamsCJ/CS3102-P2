// Copyright 2022 190010906
//
#include "RdtSocket.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char* argv[]) {
  int port = atoi(argv[2]);

  RdtSocket_t* socket = setupRdtSocket_t(argv[1], port);

  if (socket < 0) {
    perror("Couldn't setup RDT socket");
    return(-1);
  }

  RdtHeader_t header = {
          12,
          1,
          0,
          0,
          20
  };

  RdtPacket_t packet;
  packet.header = header;

  sendRdt(socket, &packet, sizeof(RdtHeader_t));

  closeRdtSocket_t(socket);

  return 0;
}