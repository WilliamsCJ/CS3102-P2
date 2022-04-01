// Copyright 2022 190010906
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <stdbool.h>

#include "RdtSocket.h"
#include "UdpSocket.h"
#include "sigalrm.h"
#include "checksum/checksum.h"

RdtPacket_t* received;
bool G_checksum_match;
int G_retries = 0;
uint16_t G_seq_no = 0;
uint16_t G_prev_seq_no = 0;

void fsm(int input, RdtSocket_t* socket);

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

/*
 * Function: recvRdtPacket
 * -----------------------
 * Receive RDT packet from socket.
 *
 * socket: Pointer to RdtSocket_t to receive packet from.
 *
 * returns: Pointer to RdtPacket_t.
 */
RdtPacket_t* recvRdtPacket(RdtSocket_t* socket) {
  int r, error = 0;
  int size = sizeof(RdtHeader_t) + RDT_MAX_SIZE;

  /* Create UdpBuffer_t to receive datagram */
  UdpBuffer_t buffer;
  uint8_t bytes[size];
  buffer.bytes = bytes;
  buffer.n = size;

  /* Receive UDP datagram */
  r = recvUdp(socket->local, &(socket->receive), &buffer);
  if (r < 0) {
    perror("Couldn't receive RDT packet");
    error = 1;
    return (RdtPacket_t*) 0;
  }

  /* Create RdtPacket_t and copy bytes */
  RdtPacket_t* packet = (RdtPacket_t *) calloc(1, size);
  memcpy(packet, buffer.bytes, r);

  /* Calculate expected checksum and compare */
  uint16_t checksum = packet->header.checksum;
  packet->header.checksum = htons(0);
  uint16_t expected = ipv4_header_checksum(packet, r);

  G_checksum_match = (expected == checksum);

  /* Convert header fields to host byteorder */
  packet->header.sequence = ntohs(packet->header.sequence);
  packet->header.size = ntohs(packet->header.size);
  packet->header.type = ntohs(packet->header.type);

  return packet;
}

int sendRdtPacket(const RdtSocket_t* socket, RdtPacket_t* packet, const uint8_t n) {
  UdpBuffer_t buffer;
  int r;
  int size = sizeof(RdtHeader_t) + n;
  uint8_t bytes[size];

  memcpy(bytes, packet, size);
  buffer.n = n;
  buffer.bytes = bytes;

  return sendUdp(socket->local, socket->remote, &buffer);
}

/*
 * Function: createPacket
 * ----------------------
 * Creates RdtPacket for given type, sequence number and (optional) data.
 *
 * type: the RDTPacketType_t of the packet to create.
 * seq_no: The uint16_t sequence number to give the packet.
 * data: (Optional) pointer to uint8_t data. Should be NULL if type is not DATA.
 *
 * returns: Pointer to created RdtPacket_t.
 */
RdtPacket_t* createPacket(RDTPacketType_t type, uint16_t seq_no, uint8_t* data) {
  uint16_t n = 0;

  /* Allocate memory for packet. Set type and sequence number. Zero checksum value. */
  RdtPacket_t* packet = (RdtPacket_t *) calloc(1, sizeof(RdtPacket_t));
  packet->header.type = htons(type);
  packet->header.sequence = htons(seq_no);
  packet->header.checksum = htons(0);

  /* Calculate the header field value */
  if (data != NULL) {
    uint16_t diff = G_buf_size - G_seq_no;

    if (diff > RDT_MAX_SIZE) {
      n = RDT_MAX_SIZE;
    } else {
      n = diff;
    }

    memcpy(&packet->data, data+G_seq_no, n);
  }

  /* Set header size and checksum */
  packet->header.size = htons(n);
  packet->header.checksum = ipv4_header_checksum(packet, sizeof(RdtHeader_t) + n);

  return packet;
}


