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
#define SEQSPACE 12     /* the min sequence space for GBN must be at least windowsize *2 */
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
void A_output(struct msg message)
{
  struct pkt sendpkt;
  int i;
  

  /* if not blocked waiting on ACK */
  if ( windowcount < WINDOWSIZE) {
    if (TRACE > 1)
      printf("----A: New message arrives, send window is not full, send new messge to layer3!\n");

    /* create packet */
    sendpkt.seqnum = A_nextseqnum;
    sendpkt.acknum = NOTINUSE;
    for ( i=0; i<20 ; i++ ) 
      sendpkt.payload[i] = message.data[i];
    sendpkt.checksum = ComputeChecksum(sendpkt); 

    /* put packet in window buffer */
    /* windowlast will always be 0 for alternating bit; but not for GoBackN */
    windowlast = (windowlast + 1) % WINDOWSIZE; 
    buffer[windowlast] = sendpkt;
    windowcount++;
    acked[sendpkt.seqnum] = false;

    /* send out packet */
    if (TRACE > 0){
      printf("Sending packet %d to layer 3\n", sendpkt.seqnum);
    }
    tolayer3 (A, sendpkt);

    /* If this is the first unACKed packet, start the timer */
    if (timer_packet == -1) {
      starttimer(A, RTT);
      timer_packet = sendpkt.seqnum;
  }

    /* start timer if first packet in window */
    /* if (windowcount == 1)*/
    /*   starttimer(A,RTT);*/
    /* Using new timer 1logic*/
    

    

    /* get next sequence number, wrap back to 0 */
    A_nextseqnum = (A_nextseqnum + 1) % SEQSPACE;  
  }
  /* if blocked,  window is full */
  else {
    if (TRACE > 0)
      printf("----A: New message arrives, send window is full\n");
    window_full++;
  }
}


/* called from layer 3, when a packet arrives for layer 4 
   In this practical this will always be an ACK as B never sends data.
*/
void A_input(struct pkt packet)
{
    int i;
    if (!IsCorrupted(packet)) {
        if (TRACE > 0) {
            printf("----A: uncorrupted ACK %d is received\n", packet.acknum);
        }

        new_ACKs++;

        if (acked[packet.acknum]) {
            if (TRACE > 0) {
                printf("----A: duplicate ACK received, do nothing!\n");
            }
        } else {
            if (TRACE > 0) {
                printf("----A: ACK %d is not a duplicate\n", packet.acknum);
            }

            acked[packet.acknum] = true;

            /* Slide window forward */
            while (windowcount > 0 && acked[buffer[windowfirst].seqnum]) {
                windowfirst = (windowfirst + 1) % WINDOWSIZE;
                windowcount--;
            }

            /* Reassign timer if this was the packet being timed */
            if (packet.acknum == timer_packet) {
                stoptimer(A);
                timer_packet = -1;

                /* Find next unACKed packet in the window */
                
                for (i = 0; i < windowcount; i++) {
                    int index = (windowfirst + i) % WINDOWSIZE;
                    if (!acked[buffer[index].seqnum]) {
                        timer_packet = buffer[index].seqnum;
                        starttimer(A, RTT);
                        if (TRACE == 1) {
                            printf("----A: Timer now set for packet %d\n", timer_packet);
                        }
                        break;
                    }
                }
            }
        }
    } else {
        if (TRACE == 1) {
            printf("----A: corrupted ACK is received, do nothing!\n");
        }
    }
}





/* called when A's timer goes off */
void A_timerinterrupt(void)
{
    

    if (TRACE > 0){
        printf("----A: time out,resend packets!\n");
    
        if (timer_packet != -1) {
          if (TRACE > 0){
              printf("---A: resending packet %d\n", timer_packet);
          }
          tolayer3(A, buffer[timer_packet]);
          packets_resent++;
  
          starttimer(A, RTT);  
        }
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
#define MAX_PAYLOAD_SIZE 20

static struct pkt recv_buffer[SEQSPACE];  /* To store out-of-order packets */
static bool received[SEQSPACE];           /* To track which packets are received */
static int expectedseqnum;                /* Base of receiver window */
static int B_nextseqnum;                  /* For generating ACK packet seqnum */

bool InWindow(int seq, int base, int window_size) {
  if (base + window_size < SEQSPACE) {
      return seq >= base && seq < base + window_size;
  } else {
      return seq >= base || seq < (base + window_size) % SEQSPACE;
  }
}


void B_input(struct pkt packet)
{
    struct pkt ackpkt;
    int seq = packet.seqnum;
    int i;
    
    bool in_window;

    if (IsCorrupted(packet)) {
        if (TRACE > 0) {
            printf("----B: packet corrupted or not expected sequence number, resend ACK!\n");
        }
        return;
    }

    packets_received++;
    upper_window = (expectedseqnum + WINDOWSIZE) % SEQSPACE;
    in_window = InWindow(seq, expectedseqnum, WINDOWSIZE);


    if (in_window) {
        

        if (!received[seq]) {
            if (TRACE > 0) {
                printf("----B: packet %d is correctly received, send ACK!\n", seq);
            }

            recv_buffer[seq] = packet;
            received[seq] = true;
        }

        while (received[expectedseqnum]) {
            tolayer5(B, recv_buffer[expectedseqnum].payload);
            received[expectedseqnum] = false;
            expectedseqnum = (expectedseqnum + 1) % SEQSPACE;
        }
    } else {
        if (TRACE > 0) {
            printf("----B: packet corrupted or not expected sequence number, resend ACK!\n");
        }
    }

    ackpkt.seqnum = B_nextseqnum;
    B_nextseqnum = (B_nextseqnum + 1) % 2;
    ackpkt.acknum = seq;

    for (i = 0; i < MAX_PAYLOAD_SIZE; i++) {
        ackpkt.payload[i] = '0';
    }

    ackpkt.checksum = ComputeChecksum(ackpkt);
    tolayer3(B, ackpkt);
}



/* the following routine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init(void)
{
    int i;
    int j;

    expectedseqnum = 0;
    B_nextseqnum = 1;

    for (i = 0; i < SEQSPACE; i++) {
        received[i] = false;

        /* Optional: zero out recv_buffer content */
        recv_buffer[i].seqnum = 0;
        recv_buffer[i].acknum = 0;
        recv_buffer[i].checksum = 0;
        for (j = 0; j < MAX_PAYLOAD_SIZE; j++) {
            recv_buffer[i].payload[j] = 0;
        }
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

