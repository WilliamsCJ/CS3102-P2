// Copyright 2022 190010906
//
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

#include "d_print/d_print.h"
#include "sigio/sigio.h"
#include "sigalrm/sigalrm.h"
#include "rdt.h"


int main(int argc, char* argv[]) {
  if (argc != 3 && argc != 4) {
    printf("Usage: ./RdtServer hostname file [debug]\n");
    return -1;
  }

  if (argc == 4 && strcmp(argv[3], "debug")) {
    G_debug = true;
  }

  FILE* pFile = fopen(argv[2],"wb");
  if (!pFile) {
    printf("Couldn't open file: %s\n", argv[1]);
    return -1;
  }

  RdtSocket_t* socket = setupRdtSocket_t(argv[1], getuid());
  if (socket < 0 ) {
    return -1;
  }

  rdtListen(socket);
  fwrite(G_buf, G_buf_size, 1, pFile);

  fclose(pFile);
  closeRdtSocket_t(socket);
  return 0;
}