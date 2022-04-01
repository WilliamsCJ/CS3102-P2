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

int G_port;

int main(int argc, char* argv[]) {
  int error = 0;
  /* TODO: Args parsing */

  /* Set up port and sockets */
  G_port = getuid();
  RdtSocket_t* socket = setupRdtSocket_t(argv[1], G_port);

  rdtListen(socket);

  /* Write your buffer to disk. */
  FILE* pFile = fopen("dog.jpg","wb");

  if (pFile){
    fwrite(G_buf, G_buf_size, 1, pFile);
    puts("Wrote to file!");
  }
  else{
    puts("Something wrong writing to File.");
  }

  fclose(pFile);

  closeRdtSocket_t(socket);

  return 0;
}