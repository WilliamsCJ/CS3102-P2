// Copyright 2022 190010906
//
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "sigio/sigio.h"
#include "rdt.h"

int counter = 0;
int max = 10;


int main(int argc, char* argv[]) {
  if (argc != 2) {
    printf("Usage: ./RdtServerRTT\n");
    return -1;
  }

  FILE *pFile = fopen(argv[1], "wb");
  if (!pFile) {
    printf("Couldn't open file: %s\n", argv[1]);
    return -1;
  }

  RdtSocket_t *socket = setupRdtSocket_t(NULL, getuid());
  if (socket < 0) {
    return -1;
  }

  while (counter < max) {
    printf("Round %d:\n", counter + 1);
    rdtListen(socket);
    printf("Received %d bytes.\n", (G_seq_no - G_seq_init));
    counter++;
  }

  closeRdtSocket_t(socket);

  printf("Bye!\n");
  return 0;
}