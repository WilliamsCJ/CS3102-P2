/*
  Simple IPv4 checmsum calculation example.
  saleem, Jan 2022, Jan 2021.

  ** Illustrative example only -- not tested for real use in a protocol!
  ** This will compile:

    clang -Wall -c ipv4_checksum.c

  See also RFC1071(I) and RFC1624(I).
*/
#include <inttypes.h>
#include <arpa/inet.h>
#include <stdio.h>
#include "../d_print/d_print.h"

// Data should have network byte ordering before
// being passed to this function.
// Return value is in network byte order.
uint16_t
ipv4_header_checksum(void *data, uint32_t size) {
  uint16_t *p_16 = (uint16_t *) data;
  uint32_t c = 0;
  int counter = 0;

  printf("HERE: %u\n", size);
  D_HEXDUMP(data, size);
    size -= 3;

  // Create sum of 16-bit words: sum is
  // "backwards" for convenenince only.
  printf("NO: %u\n", c);
  for (; size > 1; size -= 2) {
    printf("-> %u ", (uint32_t) p_16[size]);
    D_HEXDUMP(p_16+size, sizeof(uint16_t));
    c += (uint32_t) p_16[size];
    printf("->> %u\n", c);
    counter++;
  }

  printf("Counter: %d\n", counter);
  printf("C: %u\n", c);

  // Deal with odd number of bytes.
  // IPv4 header should be 32-bit aligned.
  if (size) // size == 1 here
    c += (uint16_t)((uint8_t) p_16[0]);

  // Recover any carry bits from 16-bit sum
  // to get the true one's complement sum.
  while (c & 0xffff0000)
    c = (c & 0x0000ffff)
        + ((c >> 16) & 0x0000ffff); // carry bits

  // one's complement of the one's complement sum
  c = ~c;

  uint16_t a = (uint16_t) htons((signed short) c);
  printf("AGAIN: %d\n", a);
  return a;
}

