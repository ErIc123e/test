#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "emulator.h"
#include "sr.h"  

#define RTT  16.0       /* round trip time.  MUST BE SET TO 16.0 when submitting assignment */
#define WINDOWSIZE 6    /* the maximum number of buffered unacked packet */
#define SEQSPACE 12     /* the min sequence space for sr must be at least windowsize *2 */
#define NOTINUSE (-1)   /* used to fill header fields that are not being used */

/* generic procedure to compute the checksum of a packet.  Used by both sender and receiver  */
int ComputeChecksum(struct pkt packet)
{
  int checksum = 0;
  for (int i = 0; i < 20; i++)
    checksum += (int)(packet.payload[i]);
  checksum += packet.seqnum + packet.acknum;
  return checksum;
}

bool IsCorrupted(struct pkt packet)
{
  return packet.checksum != ComputeChecksum(packet);
}

/********* Sender (A) variables and functions ************/

static struct pkt buffer[WINDOWSIZE];  /* array for storing packets waiting for ACK */
static int windowfirst, windowlast;    /* array indexes of the first/last packet awaiting ACK */
static int windowcount;                /* the number of packets currently awaiting an ACK */
static int A_nextseqnum;               /* the next sequence number to be used by the sender */
static bool acked[SEQSPACE];            /* array to track whether packets are ACKed */

/* called from layer 5 (application layer), passed the message to be sent to other side */
void A_output(struct msg message)
{
  struct pkt sendpkt;
  if (windowcount < WINDOWSIZE) {
    sendpkt.seqnum = A_nextseqnum;
    sendpkt.acknum = NOTINUSE;
    for (int i = 0; i < 20; i++)
      sendpkt.payload[i] = message.data[i];
    sendpkt.checksum = ComputeChecksum(sendpkt);

    windowlast = (windowlast + 1) % WINDOWSIZE;
    buffer[windowlast] = sendpkt;
    windowcount++;

    tolayer3(A, sendpkt);

    if (windowcount == 1)
      starttimer(A, RTT);

    A_nextseqnum = (A_nextseqnum + 1) % SEQSPACE;
  } else {
    window_full++;
  }
}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input(struct pkt packet)
{
  if (!IsCorrupted(packet)) {
    if (!acked[packet.acknum]) {
      new_ACKs++;
      acked[packet.acknum] = true;

      if (packet.acknum == buffer[windowfirst].seqnum) {
        stoptimer(A);
        while (windowcount > 0 && acked[buffer[windowfirst].seqnum]) {
          windowfirst = (windowfirst + 1) % WINDOWSIZE;
          windowcount--;
        }
        if (windowcount > 0)
          starttimer(A, RTT);
      }
    }
  }
}

/* called when A's timer goes off */
void A_timerinterrupt(void)
{
  if (windowcount > 0) {
    stoptimer(A);
    for (int i = 0; i < windowcount; i++) {
      int idx = (windowfirst + i) % WINDOWSIZE;
      if (!acked[buffer[idx].seqnum]) {
        tolayer3(A, buffer[idx]);
        packets_resent++;
      }
    }
    starttimer(A, RTT);
  }
}

/* the following routine will be called once (only) before any other */
void A_init(void)
{
  A_nextseqnum = 0;
  windowfirst = 0;
  windowlast = -1;
  windowcount = 0;
  for (int i = 0; i < SEQSPACE; i++)
    acked[i] = false;
}

/********* Receiver (B)  variables and procedures ************/

static int expectedseqnum; /* the sequence number expected next by the receiver */
static struct pkt receivepkt[SEQSPACE]; /* array to store packets received by B */
static bool received[SEQSPACE]; /* array to keep track of which packets have been received */

/* called from layer 3, when a packet arrives for layer 4 at B */
void B_input(struct pkt packet)
{
  struct pkt sendpkt;
  if (!IsCorrupted(packet)) {
    packets_received++;

    if (!received[packet.seqnum]) {
      received[packet.seqnum] = true;
      for (int i = 0; i < 20; i++)
        receivepkt[packet.seqnum].payload[i] = packet.payload[i];
    }

    /* deliver in-order packets to layer 5 */
    while (received[expectedseqnum]) {
      tolayer5(B, receivepkt[expectedseqnum].payload);
      received[expectedseqnum] = false;
      expectedseqnum = (expectedseqnum + 1) % SEQSPACE;
    }

    /* send an ACK for the received packet */
    sendpkt.acknum = packet.seqnum;
    sendpkt.seqnum = NOTINUSE;
    for (int i = 0; i < 20; i++)
      sendpkt.payload[i] = '0';
    sendpkt.checksum = ComputeChecksum(sendpkt);
    tolayer3(B, sendpkt);
  }
}

/* the following routine will be called once (only) before any other */
void B_init(void)
{
  expectedseqnum = 0;
  for (int i = 0; i < SEQSPACE; i++)
    received[i] = false;
}

/******************************************************************************
 * The following functions need be completed only for bi-directional messages *
 *****************************************************************************/

void B_output(struct msg message) {}
void B_timerinterrupt(void) {}
