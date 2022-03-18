/*
  TCP FSM emulation for connection management.
  Example of simple implementation of FSM.
  saleem, Jan 2022.
*/

#include <stdio.h>
#include <unistd.h>

/*
  Simple implementation of a FSM for TCP. Does not include RST.

  Based on the TCP FSM diagram on page 23 of RFC793(S), included below.

  A switch statement is used for TCP states, one case statement per state.

  In each case (for each state), another switch is used with a case for
  each possible state transition.

  There are other ways of implementing an FSM - this is just a simple
  example for the purposes of this demo.

  This demo requires the user to decide what happens next by chosing
  from a menu of options. It is, effectively, a text-based, menu-driven
  walkthrough of the TCP FSM.
*/

/*
** From RFC793(S) at https://datatracker.ietf.org/doc/html/rfc793

  The state diagram in figure 6 illustrates only state changes, together
  with the causing events and resulting actions, but addresses neither
  error conditions nor actions which are not connected with state
  changes.  In a later section, more detail is offered with respect to
  the reaction of the TCP to events.

  NOTE BENE:  this diagram is only a summary and must not be taken as
  the total specification.

September 1981
                                           Transmission Control Protocol
                                                Functional Specification




                              +---------+ ---------\      active OPEN
                              |  CLOSED |            \    -----------
                              +---------+<---------\   \   create TCB
                                |     ^              \   \  snd SYN
                   passive OPEN |     |   CLOSE        \   \
                   ------------ |     | ----------       \   \
                    create TCB  |     | delete TCB         \   \
                                V     |                      \   \
                              +---------+            CLOSE    |    \
                              |  LISTEN |          ---------- |     |
                              +---------+          delete TCB |     |
                   rcv SYN      |     |     SEND              |     |
                  -----------   |     |    -------            |     V
 +---------+      snd SYN,ACK  /       \   snd SYN          +---------+
 |         |<-----------------           ------------------>|         |
 |   SYN   |                    rcv SYN                     |   SYN   |
 |   RCVD  |<-----------------------------------------------|   SENT  |
 |         |                    snd ACK                     |         |
 |         |------------------           -------------------|         |
 +---------+   rcv ACK of SYN  \       /  rcv SYN,ACK       +---------+
   |           --------------   |     |   -----------
   |                  x         |     |     snd ACK
   |                            V     V
   |  CLOSE                   +---------+
   | -------                  |  ESTAB  |
   | snd FIN                  +---------+
   |                   CLOSE    |     |    rcv FIN
   V                  -------   |     |    -------
 +---------+          snd FIN  /       \   snd ACK          +---------+
 |  FIN    |<-----------------           ------------------>|  CLOSE  |
 | WAIT-1  |------------------                              |   WAIT  |
 +---------+          rcv FIN  \                            +---------+
   | rcv ACK of FIN   -------   |                            CLOSE  |
   | --------------   snd ACK   |                           ------- |
   V        x                   V                           snd FIN V
 +---------+                  +---------+                   +---------+
 |FINWAIT-2|                  | CLOSING |                   | LAST-ACK|
 +---------+                  +---------+                   +---------+
   |                rcv ACK of FIN |                 rcv ACK of FIN |
   |  rcv FIN       -------------- |    Timeout=2MSL -------------- |
   |  -------              x       V    ------------        x       V
    \ snd ACK                 +---------+delete TCB         +---------+
     ------------------------>|TIME WAIT|------------------>| CLOSED  |
                              +---------+                   +---------+

                      TCP Connection State Diagram
                               Figure 6.


                                                               [Page 23]

**
*/

// The "traditional" approach is to use integers and #defines as shown.

// //// ////
// TCP_INVALID is not part of RFC793(S), just for this FSM emulation.
#define TCP_INVALID               ((int)  0)

// //// ////
// TCP inputs from TCP user, e.g. via API.
#define TCP_INPUT_ACTIVE_OPEN     ((int)  1)
#define TCP_INPUT_PASSIVE_OPEN    ((int)  2)
#define TCP_INPUT_CLOSE           ((int)  3)
#define TCP_INPUT_SEND            ((int)  4)

