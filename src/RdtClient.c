// Copyright 2022 190010906
//
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "rdt.h"

int G_port;

int main(int argc, char* argv[]) {
  G_port = getuid();

  RdtSocket_t* socket = openRdtSocket(argv[1], G_port);
  if (socket < 0 ) {
    exit(-1);
  }

  FILE    *infile;
  char    *buffer;
  uint32_t numbytes;

  infile = fopen("./test/dog.jpg", "rb");

  if(infile == NULL)
    return 1;

  fseek(infile, 0L, SEEK_END);
  numbytes = ftell(infile);

  fseek(infile, 0L, SEEK_SET);
  buffer = (char*)calloc(numbytes, sizeof(char));
  if(buffer == NULL)
    return 1;

  fread(buffer, sizeof(char), numbytes, infile);
  fclose(infile);

  rdtSend(socket, buffer, numbytes);

//  char data[] = "Pizzazz and shazam.";
//  rdtSend(socket, &data, sizeof(data));

  closeRdtSocket_t(socket);

  printf("%d\n", numbytes);

  return 0;
}