//
// Created by CJ Williams on 17/03/2022.
//
#include "RDT.h"

#define RDT_INVALID               ((int)  0)

#define RDT_INPUT_ACTIVE_OPEN     ((int) 1)
#define RDT_INPUT_PASSIVE_OPEN    ((int) 2)
#define RDT_INPUT_CLOSE           ((int) 3)
#define RDT_INPUT_SEND            ((int) 4)

#define RDT_ACTION_SND_SYN        ((int)  5)
#define RDT_ACTION_SND_SYN_ACK    ((int)  6)
#define RDT_ACTION_SND_DATA       ((int)  7)
#define RDT_ACTION_SND_ACK        ((int)  8)
#define RDT_ACTION_SND_FIN        ((int)  9)
#define RDT_ACTION_SND_FIN_ACK    ((int) 10)

#define TCP_EVENT_RCV_SYN         ((int) 11)
#define TCP_EVENT_RCV_SYN_ACK     ((int) 12)
#define TCP_EVENT_RCV_DATA        ((int) 13)
#define TCP_EVENT_RCV_ACK         ((int) 14)
#define TCP_EVENT_RCV_FIN         ((int) 15)
#define TCP_EVENT_RCV_FIN_ACK     ((int) 16)
#define TCP_EVENT_TIMEOUT_2MSL    ((int) 17)

#define TCP_STATE_CLOSED          ((int) 18)
#define TCP_STATE_LISTEN          ((int) 19)
#define TCP_STATE_SYN_SENT        ((int) 20)
#define TCP_STATE_SYN_RCVD        ((int) 21)

#define TCP_STATE_ESTABLISHED     ((int) 22)

#define TCP_STATE_FIN_SENT        ((int) 23)
#define TCP_STATE_FIN_RCV         ((int) 24)


const char* RDT_fsm_strings[] = {
        "---",
        "active OPEN",
        "passive OPEN",
        "CLOSE",
        "SEND",
        "snd SYN",
        "snd SYN_ACK",
        "snd DATA",
        "snd ACK",
        "snd FIN",
        "snd FIN_ACK",
        "rcv SYN",
        "rcv SYN_ACK",
        "rcv DATA",
        "rcv ACK",
        "rcv FIN",
        "rcv FIN_ACK",
        "timeout=2msl",
        "CLOSE",
        "LISTEN",
        "SYN_SENT",
        "SYN_RCVD",
        "ESTABLISHED",
        "FIN_SENT",
        "FIN_RCV",
}

