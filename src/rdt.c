// Copyright 2022 190010906
//
#include <arpa/inet.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "checksum/checksum.h"
#include "rdt.h"
#include "rto/rto.h"
#include "sigalrm/sigalrm.h"
#include "sigio/sigio.h"
#include "UdpSocket/UdpSocket.h"

/* GLOBAL VARIABLES START */
RdtSocket_t*      G_socket;                     // Global socket for connections.
RdtPacket_t*      received;                     // Received packet. Set by SIGIO handler.
RdtPacket_t*      G_packet;                     // Outbound packet.

struct timespec   G_timestamp;                  // Timestamp used for calculating RTT for variable retransmission.

uint32_t          G_rtt;                        // RTT for last segment in microseconds (us).
uint8_t*          G_buf;                        // Data buffer for sending or receiving.
uint32_t          G_buf_size;                   // Size of buf.
uint16_t          G_prev_size;                  // Size of previous packet sent.
bool              G_checksum_match;             // Flag for packet checksum match.

int               G_retries = 0;                // Global retries counter.
int               G_state = RDT_STATE_CLOSED;   // Global FSM state.
uint32_t          G_seq_no = 0;                 // Sequence number.
bool              G_debug = true;              // Debug output flag.
/* GLOBAL VARIABLES END */


#define DEBUG(fmt, ...) if (G_debug) printf(fmt, __VA_ARGS__)

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
  G_buf = (uint8_t*) buf;
  G_buf_size = n;

  printf("Sending %d bytes...\n", n);
  rdtOpen(socket);
  fsm(RDT_INPUT_SEND);

  // TODO: Check that the buffer has been completely sent
  while(G_state != RDT_STATE_ESTABLISHED) {
    (void) pause(); // Wait for signal
  }

  rdtClose(G_socket);
  printf("Done!\n");
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

  printf("Listening on port %d...\n", socket->local->addr.sin_port);
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
  int size = sizeof(RdtPacket_t);

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
  RdtPacket_t* packet = calloc(1, size);
  memcpy(packet, buffer.bytes, r);

  /* Calculate expected checksum and compare */
  uint16_t checksum = packet->header.checksum;
  packet->header.checksum = 0;
  uint16_t expected = ipv4_header_checksum(packet, r);

  G_checksum_match = (expected == checksum);

  /* Convert header fields to host byteorder */
  packet->header.sequence = ntohl(packet->header.sequence);
  packet->header.size = ntohs(packet->header.size);
  packet->header.type = ntohs(packet->header.type);
  packet->header.checksum = checksum;

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
int sendRdtPacket(const RdtSocket_t* socket, RdtPacket_t* packet, const uint16_t n) {
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
    fsm(RDT_EVENT_RTO);

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
  DEBUG("fsm: old_state=%-12s input=%-12s seq=%u/%-8u ", fsm_strings[G_state], fsm_strings[input], G_seq_no, G_buf_size);
  int r, error;
  int output = 0;
  uint32_t rto;

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
        case RDT_EVENT_RTO: {
          G_state = RDT_STATE_CLOSED;

          if (G_retries >= 10) {
            goto close;
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
          /* Create packet */
          G_packet = createPacket(DATA, G_seq_no, G_buf);

          /* Calculate RTO based on previous RTT, or use default value of 1s. */
          uint32_t curr_rto = T_rto;
          if (G_seq_no == 0) {
            curr_rto = MIN_RTO;
          }

          /* Start RTT timer. */
          if (clock_gettime(CLOCK_REALTIME, &G_timestamp) != 0) {
            perror("Couldn't start RTT timer.");
          }

          /* Send packet and set ITIMER for RTO */
          printf("rto: %u\n", curr_rto);
          printf("sec: %u\n", RTO_TO_SEC(curr_rto));
          printf("usec: %u\n", RTO_TO_USEC(curr_rto));
          sendRdtPacket(G_socket, G_packet, sizeof(RdtHeader_t) + ntohs(G_packet->header.size));
          if (setITIMER(RTO_TO_SEC(curr_rto), RTO_TO_USEC(curr_rto)) != 0) {
            perror("Couldn't set RTO");
          }

          /* Update state, sequence number, output (for fsm debug), etc. */
          G_prev_size = ntohs(G_packet->header.size);
          G_state = RDT_STATE_DATA_SENT;
          G_seq_no += (uint32_t) ntohs(G_packet->header.size);
          output = RDT_ACTION_SND_DATA;
          free(G_packet);
          break;
        }

        /* RECEIVE DATA */
        case RDT_EVENT_RCV_DATA: {
          if (!G_checksum_match) {
            RdtPacket_t* packet = createPacket(DATA_ACK, G_seq_no, NULL);
            sendRdtPacket(G_socket, packet, sizeof(RdtHeader_t));
            output = RDT_ACTION_SND_ACK;
            free(packet);
            break;
          }

          /* Discard packet if the sequence number is lower than expected */
          if (received->header.sequence < G_seq_no) {
            G_seq_no = received->header.sequence;
          }

          /* ACK the expected sequence number if received sequence number is greater
           * than expected. This means a DATA packet has likely been dropped */
          if (received->header.sequence > G_seq_no) {
            RdtPacket_t* packet = createPacket(DATA_ACK, G_seq_no, NULL);
            sendRdtPacket(G_socket, packet, sizeof(RdtHeader_t));
            output = RDT_ACTION_SND_ACK;
            free(packet);
            break;
          }

          /* Sequence number is expected, so can read the data into our buffer */
          if (G_buf == NULL) {
            G_buf_size = received->header.size;
            G_buf = (uint8_t*) calloc(1, G_buf_size);
            memcpy(G_buf, &(received->data), G_buf_size);
          } else {
            if (G_buf_size < G_seq_no + received->header.size) {
              G_buf = (uint8_t*) realloc(G_buf, G_buf_size * 2);
              G_buf_size = G_buf_size * 2;
            }

            memcpy(G_buf+G_seq_no, &(received->data), received->header.size);
          }

          G_seq_no += received->header.size;
          RdtPacket_t* packet = createPacket(DATA_ACK, G_seq_no, NULL);
          sendRdtPacket(G_socket, packet, sizeof(RdtHeader_t));

          G_state = RDT_STATE_ESTABLISHED;
          output = RDT_ACTION_SND_ACK;
          free(packet);
          break;
        }

        /* CLOSE INPUT */
        close:
        case RDT_INPUT_CLOSE: {
          G_packet = createPacket(FIN, G_seq_no, NULL);
          sendRdtPacket(G_socket, G_packet, sizeof(RdtHeader_t));
          if (setITIMER(0, RDT_TIMEOUT_200MS) != 0) {
            perror("Couldn't set timeout");
          }

          G_retries = 0;
          G_state = RDT_STATE_FIN_SENT;
          output = RDT_ACTION_SND_FIN;
          free(G_packet);
          break;
        }

        /* RECEIVE FIN */
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

        /* RECEIVE ACK */
        case RDT_EVENT_RCV_ACK: {
          /* Calculate the RTT in microseconds */
          G_rtt = calculateRTT(&G_timestamp);

          /* Calculate next RTO */
          calculateRTO(G_rtt);

          /* If the whole buffer hasn't been sent, send the next packet. */
          if (G_seq_no < G_buf_size) {
            if (received->header.sequence < G_seq_no) {
              G_seq_no = received->header.sequence;
            }
            G_retries = 0;
            goto send;
          }

          /* If whole buffer has been sent, return to the established state. */
          G_state = RDT_STATE_ESTABLISHED;
          break;
        }

        /* CLOSE INPUT */
        case RDT_INPUT_CLOSE: {
          goto close;
        }

        /* RTO */
        case RDT_EVENT_RTO: {
          if (G_retries < RDT_MAX_RETRIES) {
            G_retries++;  // Increment retries counter
            G_seq_no -= G_prev_size;  // Reduce sequence number to last ACK'd.
            T_rto = T_rto * 2 > MAX_RTO ? MAX_RTO : T_rto * 2;  // Double RTO
            goto send;
          }

          G_state = RDT_STATE_CLOSED;
          break;
        }

      }
      break;

    /* FIN_SENT */
    case RDT_STATE_FIN_SENT:
      switch(input) {
        case RDT_EVENT_RCV_FIN_ACK: {
          G_state = RDT_STATE_CLOSED;
          break;
        }
        case RDT_EVENT_RTO: {
          if (G_retries < 10) {
            G_retries++;
            goto close;
          }
          break;
        }
        default: {
          printf("Seq no %u\n", received->header.sequence);
        }
      }
      break;


    default:
      G_state = RDT_INVALID;
      printf("\n");
      break;
  }

  DEBUG("new_state=%-12s output=%-12s \n", fsm_strings[G_state], fsm_strings[output]);
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