// checksum.c - IPv4 Header Checksum for calculating RDT checksum.
//
#include <inttypes.h>
#include <arpa/inet.h>

/**
 * Calculates checksum using IPv4 Header Checksum algorithm.
 *
 * CITATION: Modified from ipv4_header_checksum in ipv4_checksum.c
 *           by saleem, Jan 2022, Jan 2021.
 *
 * NOTES FROM ORIGINAL:
 * - Data should have network byte ordering before
 *   being passed to this function
 * - Return value is in network byte order.
 *
 * @param data
 * @param size
 * @return
 */
uint16_t ipv4_header_checksum(void *data, uint32_t size) {
  uint8_t *p_8 = (uint8_t *) data;
  uint32_t c = 0;

  // Create sum of 16-bit words: sum is
  // "backwards" for convenenince only.
  for( ; size > 1; size -= 2) {
    c += (uint32_t) p_8[size];
  }

  // Deal with odd number of bytes.
  // IPv4 header should be 32-bit aligned.
  if (size) // size == 1 here
    c += (uint16_t) ((uint8_t) p_8[0]);

  // Recover any carry bits from 16-bit sum
  // to get the true one's complement sum.
  while (c & 0xffff0000)
    c =  (c        & 0x0000ffff)
         + ((c >> 16) & 0x0000ffff); // carry bits

  // one's complement of the one's complement sum
  c = ~c;

  return (uint16_t) htons((signed short) c);
}

