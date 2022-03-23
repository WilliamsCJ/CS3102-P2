// Copyright 2022 190010906
//
#include "RdtSocket.h"
#include "UdpSocket.h"
#include "sigalrm.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "d_print.h"
#include <inttypes.h>
#include <arpa/inet.h>

int G_retries = 0;
int G_seq_no = 0;

int fsm(int input, RdtSocket_t* socket);

// TODO: Docstring
RdtSocket_t* openRdtClient(const char* hostname, const uint16_t port) {
  printf("Opening socket to %s:%d...\n", hostname, port);
  RdtSocket_t* socket = setupRdtSocket_t(hostname, port);
  if (socket < 0) {
    return socket;
  }

  printf("Ready!\n");

  return socket;
}

/*
 * Function: setup_sockets
 * -----------------------
 *
 * Sets up and opens UDP sockets.
 *
 * hostname (char*): Name of host to open socket to.
 *
 * returns: void
 *
 */
RdtSocket_t* setupRdtSocket_t(const char* hostname, const uint16_t port) {
  RdtSocket_t* socket = (RdtSocket_t*) calloc(1, sizeof(RdtSocket_t));
  int error = 0;

  /* setup local UDP socket */
  socket->local = setupUdpSocket_t((char *) 0, port);
  if (socket->local == (UdpSocket_t *) 0) {
    perror("Couldn't setup local UDP socket");
    error = 1;
  }

  /* setup remote UDP socket */
  socket->remote = setupUdpSocket_t(hostname, port);
  if (socket->remote == (UdpSocket_t *) 0) {
    perror("Couldn't setup remote UDP socket");
    error = 1;
  }

  /* open local UDP socket */
  if (openUdp(socket->local) < 0) {
    perror("Couldn't open UDP socket");
    error = 1;
  }

//  configure_timeout(socket);

  if (error) {
    free(socket);
    socket = (RdtSocket_t *) -1;
  }

  return socket;
}


/*
 * Function: cleanup_sockets
 * ---------------
 * Cleans up and closes the UDP sockets
 *
 * returns: void
 */
void closeRdtSocket_t(RdtSocket_t* socket) {
  closeUdp(socket->local);
  closeUdp(socket->remote);
  free(socket);
}

RdtPacket_t* recvRdtPacket(RdtSocket_t* socket) {
  int r, error = 0;
  int size = sizeof(RdtHeader_t) + RDT_MAX_SIZE;
  UdpBuffer_t buffer;

  uint8_t bytes[size];
  buffer.bytes = bytes;
  buffer.n = size;

  /* Receive UDP packets */
  r = recvUdp(socket->local, &(socket->receive), &buffer);
  if (r < 0) {
    perror("Couldn't receive RDT packet");
    error = 1;
    return (RdtPacket_t*) 0;
  }

  RdtPacket_t* packet = (RdtPacket_t *) calloc(1, size);
  memcpy(packet, buffer.bytes, r);

  return packet;
}

void sendRdtPacket(const RdtSocket_t* socket, RdtPacket_t* packet, const uint8_t n) {
  UdpBuffer_t buffer;
  int r;
  int size = sizeof(RdtHeader_t) + n;
  uint8_t bytes[size];

  memcpy(bytes, packet, size);
  buffer.n = n;
  buffer.bytes = bytes;

  r = sendUdp(socket->local, socket->remote, &buffer);
}

RdtPacket_t* createPacket(RDTPacketType_t type, uint16_t seq_no, uint8_t* data, uint16_t n) {
  RdtPacket_t* packet = (RdtPacket_t *) calloc(1, sizeof(RdtPacket_t));
  packet->header.type = type;
  packet->header.sequence = seq_no;
  packet->header.size = n;

  if (data != NULL) {

    memcpy(&packet->data, data, n);
  }

  return packet;
}