// //// ////
// TCP actions
#define TCP_ACTION_SND_SYN        ((int)  5)
#define TCP_ACTION_SND_ACK        ((int)  6)
#define TCP_ACTION_SND_SYN_ACK    ((int)  7)
#define TCP_ACTION_SND_FIN        ((int)  8)
#define TCP_ACTION_CREATE_TCB     ((int)  9)
#define TCP_ACTION_DELETE_TCB     ((int) 10)

// //// ////
// TCP events
#define TCP_EVENT_RCV_SYN         ((int) 11)
#define TCP_EVENT_RCV_ACK_OF_SYN  ((int) 12)
#define TCP_EVENT_RCV_SYN_ACK     ((int) 13)
#define TCP_EVENT_RCV_FIN         ((int) 14)
#define TCP_EVENT_RCV_ACK_OF_FIN  ((int) 15)
#define TCP_EVENT_TIMEOUT_2MSL    ((int) 16)

// //// ////
// TCP states
#define TCP_STATE_CLOSED          ((int) 17)
#define TCP_STATE_LISTEN          ((int) 18)
#define TCP_STATE_SYN_SENT        ((int) 19)
#define TCP_STATE_SYN_RCVD        ((int) 20)

#define TCP_STATE_ESTABLISHED     ((int) 21)

#define TCP_STATE_FINWAIT_1       ((int) 22)
#define TCP_STATE_FINWAIT_2       ((int) 23)
#define TCP_STATE_CLOSING         ((int) 24)
#define TCP_STATE_TIME_WAIT       ((int) 25)
#define TCP_STATE_CLOSE_WAIT      ((int) 26)
#define TCP_STATE_LAST_ACK        ((int) 27)

// This array of strings is indexed by integer values in #define
// values above, so the order of the list is important!
const char *TCP_fsm_strings_G[] = {
        "--",
        "active OPEN",
        "passive OPEN",
        "CLOSE",
        "SEND",
        "snd SYN",
        "snd ACK",
        "snd SYN,ACK",
        "snd FIN",
        "create TCB",
        "delete TCB",
        "rcv SYN",
        "rcv ACK of SYN",
        "rcv SYN,ACK",
        "rcv FIN",
        "rcv ACK of FIN",
        "Timeout=2MSL",
        "CLOSED",
        "LISTEN",
        "SYN_SENT",
        "SYN_RCVD",
        "ESTBALISHED",
        "FINWAIT_1",
        "FINWAIT_2",
        "CLOSING",
        "TIME_WAIT",
        "CLOSE_WAIT",
        "LAST_ACK"
};

// Get and check input from terminal
int
checkInputsAndEvents(int tcp_state, int *inputs, int *events)
{
  int w, what;

  printf("\n** Current state: %s\n", TCP_fsm_strings_G[tcp_state]);

  w = what = TCP_INVALID;
  while (what == TCP_INVALID) {

    // Current possibilities for inputs or actions
    printf("   Possible inputs and events:\n");
    for (int *ip = inputs; *ip != TCP_INVALID; ++ip)
      printf("     %2d : %s\n", *ip, TCP_fsm_strings_G[*ip]);
    for (int *ep = events; *ep != TCP_INVALID; ++ep)
      printf("     %2d : %s\n", *ep, TCP_fsm_strings_G[*ep]);

    // Get input
    do {
      fflush(stdin);
      printf("   Enter a number from the list above: ");
    } while (scanf("%i", &w) != 1);

    // Check validity of input
    for (int *ip = inputs; *ip != TCP_INVALID; ++ip)
      if (w == *ip) { what = *ip; break; }
    if (what == TCP_INVALID)
      for (int *ep = events; *ep != TCP_INVALID; ++ep)
        if (w == *ep) { what = *ep; break; }
    if (what == TCP_INVALID)
      printf("\n** Please select a number from the list.\n");
  }

  return what;
}


