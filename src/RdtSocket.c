// Copyright 2022 190010906
//
#include "RdtSocket.h"
#include "UdpSocket.h"
#include "sigalrm.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

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

void recvRdtPacket(RdtSocket_t* socket, RdtPacket_t* packet) {
  UdpBuffer_t buffer;
  uint8_t bytes[RDT_MAX_SIZE];
  int r, error = 0;
  UdpSocket_t receive;

  buffer.bytes = bytes;
  buffer.n = RDT_MAX_SIZE;

  /* Receive UDP packets */
  r = recvUdp(socket->local, &(socket->receive), &buffer);
  if (r < 0) {
    perror("Couldn't receive RDT packet");
    error = 1;
    return;
  }

  /* Cast bytes to RdtPacket_t */
  memcpy(packet, buffer.bytes, r);

  /* TODO: add return value*/
}

void sendRdtPacket(const RdtSocket_t* socket, RdtPacket_t* packet, const uint8_t n) {
  UdpBuffer_t buffer;
  int r;

  uint8_t bytes[n];
  memcpy(bytes, packet, n);
  buffer.n = n;
  buffer.bytes = bytes;

  r = sendUdp(socket->local, socket->remote, &buffer);
  /* TODO: Check bytes sent. */
}

int fsm(int input, RdtSocket_t* socket) {
  printf("fsm: old_state=%s, input=%s\n", fsm_strings[G_state], fsm_strings[input]);
  int r;
  int output;

  switch (G_state) {
    case RDT_STATE_CLOSED:
      switch (input) {
        case RDT_INPUT_ACTIVE_OPEN: {
          G_seq_no = 0;
          RdtPacket_t* packet = (RdtPacket_t *) calloc(1, sizeof(RdtPacket_t));
          packet->header.sequence = G_seq_no;
          packet->header.type = SYN;

          sendRdtPacket(socket, packet, sizeof(RdtPacket_t));
          if (setITIMER(0, RDT_TIMEOUT_200MS) != 0) {
            perror("Couldn't set timeout");
            return -1;
          }

          G_state = RDT_STATE_SYN_SENT;
          free(packet);
          return 0;
        }
        case RDT_INPUT_PASSIVE_OPEN:
          G_state = RDT_STATE_LISTEN;
          return 0;
      }
    case RDT_STATE_LISTEN:
      if (input == RDT_EVENT_RCV_SYN) {
        RdtPacket_t* packet = (RdtPacket_t *) calloc(1, sizeof(RdtPacket_t));
        packet->header.sequence = 11;
        packet->header.type = SYN_ACK;

        sendRdtPacket(socket, packet, sizeof(RdtPacket_t));
        G_state = RDT_STATE_ESTABLISHED;
        free(packet);
        return 0;
      }
    case RDT_STATE_SYN_SENT:
      switch(input) {
        case RDT_EVENT_RCV_SYN_ACK: {
          G_state = RDT_STATE_ESTABLISHED;
          return G_state;
        }
        case RDT_EVENT_TIMEOUT_2MSL: {
          G_state = RDT_STATE_CLOSED;
          G_retries++;
          fsm(RDT_INPUT_ACTIVE_OPEN, socket);
          return 0;
        }
      }
    case RDT_STATE_ESTABLISHED:
      switch (input) {
        case RDT_INPUT_SEND: {
          RdtPacket_t* packet = (RdtPacket_t *) calloc(1, sizeof(RdtPacket_t));
          packet->header.sequence = G_seq_no;
          packet->header.type = DATA;
          packet->data = G_buf;

          sendRdtPacket(socket, packet, sizeof(RdtHeader_t) + G_buf_size);
          G_state = RDT_STATE_DATA_SENT;
          G_seq_no += 1;
          free(packet);
          return 0;
        }
        case RDT_EVENT_RCV_DATA: {
          RdtPacket_t* packet = (RdtPacket_t *) calloc(1, sizeof(RdtPacket_t));
          packet->header.sequence = 1;
          packet->header.type = DATA_ACK;

          sendRdtPacket(socket, packet, sizeof(RdtPacket_t));
          G_state = RDT_STATE_ESTABLISHED;
          free(packet);
        }
        case RDT_EVENT_RCV_FIN: {
          RdtPacket_t* packet = (RdtPacket_t *) calloc(1, sizeof(RdtPacket_t));
          packet->header.sequence = 11;
          packet->header.type = FIN_ACK;

          sendRdtPacket(socket, packet, sizeof(RdtPacket_t));
          G_state = RDT_STATE_CLOSED;
          free(packet);
          return 0;
        }
        case RDT_INPUT_CLOSE: {
          RdtPacket_t* packet = (RdtPacket_t *) calloc(1, sizeof(RdtPacket_t));
          packet->header.sequence = 11;
          packet->header.type = FIN;

          sendRdtPacket(socket, packet, sizeof(RdtPacket_t));
          G_retries = 0;
          G_state = RDT_STATE_FIN_SENT;
          free(packet);
          return 0;
        }
      }
    case RDT_STATE_FIN_SENT:
      if (input == RDT_EVENT_RCV_FIN_ACK) {
        G_state = RDT_STATE_CLOSED;
        return 0;
      };
    default:
      printf("ERROR\n");
      printf("%s\n", fsm_strings[G_state]);
      printf("%s\n", fsm_strings[input]);
      G_state = RDT_INVALID;
  }
  return -1;
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