fsm(int input, RdtSocket_t* socket) {
  printf("fsm: old_state=%-12s input=%-12s ", fsm_strings[G_state], fsm_strings[input]);
  printf("fsm: old_state=%-12s input=%-12s ", fsm_strings[G_state], fsm_strings[input]);
  int r;
  int output = 0;

  switch (G_state) {

    /* CLOSED */
    case RDT_STATE_CLOSED:
      switch (input) {
        /* ACTIVE OPEN */
        case RDT_INPUT_ACTIVE_OPEN: {
          G_seq_no = 0; // Set seq_no to 0 as we are starting a new transmission.

          // Create and send packet, with 200MS timeout
          RdtPacket_t* packet = createPacket(SYN, G_seq_no, NULL, 0);
          sendRdtPacket(socket, packet, sizeof(RdtHeader_t));
          if (setITIMER(0, RDT_TIMEOUT_200MS) != 0) {
            perror("Couldn't set timeout");
          }

          // Update state and output
          G_state = RDT_STATE_SYN_SENT;
          output = RDT_ACTION_SND_SYN;
          free(packet);
          break;
        }
        case RDT_INPUT_PASSIVE_OPEN:
          G_state = RDT_STATE_LISTEN;
          break;
      }
      break;

    /* LISTENING */
    case RDT_STATE_LISTEN:
      /* RCV SYN */
      if (input == RDT_EVENT_RCV_SYN) {
        RdtPacket_t* packet = createPacket(SYN_ACK, 1, NULL, 0);

        sendRdtPacket(socket, packet, sizeof(RdtHeader_t));
        G_state = RDT_STATE_ESTABLISHED;
        output = RDT_ACTION_SND_SYN_ACK;
        free(packet);
      }
      break;

    /* SYN_SENT */
    case RDT_STATE_SYN_SENT:
      switch(input) {
        /* RCV SYN_ACK */
        case RDT_EVENT_RCV_SYN_ACK: {
          G_state = RDT_STATE_ESTABLISHED;
          break;
        }
        /* TIMEOUT */
        case RDT_EVENT_TIMEOUT_2MSL: {
          G_state = RDT_STATE_CLOSED;
          G_retries++;
          fsm(RDT_INPUT_ACTIVE_OPEN, socket);
        }
      }
      break;

    /* ESTABLISHED */
    case RDT_STATE_ESTABLISHED:
      switch (input) {
        /* SEND */
        case RDT_INPUT_SEND: {
          RdtPacket_t* packet = createPacket(DATA, 2, G_buf, G_buf_size);

          sendRdtPacket(socket, packet, sizeof(RdtHeader_t) + G_buf_size);
          G_state = RDT_STATE_DATA_SENT;
          G_seq_no += 1;
          output = RDT_ACTION_SND_DATA;
          free(packet);
          break;
        }
        case RDT_EVENT_RCV_DATA: {
          printf("@Shit\n");
          RdtPacket_t* packet = createPacket(DATA_ACK, 3, NULL, 0);

          sendRdtPacket(socket, packet, sizeof(RdtHeader_t));
          G_state = RDT_STATE_ESTABLISHED;
          output = RDT_ACTION_SND_ACK;
          free(packet);
          break;
        }
        case RDT_INPUT_CLOSE: {
          RdtPacket_t* packet = createPacket(FIN, 4, NULL, 0);

          sendRdtPacket(socket, packet, sizeof(RdtHeader_t));
          G_retries = 0;
          G_state = RDT_STATE_FIN_SENT;
          output = RDT_ACTION_SND_FIN;
          free(packet);
          break;
        }
        case RDT_EVENT_RCV_FIN: {
          RdtPacket_t* packet = createPacket(FIN_ACK, 5, NULL, 0);

          sendRdtPacket(socket, packet, sizeof(RdtHeader_t));
          G_state = RDT_STATE_CLOSED;
          output = RDT_ACTION_SND_FIN_ACK;
          free(packet);
          break;
        }
      }
      break;

    /* DATA_SENT */
    case RDT_STATE_DATA_SENT:
      switch (input) {
        case RDT_EVENT_RCV_ACK: {
          G_state = RDT_STATE_ESTABLISHED;
          fsm(RDT_INPUT_CLOSE, socket);
        }
        /* Todo: Timeout case */
      }
      break;

    /* FIN_SENT */
    case RDT_STATE_FIN_SENT:
      if (input == RDT_EVENT_RCV_FIN_ACK) {
        G_state = RDT_STATE_CLOSED;
      };
      break;


    default:
      G_state = RDT_INVALID;
  }
  printf("new_state=%-12s output=%-12s", fsm_strings[G_state], fsm_strings[output]);
}

int RdtTypeTypeToRdtEvent(RDTPacketType_t type) {
  switch (type) {
    case SYN: return RDT_EVENT_RCV_SYN;
    case SYN_ACK: return RDT_EVENT_RCV_SYN_ACK;
    case DATA: return RDT_EVENT_RCV_DATA;
    case DATA_ACK: return RDT_EVENT_RCV_ACK;
    case FIN: return RDT_EVENT_RCV_FIN;
    case FIN_ACK: return RDT_EVENT_RCV_FIN_ACK;
    default: return RDT_INVALID;
  }
}

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

// Data should have network byte ordering before
// being passed to this function.
// Return value is in network byte order.
uint16_t
ipv4_header_checksum(void *data, uint32_t size)
{
  uint16_t *p_16 = (uint16_t *) data;
  uint32_t c = 0;

  // Create sum of 16-bit words: sum is
  // "backwards" for convenenince only.
  for( ; size > 1; size -= 2)
    c += (uint32_t) p_16[size];

  // Deal with odd number of bytes.
  // IPv4 header should be 32-bit aligned.
  if (size) // size == 1 here
    c += (uint16_t) ((uint8_t) p_16[0]);

  // Recover any carry bits from 16-bit sum
  // to get the true one's complement sum.
  while (c & 0xffff0000)
    c =  (c        & 0x0000ffff)
         + ((c >> 16) & 0x0000ffff); // carry bits

  // one's complement of the one's complement sum
  c = ~c;

  return (uint16_t) htons((signed short) c);
}