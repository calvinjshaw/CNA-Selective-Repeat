#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "emulator.h"
#include "sr.h"




/* ******************************************************************
   Go Back N protocol.  Adapted from J.F.Kurose
   ALTERNATING BIT AND GO-BACK-N NETWORK EMULATOR: VERSION 1.2  

   Network properties:
   - one way network delay averages five time units (longer if there
   are other messages in the channel for GBN), but can be larger
   - packets can be corrupted (either the header or the data portion)
   or lost, according to user-defined probabilities
   - packets will be delivered in the order in which they were sent
   (although some can be lost).

   Modifications: 
   - removed bidirectional GBN code and other code not used by prac. 
   - fixed C style to adhere to current programming style
   - added GBN implementation
**********************************************************************/

#define RTT  16.0       /* round trip time.  MUST BE SET TO 16.0 when submitting assignment */
#define WINDOWSIZE 6    /* the maximum number of buffered unacked packet */
#define SEQSPACE 12      /* the min sequence space for GBN must be at least windowsize *2 */
#define NOTINUSE (-1)   /* used to fill header fields that are not being used */
/* generic procedure to compute the checksum of a packet.  Used by both sender and receiver  
   the simulator will overwrite part of your packet with 'z's.  It will not overwrite your 
   original checksum.  This procedure must generate a different checksum to the original if
   the packet is corrupted.
*/


int ComputeChecksum(struct pkt packet)
{
  int checksum = 0;
  int i;

  checksum = packet.seqnum;
  checksum += packet.acknum;
  for ( i=0; i<20; i++ ) 
    checksum += (int)(packet.payload[i]);

  return checksum;
}

bool IsCorrupted(struct pkt packet)
{
  if (packet.checksum == ComputeChecksum(packet))
    return (false);
  else
    return (true);
}


/********* Sender (A) variables and functions ************/

static struct pkt buffer[WINDOWSIZE];  /* array for storing packets waiting for ACK */
static int windowfirst, windowlast;    /* array indexes of the first/last packet awaiting ACK */
static int windowcount;                /* the number of packets currently awaiting an ACK */
static int A_nextseqnum;               /* the next sequence number to be used by the sender */
static int timer_packet = -1;        /* The seqnum of the packet currently being timed */
static bool acked[SEQSPACE];         /* Track which packets are ACKed */

/* called from layer 5 (application layer), passed the message to be sent to other side */
void A_init(void) {
  base        = 0;
  A_nextseqnum  = 0;
  timer_packet= -1;
  for (int i = 0; i < SEQSPACE; i++) acked[i] = false;
}

void A_output(struct msg message) {
  // can we send?
  int window_size = (A_nextseqnum + SEQSPACE - base) % SEQSPACE;
  if (window_size < WINDOW) {
    // build packet
    struct pkt p;
    p.seqnum  = A_nextseqnum;
    p.acknum  = -1;
    memcpy(p.payload, message.data, 20);
    p.checksum = ComputeChecksum(p);

    // buffer it
    window[A_nextseqnum] = false;

    // send it
    tolayer3(A, p);

    // if no timer running, start one for this packet
    if (timer_packet < 0) {
      timer_packet = A_nextseqnum;
      starttimer(A, RTT);
    }

    // advance nextseqnum
    (A_nextseqnum + 1) % SEQSPACE;
  } else {
    // window full; drop or buffer at appl. layer
  }
}

void A_input(struct pkt ackpkt) {
  if (IsCorrupted(ackpkt)) return;  // ignore corrupted ACK

  int ack = ackpkt.acknum;
  if (acked[ack]) return;           // duplicate ACK

  // mark this packet as ACKed
  acked[ack] = true;

  // slide base forward over any newly ACKed packets
  while (acked[base]) {
    acked[base] = false;  // clear for potential reuse
    base = (base + 1) % SEQSPACE;
  }

  // reset or cancel timer
  if (base == A_nextseqnum) {
    // nothing left outstanding
    stoptimer(A);
    timer_packet = -1;
  } else {
    // there are still un-ACKed packets; restart timer on the new oldest
    stoptimer(A);
    timer_packet = base;
    starttimer(A, RTT);
  }
}

void A_timerinterrupt(void) {
  // retransmit only the oldest un-ACKed packet
  if (timer_packet >= 0) {
    tolayer3(A, window[timer_packet]);
    packets_resent++;
    // restart timer
    starttimer(A, RTT);
  }
}

       



/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init(void)
{
  int i;
  /* initialise A's window, buffer and sequence number */
  A_nextseqnum = 0;  /* A starts with seq num 0, do not change this */
  windowfirst = 0;
  windowlast = -1;   /* windowlast is where the last packet sent is stored.  
		     new packets are placed in winlast + 1 
		     so initially this is set to -1
		   */
  windowcount = 0;
  timer_packet = -1;
  for (i = 0; i < SEQSPACE; i++){
    acked[i] = false;
  }

}



/********* Receiver (B)  variables and procedures ************/

static int expectedseqnum; /* the sequence number expected next by the receiver */
static int B_nextseqnum;   /* the sequence number for the next packets sent by B */
static bool received[SEQSPACE];    /* Marks which packets have been received */
/*static struct pkt recv_buffer[SEQSPACE]; Stores received packets */


/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input(struct pkt packet)
{
    struct pkt sendpkt;
    int i;

    if (!IsCorrupted(packet) && packet.seqnum == expectedseqnum) {
        if (TRACE > 0) {
            printf("----B: packet %d is correctly received, send ACK!\n", packet.seqnum);
        }

        packets_received++;  

        /* Deliver directly */
        tolayer5(B, packet.payload);

        /* Prepare ACK */
        sendpkt.acknum = expectedseqnum;
        expectedseqnum = (expectedseqnum + 1) % SEQSPACE;

    } else {
        if (TRACE > 0) {
            printf("----B: packet corrupted or not expected sequence number, resend ACK!\n");
        }

        if (expectedseqnum == 0)
            sendpkt.acknum = SEQSPACE - 1;
        else
            sendpkt.acknum = expectedseqnum - 1;
    }

    /* Send ACK */
    sendpkt.seqnum = B_nextseqnum;
    B_nextseqnum = (B_nextseqnum + 1) % 2;

    for (i = 0; i < 20; i++) {
        sendpkt.payload[i] = '0';
    }

    sendpkt.checksum = ComputeChecksum(sendpkt);
    tolayer3(B, sendpkt);
}




/* the following routine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init(void)
{
  int i;
  expectedseqnum = 0;
  B_nextseqnum = 1;

  for (i=0; i<SEQSPACE; i++) {
    received[i] = false;
  }
}

/******************************************************************************
 * The following functions need be completed only for bi-directional messages *
 *****************************************************************************/

/* Note that with simplex transfer from a-to-B, there is no B_output() */
void B_output(struct msg message)  
{
}

/* called when B's timer goes off */
void B_timerinterrupt(void)
{
}

