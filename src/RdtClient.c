// Copyright 2022 190010906
//
#include "RdtSocket.h"
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
  RdtSocket_t* socket;

  int port = atoi(argv[2]);

  if (setupRdtSocket_t(argv[1], port, socket) < 0) {
    perror("Couldn't setup RDT socket");
    return(-1);
  }

  return 0;
}