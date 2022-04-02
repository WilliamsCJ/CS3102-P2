// Copyright 2022 190010906
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <stdbool.h>

#include "rdt.h"
#include "UdpSocket/UdpSocket.h"
#include "sigalrm/sigalrm.h"
#include "checksum/checksum.h"
#include "sigio/sigio.h"
#include "d_print/d_print.h"

RdtSocket_t* G_socket;
RdtPacket_t* received;
RdtPacket_t* G_packet;
bool G_checksum_match;
int G_retries = 0;
int G_state = RDT_STATE_CLOSED;
uint32_t G_seq_no = 0;
uint8_t* G_buf;
uint32_t G_buf_size;

void fsm(int input);
void rdtOpen(RdtSocket_t* socket);
void rdtClose(RdtSocket_t* socket);
void handleSIGALRM(int sig);
void handleSIGIO(int sig);
int rdtTypeToRdtEvent(RDTPacketType_t type);

/* API START */
/**
 * Opens an RDT socket.
 * @param hostname The hostname to open the socket to.
 * @param port The port to open the socket on.
 * @return Pointer to RdtSocket_t.
 */
RdtSocket_t* openRdtSocket(const char* hostname, const uint16_t port) {
  printf("Opening socket to %s:%d...\n", hostname, port);
  RdtSocket_t* socket = setupRdtSocket_t(hostname, port);
  if (socket < 0) {
    return socket;
  }

  printf("Ready!\n");

  return socket;
}

/**
 * Send data over RDT.
 * @param socket The socket to send data over.
 * @param buf Buffer containing the data
 * @param n The size of 'buf'
 */
void rdtSend(RdtSocket_t* socket, const void* buf, uint32_t n) {
  rdtOpen(socket);

  G_buf = (uint8_t*) buf;
  G_buf_size = n;
  fsm(RDT_INPUT_SEND);

  // TODO: Check that the buffer has been completely sent
  while(G_state != RDT_STATE_ESTABLISHED) {
    (void) pause(); // Wait for signal
  }

  rdtClose(G_socket);
}

/**
 * Listen for RDT connections on socket.
 * @param socket Socket to listen on.
 */
void rdtListen(RdtSocket_t* socket) {
  G_socket = socket;
  G_state = RDT_STATE_LISTEN;

  setupSIGIO(G_socket->local->sd, handleSIGIO);
  setupSIGALRM(handleSIGALRM);

  // TODO: FSM

  while(G_state != RDT_STATE_CLOSED) {
    (void) pause(); // Wait for signal
  }
}
/* API END */


/* CONNECTION MANAGEMENT START */
/**
 * Open an RDT socket.
 * @param socket Uninitialised RDT socket.
 */
void rdtOpen(RdtSocket_t* socket) {
  /* Setup SIGIO to handle network events. */
  G_socket = socket;

  setupSIGIO(G_socket->local->sd, handleSIGIO);
  setupSIGALRM(handleSIGALRM);

  fsm(RDT_INPUT_ACTIVE_OPEN);

  while(G_state != RDT_STATE_ESTABLISHED) {
    (void) pause(); // Wait for signal
  }
}

/**
 * Closes an RDT socket.
 * @param socket The socket to close.
 */
void rdtClose(RdtSocket_t* socket) {
  fsm(RDT_INPUT_CLOSE);

  while(G_state != RDT_STATE_CLOSED) {
    (void) pause();
  }
}
/* CONNECTION MANAGEMENT CLOSE */


/* SOCKETS START */
/**
 * Sets up and opens UDP sockets to support RDT.
 * @param hostname The name of the host to open socket to.
 * @param port The port to open socket on.
 * @return Pointer to RdtSocket_t.
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

  if (error) {
    free(socket);
    socket = (RdtSocket_t *) -1;
  }

  return socket;
}

/**
 * Cleans up and closes the underlying UDP sockets.
 * @param socket The socket to close.
 */
void closeRdtSocket_t(RdtSocket_t* socket) {
  closeUdp(socket->local);
  closeUdp(socket->remote);
  free(socket);
}
/* SOCKETS END */


