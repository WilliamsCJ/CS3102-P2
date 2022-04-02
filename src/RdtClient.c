// Copyright 2022 190010906
//
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "rdt.h"

FILE    *file;
char    *buf;
uint32_t n;

int main(int argc, char* argv[]) {
  if (argc != 3) {
    printf("Usage: ./RdtClient <hostname> <in file>\n");
    return -1;
  }

  RdtSocket_t* socket = openRdtSocket(argv[1], getuid());
  if (socket < 0 ) {
    printf("Couldn't open socket.\n");
    return -1;
  }

  file = fopen(argv[2], "rb");
  if (file == NULL) {
    printf("Couldn't open file: %s\n", argv[2]);
    return -1;
  }

  /* Get file length */
  fseek(file, 0L, SEEK_END);
  n = ftell(file);
  fseek(file, 0L, SEEK_SET);

  /* Allocate buffer for data and copy file bytes */
  buf = (char*) calloc(n, sizeof(char));
  if(buf == NULL) {
    return -1;
  }
  fread(buf, sizeof(char), n, file);
  fclose(file);

  /* Send data over RDT */
  rdtSend(socket, buf, n);

  /* Clean up and return */
  closeRdtSocket_t(socket);
  return 0;
}