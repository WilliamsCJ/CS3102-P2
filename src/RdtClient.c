// Copyright 2022 190010906
//
#include "RdtSocket.h"
#include <stdlib.h>
#include <stdio.h>

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

  char buf[] = "Test";

  RdtPacket_t packet = {header, &buf};

  sendRdt(socket, const &packet);

  closeRdtSocket_t(socket);

  return 0;
}