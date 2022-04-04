# CS3102 P2

## Running

To run RdtClient from the `code` directory;

```shell
make RdtClient
./RdtClient <hostname of server/slurpe> <file to send>
```

To run RdtServer from the `code` directory:

```shell
make RdtServer
./RdtServer <file to output received data to>
```

## Files:

- Makefile (Makefile for all source code)
- rdt.c (Main RDT source code file)
- rdt.h (Main RDT header file)
- RdtClient.c (Example RDT client that sends a file to an RDT server)
- RdtServer.c (Example RDT server that receives a file and writes it to a new file)
- RdtClientRTT.c (Test program used for calculating average RTT for report. Runs for 10 rounds.)
- RdtServerRTT.c (Test program used for receiving packets from RdtClientRTT.c)
- UdpSocket/UdpSocket.c (UdpSocket source code by Saleem Bhatti)
- UdpSocket/UdpSocket.h (Header file for UdpSocket/UdpSocket.c)
- sigio/sigio.c (Provides SIGIO functionality used to receive packets triggered by network interrupts. Modified from source code by Saleem Bhatti)
- sigio/sigio.h (Header file for sigio/sigio.c)
- sigalrm/sigalrm.c (Provides SIGALRM functionality used to trigger alarms for retransmisison timeouts. Modified from source code by Saleem Bhatti)
- sigalrm/sigalrm.h (Header file for sigalrm/sigalrm.c)
- rto/rto.c (Source code for calculating adaptive RTO and measuring RTT. Modified from source code by Saleem Bhatti)
- rto/rto.h (Header file for rto/rto.c)
- d_print/d_print.c (Source code by Salem Bhatti for debug output).
- d_print/d_print.h (Header file for d_print/d_print.c)
- checksum/checksum.c (Provides IPv4 Header Checksum functionality. Modified from source code by Saleem Bhatti)
- checksum/checksum.h (Header file for checksum/checksum.c)