int
main(void)
{
  int tcp_state;

  tcp_state = TCP_STATE_CLOSED;

  while (tcp_state != TCP_INVALID) {

    int tcp_new_state, // after the state change

    // The TCP FSM only ever has 2 inputs, events, or actions
    // possible in each state, so the following is convenient.
    inputs[3],     // possible input from TCP user
    events[3],     // possible network or timer events
    actions[3],    // actions to be taken during state change

    // Input for what to do next in the emulation.
    what;          // what to do next

    // initialise values for this iteration
    tcp_new_state = what = TCP_INVALID;
    inputs[0]  = inputs[1]  = inputs[2]  = TCP_INVALID;
    events[0]  = events[1]  = events[2]  = TCP_INVALID;
    actions[0] = actions[1] = actions[2] = TCP_INVALID;

#define CHECK_INPUTS_AND_EVENTS \
    do { \
      what = checkInputsAndEvents(tcp_state, inputs, events); \
    } while (what == TCP_INVALID)

    switch (tcp_state)
    {
      // CLOSED
      case TCP_STATE_CLOSED:
        inputs[0] = TCP_INPUT_ACTIVE_OPEN; // from TCP user
        inputs[1] = TCP_INPUT_PASSIVE_OPEN; // from TCP user
        CHECK_INPUTS_AND_EVENTS;

        switch (what) {
          case TCP_INPUT_ACTIVE_OPEN:
            actions[0] = TCP_ACTION_CREATE_TCB; // local TCP
            actions[1] = TCP_ACTION_SND_SYN; // local TCP
            tcp_new_state = TCP_STATE_SYN_SENT;
            break;

          case TCP_INPUT_PASSIVE_OPEN:
            actions[0] = TCP_ACTION_CREATE_TCB; // local TCP
            tcp_new_state = TCP_STATE_LISTEN;
            break;
        }
        break;

        // LISTEN
      case TCP_STATE_LISTEN:
        inputs[0] = TCP_INPUT_SEND; // from TCP user
        inputs[1] = TCP_INPUT_CLOSE; // from TCP user
        events[0] = TCP_EVENT_RCV_SYN; // from network
        CHECK_INPUTS_AND_EVENTS;

        switch (what) {
          case TCP_INPUT_SEND:
            actions[0] = TCP_ACTION_SND_SYN; // local TCP
            tcp_new_state = TCP_STATE_SYN_SENT;
            break;

          case TCP_INPUT_CLOSE:
            actions[0] = TCP_ACTION_DELETE_TCB; // local TCP
            actions[1] = TCP_INPUT_CLOSE; // local TCP
            tcp_new_state = TCP_STATE_CLOSED;
            break;

          case TCP_EVENT_RCV_SYN:
            actions[0] = TCP_ACTION_SND_SYN_ACK; // local TCP
            tcp_new_state = TCP_STATE_SYN_RCVD;
            break;
        }
        break;

        // SYN SENT
      case TCP_STATE_SYN_SENT:
        inputs[0] = TCP_INPUT_CLOSE; // from TCP user
        events[0] = TCP_EVENT_RCV_SYN; // from network
        events[1] = TCP_EVENT_RCV_SYN_ACK; // from network
        CHECK_INPUTS_AND_EVENTS;

        switch (what) {
          case TCP_INPUT_CLOSE:
            actions[0] = TCP_ACTION_DELETE_TCB; // local TCP
            actions[1] = TCP_INPUT_CLOSE; // local TCP
            tcp_new_state = TCP_STATE_CLOSED;
            break;

          case TCP_EVENT_RCV_SYN:
            actions[0] = TCP_ACTION_SND_ACK; // local TCP
            tcp_new_state = TCP_STATE_SYN_RCVD;
            break;

          case TCP_EVENT_RCV_SYN_ACK:
            actions[0] = TCP_ACTION_SND_ACK; // local TCP
            tcp_new_state = TCP_STATE_ESTABLISHED;
            break;
        }
        break;

        // SYN RCVD
      case TCP_STATE_SYN_RCVD:
        inputs[0] = TCP_INPUT_CLOSE; // from TCP user
        events[0] = TCP_EVENT_RCV_ACK_OF_SYN; // from network
        CHECK_INPUTS_AND_EVENTS;

        switch (what) {
          case TCP_INPUT_CLOSE:
            actions[0] = TCP_ACTION_SND_FIN; // local TCP
            actions[1] = TCP_INPUT_CLOSE; // local TCP
            tcp_new_state = TCP_STATE_FINWAIT_1;
            break;

          case TCP_EVENT_RCV_ACK_OF_SYN:
            tcp_new_state = TCP_STATE_ESTABLISHED;
            break;
        }
        break;

        // ESTABLISHED
      case TCP_STATE_ESTABLISHED:
        inputs[0] = TCP_INPUT_CLOSE; // from TCP user
        events[0] = TCP_EVENT_RCV_FIN; // from network
        CHECK_INPUTS_AND_EVENTS;

        switch (what) {
          case TCP_INPUT_CLOSE:
            actions[0] = TCP_ACTION_SND_FIN; // local TCP
            tcp_new_state = TCP_STATE_FINWAIT_1;
            break;

          case TCP_EVENT_RCV_FIN:
            actions[0] = TCP_ACTION_SND_ACK; // local TCP
            tcp_new_state = TCP_STATE_CLOSE_WAIT;
            break;
        }
        break;

        // FINWAIT-1
      case TCP_STATE_FINWAIT_1:
        events[0] = TCP_EVENT_RCV_FIN; // from network
        events[1] = TCP_EVENT_RCV_ACK_OF_FIN; // from network
        CHECK_INPUTS_AND_EVENTS;

        switch (what) {
          case TCP_EVENT_RCV_FIN:
            actions[0] = TCP_ACTION_SND_ACK; // local TCP
            tcp_new_state = TCP_STATE_CLOSING;
            break;

          case TCP_EVENT_RCV_ACK_OF_FIN:
            tcp_new_state = TCP_STATE_FINWAIT_2;
            break;
        }
        break;

        // FINWAIT-2
      case TCP_STATE_FINWAIT_2:
        events[0] = TCP_EVENT_RCV_FIN; // from network
        CHECK_INPUTS_AND_EVENTS;

        switch (what) {
          case TCP_EVENT_RCV_FIN:
            actions[0] = TCP_ACTION_SND_ACK; // local TCP
            tcp_new_state = TCP_STATE_CLOSING;
            break;
        }
        break;

        // CLOSING
      case TCP_STATE_CLOSING:
        events[0] = TCP_EVENT_RCV_ACK_OF_FIN; // from network
        CHECK_INPUTS_AND_EVENTS;

        switch (what) {
          case TCP_EVENT_RCV_ACK_OF_FIN:
            tcp_new_state = TCP_STATE_TIME_WAIT;
            break;
        }
        break;

        // TIME WAIT
      case TCP_STATE_TIME_WAIT:
        events[0] = TCP_EVENT_TIMEOUT_2MSL; // local timer
        CHECK_INPUTS_AND_EVENTS;

        switch (what) {
          case TCP_EVENT_TIMEOUT_2MSL:
            actions[0] = TCP_ACTION_DELETE_TCB; // local TCP
            tcp_new_state = TCP_STATE_CLOSED;
            break;
        }
        break;

        // CLOSE WAIT
      case TCP_STATE_CLOSE_WAIT:
        inputs[0] = TCP_INPUT_CLOSE; // local TCP
        CHECK_INPUTS_AND_EVENTS;

        switch (what) {
          case TCP_INPUT_CLOSE:
            actions[0] = TCP_ACTION_SND_FIN; // local TCP
            tcp_new_state = TCP_STATE_LAST_ACK;
            break;
        }
        break;

        // LAST ACK
      case TCP_STATE_LAST_ACK:
        events[0] = TCP_EVENT_RCV_ACK_OF_FIN; // from network
        CHECK_INPUTS_AND_EVENTS;

        switch (what) {
          case TCP_EVENT_RCV_ACK_OF_FIN:
            tcp_new_state = TCP_STATE_CLOSED;
            break;
        }
        break;

      default:
        // should not happen!
        printf("We have a problem! %d\n", tcp_state);
        tcp_new_state = TCP_INVALID;
        break;
    }

    printf("** New state: %s\n", TCP_fsm_strings_G[tcp_new_state]);
    printf("   Actions executed by TCP:\n");
    for(int *ap = actions; *ap != TCP_INVALID; ++ap)
      printf("     %d : %s\n", *ap, TCP_fsm_strings_G[*ap]);
    printf("\n");

    tcp_state = tcp_new_state;

    sleep(1); // a short pause

  } // while(tcp_state != TCP_INVALID)

  return 0;
}