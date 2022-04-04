// Copyright 2022 190010906
//
#include <arpa/inet.h>
#include <errno.h>
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

uint32_t          G_seq_init;                   // Initial sequence number.
uint32_t          G_seq_no;                     // Current sequence number.
uint32_t          G_rtt;                        // RTT for last segment in microseconds (us).
uint8_t*          G_buf;                        // Data buffer for sending or receiving.
uint32_t          G_buf_size;                   // Size of buf.
uint16_t          G_prev_size;                  // Size of previous packet sent.
bool              G_checksum_match;             // Flag for packet checksum match.

int               G_errors  = 0;                // Error counter. Will cause transmission to stop if too many errors encountered.
int               G_retries = 0;                // Global retries counter.
int               G_state   = RDT_STATE_CLOSED; // Global FSM state.
bool              G_debug   = false;            // Debug output flag.
bool              G_sender  = false;            // Whether in send or receive mode.
/* GLOBAL VARIABLES END */


#define DEBUG(fmt, ...) if (G_debug) printf(fmt, __VA_ARGS__)

void fsm(int input);
void rdtOpen(RdtSocket_t* socket);
void rdtClose();
void handleSIGALRM(int sig);
void handleSIGIO(int sig);
void printProgress(int input);
int rdtTypeToRdtEvent(RDTPacketType_t type);

/* API START */
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
    errno = ENOTCONN;
    perror("Couldn't setup local UDP socket");
    error = 1;
  }

  /* open local UDP socket */
  if (openUdp(socket->local) < 0) {
    errno = ENOTCONN;
    perror("Couldn't open UDP socket");
    error = 1;
  }

  if (hostname == NULL) {
      printf("Opening socket on port %d...\n", port);
  } else {
    printf("Opening socket to %s:%d...\n", hostname, port);
  }

  /* setup remote UDP socket */
  socket->remote = setupUdpSocket_t(hostname, port);
  if (socket->remote == (UdpSocket_t *) 0) {
    errno = ENOTCONN;
    perror("Couldn't setup remote UDP socket");
    error = 1;
  }

  if (error) {
    free(socket);
    socket = (RdtSocket_t *) -1;
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
  G_sender = true;

  /* Seed srand */
  srandom(time(NULL));

  printf("Connecting to remote host...\n");
  rdtOpen(socket);

  if (G_state == RDT_STATE_CLOSED) {
    printf("Unable to connect to remote host. Aborting!\n");
    return;
  }

  printf("Sending %d bytes...\n", n);
  fsm(RDT_INPUT_SEND);

  while(G_state != RDT_STATE_ESTABLISHED && G_state != RDT_STATE_CLOSED) {
    (void) pause(); // Wait for signal
  }
  printf("Finished!\n");

  rdtClose();
  printf("Bye!\n");
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

  printf("Listening on port %d...\n", ntohs(socket->local->addr.sin_port));

  while(G_state != RDT_STATE_CLOSED) {
    (void) pause(); // Wait for signal
  }
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
/* API END */


/* SOCKETS START */
/**
 * Sets remote socket to hostname supplied. Allows accepting SYN initially, then setting remote hostname to whoever we
 * received SYN from
 * @param hostname
 * @return int 0 if success, -1 if failure.
 */
int setRemoteSocket(char* hostname) {
  G_socket->remote = setupUdpSocket_t(hostname, ntohs(G_socket->receive.addr.sin_port));
  if (G_socket->remote == (UdpSocket_t *) 0) {
    errno = ENOTCONN;
    perror("Couldn't setup remote UDP socket\n");
    return -1;
  }

  return 0;
}


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

  while(G_state != RDT_STATE_ESTABLISHED  && G_state != RDT_STATE_CLOSED) {
    (void) pause(); // Wait for signal
  }
}

/**
 * Closes an RDT socket.
 * @param socket The socket to close.
 */