void fsm(int input, RdtSocket_t* socket) {
  printf("fsm: old_state=%-12s input=%-12s ", fsm_strings[G_state], fsm_strings[input]);
  int r, error;
  int output = 0;

  switch (G_state) {

    /* CLOSED */
    case RDT_STATE_CLOSED:
      switch (input) {
        /* ACTIVE OPEN */
        case RDT_INPUT_ACTIVE_OPEN: {
          G_seq_no = 0; // Set seq_no to 0 as we are starting a new transmission.

          // Create and send packet, with 200MS timeout
          RdtPacket_t* packet = createPacket(SYN, G_seq_no, NULL);
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
        G_seq_no = received->header.sequence;
        RdtPacket_t* packet = createPacket(SYN_ACK, G_seq_no, NULL);

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

          if (G_retries >= 10) {
            fsm(RDT_INPUT_CLOSE, socket);
            break;
          }

          G_retries++;
          fsm(RDT_INPUT_ACTIVE_OPEN, socket);
        }
      }
      break;

    /* ESTABLISHED */
    case RDT_STATE_ESTABLISHED:
      switch (input) {
        /* SEND */
        send:
        case RDT_INPUT_SEND: {
          RdtPacket_t* packet = createPacket(DATA, G_seq_no, G_buf);
          sendRdtPacket(socket, packet, sizeof(RdtHeader_t) + G_buf_size);
          if (setITIMER(0, RDT_TIMEOUT_200MS) != 0) {
            perror("Couldn't set timeout");
          }

          G_state = RDT_STATE_DATA_SENT;
          G_seq_no += ntohs(packet->header.size);
          output = RDT_ACTION_SND_DATA;
          free(packet);
          break;
        }
        case RDT_EVENT_RCV_DATA: {
          int seq = G_seq_no;

//          if (G_checksum_match) {
//            seq += received->header.size;
//            G_seq_no = seq;
//          } else {
//
//          }

          seq += received->header.size;
          G_seq_no = seq;

          RdtPacket_t* packet = createPacket(DATA_ACK, seq, NULL);
          sendRdtPacket(socket, packet, sizeof(RdtHeader_t));

          G_state = RDT_STATE_ESTABLISHED;
          output = RDT_ACTION_SND_ACK;
          free(packet);
          break;
        }
        close:
        case RDT_INPUT_CLOSE: {
          RdtPacket_t* packet = createPacket(FIN, G_seq_no, NULL);

          sendRdtPacket(socket, packet, sizeof(RdtHeader_t));
          G_retries = 0;
          G_state = RDT_STATE_FIN_SENT;
          output = RDT_ACTION_SND_FIN;
          free(packet);
          break;
        }
        case RDT_EVENT_RCV_FIN: {
          RdtPacket_t* packet = createPacket(FIN_ACK, received->header.sequence, NULL);

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
          if (G_seq_no < G_buf_size) {
            if (received->header.sequence < G_seq_no) {
              G_seq_no = received->header.sequence;
            }
            goto send;
          }
          G_state = RDT_STATE_ESTABLISHED;
          break;
        }
        closer:
        case RDT_INPUT_CLOSE: {
          RdtPacket_t* packet = createPacket(FIN, G_seq_no, NULL);
          r = sendRdtPacket(socket, packet, sizeof(RdtHeader_t));
          G_retries = 0;
          G_state = RDT_STATE_FIN_SENT;
          output = RDT_ACTION_SND_FIN;
          free(packet);
          break;
        }
        case RDT_EVENT_TIMEOUT_2MSL: {
          if (G_retries < 10) {
            G_retries++;
            G_seq_no -= RDT_MAX_SIZE;
            goto send ;
          }

          G_state = RDT_STATE_CLOSED;
          break;
        }
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
      printf("\n");
      break;
  }
  printf("new_state=%-12s output=%-12s\n", fsm_strings[G_state], fsm_strings[output]);
}

/*
 * Function: rdtTypeToRdtEvent
 * ---------------------------
 * Converts the value of the type field in RDT header to an RDT Event.
 * This event is used by the fsm.
 *
 * type: The RdtPacketType_t value.
 *
 * returns: int, the RDT_Event
 */
int rdtTypeToRdtEvent(RDTPacketType_t type) {
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
