//
// Created by cjdw1 on 04/04/2022.
//

// Copyright 2022 190010906
//
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "d_print/d_print.h"
#include "rdt.h"

FILE    *file;
FILE    *out;
char    *buf;
uint32_t n;
struct timespec start;
struct timespec end;
int counter = 0;
int max = 10;

int main(int argc, char* argv[]) {
  if (argc != 3) {
    printf("Usage: ./RdtClient hostname file\n");
    return -1;
  }

  G_fp = fopen(filename, "w");
  d_advise(G_fp, "Attempt,Avg. RTT\n");

  RdtSocket_t* socket = setupRdtSocket_t(argv[1], getuid());
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

  if (argc == 4 && strcmp(argv[3], "time") == 0) {
    if (clock_gettime(CLOCK_REALTIME, &start) < 0) {
      printf("Error starting timer.\n");
      return -1;
    }
  }

  while(counter < max) {
    /* Send data over RDT */
    rdtSend(socket, buf, n);
    d_advise(G_fp, "%d,%lf\n", counter + 1, rtt);
    counter++
  }

  /* Clean up and return */
  closeRdtSocket_t(socket);
  return 0;
}