void rdtClose() {
  fsm(RDT_INPUT_CLOSE);

  while(G_state != RDT_STATE_CLOSED) {
    (void) pause();
  }
}
/* CONNECTION MANAGEMENT CLOSE */


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

/**
 * Sends an RdtPacket_t over the specified RdtSocket.
 * @param socket The socket to send the packet over.
 * @param packet Pointer to the packet to send.
 * @param n The size of 'packet' (header + data).
 * @return Number of bytes sent.
 */
int sendRdtPacket(const RdtSocket_t* socket, RdtPacket_t* packet, const uint16_t n) {
  UdpBuffer_t buffer;
  uint8_t bytes[n];

  memcpy(bytes, packet, n);
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
    uint32_t diff = G_buf_size - (G_seq_no - G_seq_init);

    if (diff > RDT_MAX_SIZE) {
      n = RDT_MAX_SIZE;
    } else {
      n = diff;
    }

    memcpy(&packet->data, data + (G_seq_no - G_seq_init), n);
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
  DEBUG("fsm: old_state=%-12s input=%-12s seq=%u/%-8u ", fsm_strings[G_state], fsm_strings[input], (G_seq_no - G_seq_init), G_buf_size);
  int r, size;
  int output = 0;

  /* If RST is ever received as input, close the connection immediately. */
  if (input == RDT_EVENT_RCV_RST) {
    G_state = RDT_STATE_CLOSED;
    return;
  }

  switch (G_state) {

    /* CLOSED */
    case RDT_STATE_CLOSED:
      switch (input) {

        /* ACTIVE OPEN */
        open:
        case RDT_INPUT_ACTIVE_OPEN: {
          /* Set G_seq_init and G_seq_no to a random starter value,
           * to minimise "old" packet ambiguity/predictability */
          G_seq_init = (uint32_t) random();
          G_seq_no = G_seq_init;

          /* If this is the first attempt, set RTO to 200ms */
          if (T_rto == 0) {
            T_rto = HANDSHAKE_RTO;
          }

          /* Create and send SYN packet */
          G_packet = createPacket(SYN, G_seq_no, NULL);
          size = sizeof(RdtHeader_t);
          if (sendRdtPacket(G_socket, G_packet, size) != size) {
            errno = ECOMM;
            perror("Error sending RDT packet.");
            if (G_errors++ > RDT_MAX_ERROR) exit(errno);
          }

          /* Set ITIMER to RTO, starting from 200ms for handshake */
          if (setITIMER(RTO_TO_SEC(T_rto), RTO_TO_USEC(T_rto)) != 0) {
            perror("Couldn't set timeout");
          }

          /* Update state and output flag */
          G_state = RDT_STATE_SYN_SENT;
          output = RDT_ACTION_SND_SYN;
          free(G_packet);
          break;
        }

        /* PASSIVE OPEN */
        case RDT_INPUT_PASSIVE_OPEN:
          G_state = RDT_STATE_LISTEN;
          break;
      }
      break;

    /* LISTENING */
    case RDT_STATE_LISTEN:
      switch (input) {

        /* RCV SYN */
        syn:
        case RDT_EVENT_RCV_SYN: {
          /* Set sequence number to received sequence number */
          G_seq_init = received->header.sequence;
          G_seq_no = G_seq_init;

          /* Set remote socket to host that we've received SYN from */
          printf("Receiving bytes from %s...\n", inet_ntoa(G_socket->receive.addr.sin_addr));
          if (setRemoteSocket(inet_ntoa(G_socket->receive.addr.sin_addr)) < 0) {
            errno = ECONNABORTED;
            perror("Couldn't set up remote socket. Aborting!");
            exit(-1);
          }

          /* Create and send SYN ACK packet */
          G_packet = createPacket(SYN_ACK, G_seq_init, NULL);
          size = sizeof(RdtHeader_t);
          if (sendRdtPacket(G_socket, G_packet, size) != size) {
            errno = ECOMM;
            perror("Error sending RDT packet.");
            if (G_errors++ > RDT_MAX_ERROR) exit(errno);
          }

          /* Update state and set output flag */
          G_state = RDT_STATE_ESTABLISHED;
          output = RDT_ACTION_SND_SYN_ACK;
          free(G_packet);
          break;
        }

        /* DEFAULT / ALL OTHER PACKET TYPES */
        default: {
          /* Create and send RST */
          G_packet = createPacket(RST, 0, NULL);
          size = sizeof(RdtHeader_t);
          if (sendRdtPacket(G_socket, G_packet, size) != size) {
            errno = ECOMM;
            perror("Error sending RDT packet.");
            if (G_errors++ > RDT_MAX_ERROR) exit(errno);
          }

          /* Remain in LISTEN state */
          G_state = RDT_STATE_CLOSED;
          output = RDT_ACTION_SND_RST;
          free(G_packet);
        }

      }
      break;

    /* SYN_SENT */
    case RDT_STATE_SYN_SENT:
      switch(input) {

        /* RCV SYN_ACK */
        case RDT_EVENT_RCV_SYN_ACK: {
          G_state = RDT_STATE_ESTABLISHED;
          T_rto = 0;
          break;
        }

        /* TIMEOUT */
        case RDT_EVENT_RTO: {
          G_state = RDT_STATE_CLOSED;

          if (G_retries < RDT_MAX_RETRIES) {
            G_retries++;
            T_rto = T_rto * 2 > MAX_RTO ? MAX_RTO : T_rto * 2;  // Double RTO
            goto open;
          }

          G_packet = createPacket(RST, 0, NULL);
          size = sizeof(RdtHeader_t);
          if (sendRdtPacket(G_socket, G_packet, size) != size) {
            errno = ECOMM;
            perror("Error sending RDT packet.");
            if (G_errors++ > RDT_MAX_ERROR) exit(errno);
          }

          output = RDT_ACTION_SND_RST;
          free(G_packet);
          G_state = RDT_STATE_CLOSED;
          break;
        }

        /* DEFAULT / ALL OTHER PACKET TYPES */
        default: {
          /* Create and send RST */
          G_packet = createPacket(RST, 0, NULL);
          size = sizeof(RdtHeader_t);
          if (sendRdtPacket(G_socket, G_packet, size) != size) {
            errno = ECOMM;
            perror("Error sending RDT packet.");
            if (G_errors++ > RDT_MAX_ERROR) exit(errno);
          }

          /* Remain in SYN_SENT state */
          G_state = RDT_STATE_SYN_SENT;
          output = RDT_ACTION_SND_RST;
          free(G_packet);
          break;
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
          if (G_seq_no == G_seq_init) {
            curr_rto = MIN_RTO;
          }

          /* Start RTT timer. */
          if (clock_gettime(CLOCK_REALTIME, &G_timestamp) != 0) {
            perror("Couldn't start RTT timer.");
          }

          /* Send packet*/
          size = sizeof(RdtHeader_t) + ntohs(G_packet->header.size);
          if (sendRdtPacket(G_socket, G_packet, size) != size) {
            errno = ECOMM;
            perror("Error sending RDT packet.");
            if (G_errors++ > RDT_MAX_ERROR) exit(errno);
          }

          /* Set ITIMER for RTO. */
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
            /* Create and send ACK packet */
            G_packet = createPacket(ACK, G_seq_no, NULL);
            size = sizeof(RdtHeader_t);
            if (sendRdtPacket(G_socket, G_packet, size) != size) {
              errno = ECOMM;
              perror("Error sending RDT packet.");
              if (G_errors++ > RDT_MAX_ERROR) exit(errno);
            }

            output = RDT_ACTION_SND_ACK;
            free(G_packet);
            break;
          }

          /* Discard packet if the sequence number is lower than expected */
          if (received->header.sequence < G_seq_no) {
            G_seq_no = received->header.sequence;
          }

          /* ACK the expected sequence number if received sequence number is greater
           * than expected. This means a DATA packet has likely been dropped */
          if (received->header.sequence > G_seq_no) {
            /* Create and send packet */
            G_packet = createPacket(ACK, G_seq_no, NULL);
            size = sizeof(RdtHeader_t);
            if (sendRdtPacket(G_socket, G_packet, size) != size) {
              errno = ECOMM;
              perror("Error sending RDT packet.");
              if (G_errors++ > RDT_MAX_ERROR) exit(errno);
            }

            /* Set output flag */
            output = RDT_ACTION_SND_ACK;
            free(G_packet);
            break;
          }

          /* Sequence number is expected, so can read the data into our buffer */
          if (G_buf == NULL) {
            G_buf_size = received->header.size;
            G_buf = (uint8_t*) calloc(1, G_buf_size);
            memcpy(G_buf, &(received->data), G_buf_size);
          } else {
            if (G_buf_size < (G_seq_no - G_seq_init) + received->header.size) {
              G_buf = (uint8_t*) realloc(G_buf, G_buf_size * 2);
              G_buf_size = G_buf_size * 2;
            }

            memcpy(G_buf + (G_seq_no - G_seq_init), &(received->data), received->header.size);
          }

          G_seq_no += received->header.size;
          G_packet = createPacket(ACK, G_seq_no, NULL);
          size = sizeof(RdtHeader_t);
          if (sendRdtPacket(G_socket, G_packet, size) != size) {
            errno = ECOMM;
            perror("Error sending RDT packet.");
            if (G_errors++ > RDT_MAX_ERROR) exit(errno);
          }

          G_state = RDT_STATE_ESTABLISHED;
          output = RDT_ACTION_SND_ACK;
          free(G_packet);
          break;
        }

        /* CLOSE INPUT */
        close:
        case RDT_INPUT_CLOSE: {
          /* If this is the first attempt, set RTO to 200ms */
          if (T_rto == 0) {
            T_rto = HANDSHAKE_RTO;
          }

          G_packet = createPacket(FIN, G_seq_no, NULL);
          size = sizeof(RdtHeader_t);
          if (sendRdtPacket(G_socket, G_packet, size) != size) {
            errno = ECOMM;
            perror("Error sending RDT packet.");
            if (G_errors++ > RDT_MAX_ERROR) exit(errno);
          }

          /* Set ITIMER for RTO */
          if (setITIMER(RTO_TO_SEC(T_rto), RTO_TO_USEC(T_rto)) != 0) {
            perror("Couldn't set timeout");
          }

          G_state = RDT_STATE_FIN_SENT;
          output = RDT_ACTION_SND_FIN;
          free(G_packet);
          break;
        }

        /* RECEIVE FIN */
        case RDT_EVENT_RCV_FIN: {
          /* Create and send FIN ACK packet */
          G_packet = createPacket(FIN_ACK, received->header.sequence, NULL);
          size = sizeof(RdtHeader_t);
          if (sendRdtPacket(G_socket, G_packet, size) != size) {
            errno = ECOMM;
            perror("Error sending RDT packet.");
            if (G_errors++ > RDT_MAX_ERROR) exit(errno);
          }

          G_state = RDT_STATE_CLOSED;
          output = RDT_ACTION_SND_FIN_ACK;
          free(G_packet);
          setRemoteSocket(NULL);
          printf("Done!\n");
          break;
        }

        /* RECEIVE SYN */
        case RDT_EVENT_RCV_SYN: {
          goto syn;
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
          if ((G_seq_no - G_seq_init) < G_buf_size) {
            if (received->header.sequence < G_seq_no) {
              G_seq_no = received->header.sequence;
            }

            G_retries = 0;
            goto send;
          }

          /* If whole buffer has been sent, return to the established state. */
          G_state = RDT_STATE_ESTABLISHED;
          G_retries = 0;

          /* Set RTO to 0 for termination */
          T_rto = 0;
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

        /* RECEIVE FIN ACK */
        case RDT_EVENT_RCV_FIN_ACK: {
          G_state = RDT_STATE_CLOSED;
          printf("Connection terminated gracefully!\n");
          break;
        }

        /* RTO */
        case RDT_EVENT_RTO: {
          if (G_retries < RDT_MAX_RETRIES) {
            G_retries++;
            T_rto = T_rto * 2 > MAX_RTO ? MAX_RTO : T_rto * 2;  // Double RTO
            goto close;
          }

          /* Create and send packet */
          G_packet = createPacket(RST, 0, NULL);
          size = sizeof(RdtHeader_t);
          if (sendRdtPacket(G_socket, G_packet, size) != size) {
            errno = ECOMM;
            perror("Error sending RDT packet.");
            if (G_errors++ > RDT_MAX_ERROR) exit(errno);
          }

          /* Update state, etc */
          G_state = RDT_STATE_CLOSED;
          output = RDT_ACTION_SND_RST;
          free(G_packet);
          printf("Unable to terminate gracefully. Terminating abruptly!\n");
          break;
        }

        /* DEFAULT */
        default: {
          G_packet = createPacket(RST, 0, NULL);
          size = sizeof(RdtHeader_t);
          if (sendRdtPacket(G_socket, G_packet, size) != size) {
            errno = ECOMM;
            perror("Error sending RDT packet.");
            if (G_errors++ > RDT_MAX_ERROR) exit(errno);
          }

          G_state = RDT_STATE_CLOSED;
          output = RDT_ACTION_SND_RST;
          free(G_packet);
        }

      }
      break;

    default:
      /* Create and send RST packet */
      G_packet = createPacket(RST, 0, NULL);
      size = sizeof(RdtHeader_t);
      if (sendRdtPacket(G_socket, G_packet, size) != size) {
        errno = ECOMM;
        perror("Error sending RDT packet.");
        if (G_errors++ > RDT_MAX_ERROR) exit(errno);
      }

      /* Update state etc */
      G_state = RDT_STATE_CLOSED;
      output = RDT_ACTION_SND_RST;
      free(G_packet);
      G_state = RDT_INVALID;
  }

  DEBUG("new_state=%-12s output=%-12s \n", fsm_strings[G_state], fsm_strings[output]);
  printProgress(input);
}