/* PACKETS START */
/**
 * Receive an RDT packet from the socket.
 * @param socket Pointer to RdtSocket_t to receive packet from.
 * @return Pointer to RdtPacket_t.
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
  packet->header.sequence = ntohl(packet->header.sequence);
  packet->header.size = ntohs(packet->header.size);
  packet->header.type = ntohs(packet->header.type);

  return packet;
}

// TODO: Is the right return value?
/**
 * Sends an RdtPacket_t over the specified RdtSocket.
 * @param socket The socket to send the packet over.
 * @param packet Pointer to the packet to send.
 * @param n The size of 'packet' (header + data).
 * @return TODO: What?
 */
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

/**
 * Creates an RDTPacket for a given type, sequence number and (optional) data.
 * @param type The RDTPacketType_t of the packet to create.
 * @param seq_no The uint16_t sequence number to give the packet.
 * @param data (Optional) pointer to uint8_t data. Should be NULL if type is not DATA.
 * @return Pointer to created RdtPacket_t.
 */
RdtPacket_t* createPacket(RDTPacketType_t type, uint32_t seq_no, uint8_t* data) {
  uint16_t n = 0;

  /* Allocate memory for packet. Set type and sequence number. Zero checksum value. */
  RdtPacket_t* packet = (RdtPacket_t *) calloc(1, sizeof(RdtPacket_t));
  packet->header.type = htons(type);
  packet->header.sequence = htonl(seq_no);
  packet->header.checksum = htons(0);

  /* Calculate the header field value */
  if (data != NULL) {
    uint32_t diff = G_buf_size - G_seq_no;

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
/* PACKETS END */


/* SIGNALS START */
/**
 * SIGIO handler. Called when packets are received.
 * @param sig
 */
void handleSIGIO(int sig) {
  if (sig == SIGIO) {
    /* protect the network and keyboard reads from signals */
    sigprocmask(SIG_BLOCK, &G_sigmask, (sigset_t *) 0);

    received = recvRdtPacket(G_socket);

    int input = rdtTypeToRdtEvent(received->header.type);

    fsm(input);

    free(received);

    /* allow the signals to be delivered */
    sigprocmask(SIG_UNBLOCK, &G_sigmask, (sigset_t *) 0);
  }
  else {
    perror("handleSIGIO(): got a bad signal number");
    exit(1);
  }
}

/**
 * SIGALRM handler. Triggered 200ms after ITIMER set for alarm.
 * @param sig
 */
void handleSIGALRM(int sig) {
  if (sig == SIGALRM) {
    /* protect handler actions from signals */
    sigprocmask(SIG_BLOCK, &G_sigmask, (sigset_t *) 0);

    /* Send a packet */
    fsm(RDT_EVENT_TIMEOUT_2MSL);

    /* protect handler actions from signals */
    sigprocmask(SIG_UNBLOCK, &G_sigmask, (sigset_t *) 0);
  }
  else {
    perror("handleSIGALRM() got a bad signal");
    exit(1);
  }
}
/* SIGNALS END */


/* OTHER*/
/**
 * RDT Finite State Machine
 * @param input
 */
void fsm(int input) {
//  printf("fsm: old_state=%-12s input=%-12s ", fsm_strings[G_state], fsm_strings[input]);
  int r, error;
  int output = 0;

  switch (G_state) {

    /* CLOSED */
    case RDT_STATE_CLOSED:
      switch (input) {
        /* ACTIVE OPEN */
        open:
        case RDT_INPUT_ACTIVE_OPEN: {
          G_seq_no = 0; // Set seq_no to 0 as we are starting a new transmission.

          // Create and send packet, with 200MS timeout
          G_packet = createPacket(SYN, G_seq_no, NULL);
          sendRdtPacket(G_socket, G_packet, sizeof(RdtHeader_t));
          if (setITIMER(0, RDT_TIMEOUT_200MS) != 0) {
            perror("Couldn't set timeout");
          }

          // Update state and output
          G_state = RDT_STATE_SYN_SENT;
          output = RDT_ACTION_SND_SYN;
          free(G_packet);
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
        G_packet = createPacket(SYN_ACK, G_seq_no, NULL);

        sendRdtPacket(G_socket, G_packet, sizeof(RdtHeader_t));
        G_state = RDT_STATE_ESTABLISHED;
        output = RDT_ACTION_SND_SYN_ACK;
        free(G_packet);
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
            goto close;
            break;
          }

          G_retries++;
          goto open;
        }
      }
      break;

    /* ESTABLISHED */
    case RDT_STATE_ESTABLISHED:
      switch (input) {
        /* SEND */
        send:
        case RDT_INPUT_SEND:
        {
          G_packet = createPacket(DATA, G_seq_no, G_buf);
          sendRdtPacket(G_socket, G_packet, sizeof(RdtHeader_t) + ntohs(G_packet->header.size));
          if (setITIMER(0, RDT_TIMEOUT_200MS) != 0) {
            perror("Couldn't set timeout");
          }

          G_state = RDT_STATE_DATA_SENT;
          G_seq_no += (uint32_t) ntohs(G_packet->header.size);
          output = RDT_ACTION_SND_DATA;
          free(G_packet);
          break;
        }
        case RDT_EVENT_RCV_DATA: {
          uint32_t seq = G_seq_no;

//          if (G_checksum_match) {
//            seq += received->header.size;
//            G_seq_no = seq;
//          } else {
//
//          }

          if (G_buf == NULL) {
            G_buf_size = received->header.size;
            G_buf = (uint8_t*) calloc(1, G_buf_size);
            memcpy(G_buf, &(received->data), G_buf_size);
          } else {
            G_buf = (uint8_t*) realloc(G_buf, G_buf_size + received->header.size);
            memcpy(G_buf+G_buf_size, &(received->data), received->header.size);
            G_buf_size = G_buf_size + received->header.size;
          }

          seq += received->header.size;
          G_seq_no = seq;

          RdtPacket_t* packet = createPacket(DATA_ACK, seq, NULL);
          sendRdtPacket(G_socket, packet, sizeof(RdtHeader_t));

          G_state = RDT_STATE_ESTABLISHED;
          output = RDT_ACTION_SND_ACK;
          free(packet);
          break;
        }
        close:
        case RDT_INPUT_CLOSE: {
          G_packet = createPacket(FIN, G_seq_no, NULL);

          sendRdtPacket(G_socket, G_packet, sizeof(RdtHeader_t));
          G_retries = 0;
          G_state = RDT_STATE_FIN_SENT;
          output = RDT_ACTION_SND_FIN;
          free(G_packet);
          break;
        }
        case RDT_EVENT_RCV_FIN: {
          G_packet = createPacket(FIN_ACK, received->header.sequence, NULL);

          sendRdtPacket(G_socket, G_packet, sizeof(RdtHeader_t));
          G_state = RDT_STATE_CLOSED;
          output = RDT_ACTION_SND_FIN_ACK;
          free(G_packet);
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
        case RDT_INPUT_CLOSE: {
          G_packet = createPacket(FIN, G_seq_no, NULL);
          r = sendRdtPacket(G_socket, G_packet, sizeof(RdtHeader_t));
          G_retries = 0;
          G_state = RDT_STATE_FIN_SENT;
          output = RDT_ACTION_SND_FIN;
          free(G_packet);
          break;
        }
        case RDT_EVENT_TIMEOUT_2MSL: {
          if (G_retries < 10) {
            G_retries++;
            G_seq_no -= RDT_MAX_SIZE;
            goto send;
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
    if (G_seq_no % 1000 == 0) {
      printf("Progress: %d / %d\n", G_seq_no, G_buf_size);
    }
//  printf("new_state=%-12s output=%-12s\n", fsm_strings[G_state], fsm_strings[output]);
}

/**
 * Converts the value of the type field in RDT header to an RDT EVENT.
 * These EVENTS are used by the FSM.
 * @param type The RdtPacketType_t value.
 * @return int, the RDT EVENT.
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
/* OTHER END */