/**
 * Converts the value of the type field in RDT header to an RDT EVENT.
 * These EVENTS are used by the FSM.
 * @param type The RdtPacketType_t value.
 * @return int, the RDT EVENT.
 */
int rdtTypeToRdtEvent(RDTPacketType_t type) {
  switch (type) {
    case SYN:         return RDT_EVENT_RCV_SYN;
    case SYN_ACK:     return RDT_EVENT_RCV_SYN_ACK;
    case DATA:        return RDT_EVENT_RCV_DATA;
    case ACK:         return RDT_EVENT_RCV_ACK;
    case FIN:         return RDT_EVENT_RCV_FIN;
    case FIN_ACK:     return RDT_EVENT_RCV_FIN_ACK;
    case RST:         return RDT_EVENT_RCV_RST;
    default:          return RDT_INVALID;
  }
}

/**
 * Prints progress for sender.
 */
void printProgress(int input) {
  if (G_sender && G_state == RDT_STATE_DATA_SENT && input == RDT_EVENT_RCV_ACK && !G_debug) {
    double progress = (double) (((double) (G_seq_no - G_seq_init) / (double) G_buf_size)) * 100.0;
    if (progress > 0.0) {
      printf("\b\b\b\b\b\b\b\b");
    }
    printf("%.1f%%", progress);
    fflush(stdout);
    if (progress == 100.0) {
      printf("\n");
    }
  }
}
/* OTHER END */