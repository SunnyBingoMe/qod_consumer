#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "consumer.h"

#include <stdlib.h>
#include <stddef.h> /* offsetof */
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <pthread.h>
#include <net/if_arp.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <time.h>
#include <semaphore.h>

#define STPBRIDGES 0x0026
#define CDPVTP 0x016E
#define ETHERTYPE_VLAN 0x8100

static void hexdump(FILE* fp, const char* data, size_t size);

struct vlan_proto {
  u_int16_t type; /* Encoded VLAN */
};

struct my_arpreq {
  unsigned char arp_sha[ETH_ALEN];      /* Sender hardware address.  */
  unsigned char arp_sip[4];             /* Sender IP address.  */
  unsigned char arp_tha[ETH_ALEN];      /* Target hardware address.  */
  unsigned char arp_tip[4];             /* Target IP address.  */
};

int classify_packet(struct cap_header* cp, struct frame_t* frame){
  assert(cp);
  assert(frame);

  memset(frame, 0, sizeof(struct frame_t));

  char* ptr = (char*)cp;
  frame->type = 0;
  frame->eth = (struct ethhdr*)cp->payload;
  frame->frame_size = cp->len;
  frame->payload_size = cp->caplen;

  /* offset from capture header to start of ethernet frame payload */
  size_t ethernet_offset = sizeof(cap_head) + sizeof(struct ethhdr);
  
  switch ( ntohs(frame->eth->h_proto) ){
  case ETHERTYPE_VLAN:
    frame->vlan = (struct ether_vlan_header*)frame->eth;
    {
      size_t vlan_offset = sizeof(struct cap_header) + sizeof(struct ether_vlan_header);
      struct vlan_proto* vlanPROTO = (struct vlan_proto*)(ptr + vlan_offset);
      fprintf(stdout, "802.1Q vlan# %d: ", 0x0FFF&ntohs(frame->vlan->vlan_tci));
      
      if( ntohs(vlanPROTO->type) != ETHERTYPE_IP ){
	fprintf(stderr, ": Encapsulation type [%0x] ", ntohs(vlanPROTO->type));
	fprintf(stderr, "Not a IP .\n");
	return 1;
      }

      frame->type |= PACKET_IP;
      frame->ip = (struct ip*)(ptr + vlan_offset + sizeof(struct vlan_proto));
    }
    /* fallthrough */

  case ETHERTYPE_IP:
    if ( !frame->ip ){ /* may be set by ETHERTYPE_VLAN */
      frame->type |= PACKET_IP;
      frame->ip = (struct ip*)(ptr + ethernet_offset);
    }
    {
      /* offset from capture header to ip payload */
      size_t ip_offset = ethernet_offset + 4*frame->ip->ip_hl;

      switch( frame->ip->ip_p) {
      case IPPROTO_TCP:
	frame->type |= TRANSPORT_TCP;
	frame->tcp = (struct tcphdr*)(ptr + ip_offset);
	frame->payload = (void*)frame->tcp + sizeof(struct tcphdr);
	return 0;

      case IPPROTO_UDP:
	frame->type |= TRANSPORT_UDP;
	frame->udp = (struct udphdr*)(ptr + ip_offset);
	frame->payload = (void*)frame->udp + sizeof(struct udphdr);
	return 0;

      case IPPROTO_ICMP:
	frame->type |= PACKET_ICMP;
	frame->icmp = (struct icmphdr*)(ptr + ip_offset);
	frame->payload = NULL;
	return 0;

      default:
	fprintf(stdout, "Unknown transport protocol: %d \n", frame->ip->ip_p);
	return 1;
      }
    }
    break;

  case ETHERTYPE_IPV6:
    fprintf(stdout, "IPv6\n");
    return 1;
   
  case ETHERTYPE_ARP:
    frame->type |= PACKET_ARP;
    frame->arp.header = (struct arphdr*)(ptr + ethernet_offset);
    frame->arp.req = (struct my_arpreq*)(ptr + ethernet_offset + sizeof(struct arphdr));
    return 0;

  case 0x0810:
    fprintf(stdout, "MP packet\n");
    return 1;

  case STPBRIDGES:
    fprintf(stdout, "STP(0x%x): (spanning-tree for bridges)\n",ntohs(frame->eth->h_proto));
    return 1;

  case CDPVTP:
    fprintf(stdout, "CDP(0x%x): (CISCO Discovery Protocol)\n",ntohs(frame->eth->h_proto));
    return 1;

  default:
    fprintf(stdout, "Unknown ethernet protocol (0x%x)\n", ntohs(frame->eth->h_proto));
    hexdump(stdout, cp->payload, cp->caplen);
    return 1;
  }
}

void print_frame_ip(FILE* dst, const struct frame_t* frame){
  fprintf(dst, "IPv4(HDR[%d])[", 4*frame->ip->ip_hl);
  fprintf(dst, "Len=%d:",(u_int16_t)ntohs(frame->ip->ip_len));
  fprintf(dst, "ID=%d:",(u_int16_t)ntohs(frame->ip->ip_id));
  fprintf(dst, "TTL=%d:",(u_int8_t)frame->ip->ip_ttl);
  fprintf(dst, "Chk=%d:",(u_int16_t)ntohs(frame->ip->ip_sum));
  
  if(ntohs(frame->ip->ip_off) & IP_DF) {
    fprintf(dst, "DF");
  }
  if(ntohs(frame->ip->ip_off) & IP_MF) {
    fprintf(dst, "MF");
  }
  
  fprintf(dst, " Tos:%0x]:\t",(u_int8_t)frame->ip->ip_tos);
  
  if ( frame->type & TRANSPORT_TCP ){
    fprintf(dst, "TCP(HDR[%d]DATA[%0x]):\t [",4*frame->tcp->doff, ntohs(frame->ip->ip_len) - 4*frame->tcp->doff - 4*frame->ip->ip_hl);
    if(frame->tcp->syn) {
      fprintf(dst, "S");
    }
    if(frame->tcp->fin) {
      fprintf(dst, "F");
    }
    if(frame->tcp->ack) {
      fprintf(dst, "A");
    }
    if(frame->tcp->psh) {
      fprintf(dst, "P");
    }
    if(frame->tcp->urg) {
      fprintf(dst, "U");
    }
    if(frame->tcp->rst) {
      fprintf(dst, "R");
    }
    
    fprintf(dst, "] %s:%d ",inet_ntoa(frame->ip->ip_src),(u_int16_t)ntohs(frame->tcp->source));
    fprintf(dst, " --> %s:%d",inet_ntoa(frame->ip->ip_dst),(u_int16_t)ntohs(frame->tcp->dest));
  }
  
  if ( frame->type & TRANSPORT_UDP ){
    fprintf(dst, "UDP(HDR[8]DATA[%d]):\t %s:%d ",(u_int16_t)(ntohs(frame->udp->len)-8),inet_ntoa(frame->ip->ip_src),(u_int16_t)ntohs(frame->udp->source));
    fprintf(dst, " --> %s:%d", inet_ntoa(frame->ip->ip_dst),(u_int16_t)ntohs(frame->udp->dest));
  }
  
  if ( frame->type & PACKET_ICMP ){
    fprintf(dst, "ICMP:\t %s ",inet_ntoa(frame->ip->ip_src));
    fprintf(dst, " --> %s ",inet_ntoa(frame->ip->ip_dst));
    fprintf(dst, "Type %d , code %d",frame->icmp->type, frame->icmp->code);
    if( frame->icmp->type==0 && frame->icmp->code==0){
      fprintf(dst, " echo reply: SEQNR = %d ", frame->icmp->un.echo.sequence);
    }
    if( frame->icmp->type==8 && frame->icmp->code==0){
      fprintf(dst, " echo reqest: SEQNR = %d ", frame->icmp->un.echo.sequence);
    }
  }
}

void print_frame_arp(FILE* dst, const struct frame_t* frame){
  fprintf(dst, "ARP (HDR[%zd]):", sizeof(struct my_arpreq) + sizeof(struct arphdr));

  if(ntohs(frame->arp.header->ar_op)==ARPOP_REQUEST ) {
    fprintf(dst, "Request: %02d.%02d.%02d.%02d tell ",
	    frame->arp.req->arp_tip[0], frame->arp.req->arp_tip[1], frame->arp.req->arp_tip[2], frame->arp.req->arp_tip[3]);
    fprintf(dst, "%02d.%02d.%02d.%02d ",
	    frame->arp.req->arp_sip[0], frame->arp.req->arp_sip[1], frame->arp.req->arp_sip[2], frame->arp.req->arp_sip[3]);
    fprintf(dst, "(%02X:%02X:%02X:%02X:%02X:%02X)",
	    frame->arp.req->arp_sha[0], frame->arp.req->arp_sha[1], frame->arp.req->arp_sha[2], frame->arp.req->arp_sha[3], frame->arp.req->arp_sha[4], frame->arp.req->arp_sha[5]);
  }
  if(ntohs(frame->arp.header->ar_op)==ARPOP_REPLY ) {
    fprintf(dst, "Reply: %02d.%02d.%02d.%02d is at (%02X:%02X:%02X:%02X:%02X:%02X) tell ",
	    frame->arp.req->arp_tip[0], frame->arp.req->arp_tip[1], frame->arp.req->arp_tip[2], frame->arp.req->arp_tip[3],
	    frame->arp.req->arp_tha[0], frame->arp.req->arp_tha[1], frame->arp.req->arp_tha[2], frame->arp.req->arp_tha[3], frame->arp.req->arp_tha[4], frame->arp.req->arp_tha[5]);
    fprintf(dst, " %02d.%02d.%02d.%02d (%02X:%02X:%02X:%02X:%02X:%02X)",
	    frame->arp.req->arp_sip[0], frame->arp.req->arp_sip[1], frame->arp.req->arp_sip[2], frame->arp.req->arp_sip[3],
	    frame->arp.req->arp_sha[0], frame->arp.req->arp_sha[1], frame->arp.req->arp_sha[2], frame->arp.req->arp_sha[3], frame->arp.req->arp_sha[4], frame->arp.req->arp_sha[5]);
  }
}

/**
 * Dump the content of data as hexadecimal (and its ascii repr.)
 */
static void hexdump(FILE* fp, const char* data, size_t size){
  const size_t align = size + (size % 16);
  fputs("[0000]  ", fp);
  for( int i=0; i < align; i++){
    if ( i < size ){
      fprintf(fp, "%02X ", data[i] & 0xff);
    } else {
      fputs("   ", fp);
    }
    if ( i % 4 == 3 ){
      fputs("   ", fp);
    }
    if ( i % 16 == 15 ){
      fputs("    |", fp);
      for ( int j = i-15; j<=i; j++ ){
	char ch = data[j];

	if ( j >= size ){
	  ch = ' ';
	} else if ( !isprint(data[j]) ){
	  ch = '.';
	}

	fputc(ch, fp);
      }
      fputs("|", fp);
      if ( (i+1) < align){
	fprintf(fp, "\n[%04X]  ", i+1);
      }
    }
  }
  printf("\n");
}

void print_frame(FILE* dst, const struct frame_t* frame, int show_payload){
  if ( ntohs(frame->eth->h_proto) == ETHERTYPE_IP ){
    print_frame_ip(dst, frame);
  }

  if ( ntohs(frame->eth->h_proto) == ETHERTYPE_ARP ){
    print_frame_arp(dst, frame);
  }

  if( show_payload && frame->payload ){
    hexdump(dst, frame->payload, frame->payload_size);
  }
  
  fprintf(dst, "\n");
}

static int min(int a, int b){
  return b + ((a - b) & -(a < b));
}

static int max(int a, int b){
  return a - ((a - b) & -(a < b));
}

struct consumer_thread {
  pthread_t thread;
  pthread_mutex_t mutex;
  sem_t semaphore;
  
  volatile int state;
  timepico time;

  struct stream* stream[4];
  struct filter* filter[4];

  timepico delay;
  int read_pos;
  int write_pos;
  size_t buffer_size;
  struct packet pkt[0];
};

static void* consumer_thread_func(struct consumer_thread* con){
  static const size_t caphead_offset = offsetof(struct packet, caphead);
  static const size_t buffer_offset  = offsetof(struct packet, buf);
  static unsigned int pkt_counter = 1;

  con->state = 1;
  while ( con->state == 1 ){
    for ( int i = 0; i < 4; i++ ){
      if ( !con->stream[i] ){
	continue;
      }

      cap_head* cp;
      long ret = stream_read(con->stream[i], &cp, con->filter[i], NULL);
      if ( ret == 0 ){
	pthread_mutex_lock(&con->mutex);
	{
	  struct packet* pkt = &con->pkt[con->write_pos];
	  char* data = (char*)pkt;
	  
	  const int overwrite = pkt->used == 1;
	  
	  if ( overwrite ){
	    fprintf(stderr, "consumer not reading fast enough. HERE BE DRAGONS!\n");
	    con->read_pos = (con->read_pos + 1) % con->buffer_size;
	  }

	  const size_t len = min(cp->caplen, MAX_CAPTURE_SIZE);
	  
	  memcpy(data + caphead_offset, cp, sizeof(struct cap_header));
	  memcpy(data + buffer_offset,  cp->payload, len);
	  pkt->used = 1;
	  pkt->packet_id = pkt_counter++;
	  pkt->stream_id = i;
	  
	  /* apply delay */
	  pkt->caphead.ts.tv_sec  += con->delay.tv_sec;
	  pkt->caphead.ts.tv_psec += con->delay.tv_psec;
	  if ( pkt->caphead.ts.tv_psec > 1000000000000 ){ /* wrap psec */
	    pkt->caphead.ts.tv_sec += 1;
	    pkt->caphead.ts.tv_psec -= 1000000000000;
	  }

	  con->write_pos = (con->write_pos + 1) % con->buffer_size;
	  
	  /* if a packet was overwritten the semaphore is not raised as a new packet wasn't added. */
	  if ( !overwrite ){
	    sem_post(&con->semaphore);
	  }

	}
	pthread_mutex_unlock(&con->mutex);
      } else if ( ret == EAGAIN ){
	continue;
      } else {
	fprintf(stderr, "read_post failed with code 0x%08lX: %s\n", ret, caputils_error_string(ret));
      }
    }
  }

  return 0;
}

int consumer_thread_init(consumer_thread_t* conptr, size_t buffer_size, const timepico* delay){
  size_t size = sizeof(struct consumer_thread) + sizeof(struct packet) * buffer_size;
  consumer_thread_t con = (struct consumer_thread*)malloc(size);
  memset(con, 0, size);
  con->buffer_size = buffer_size;

  if ( delay ){
    con->delay.tv_sec  = delay->tv_sec;
    con->delay.tv_psec = delay->tv_psec;
  }

  pthread_mutex_init(&con->mutex, NULL);
  sem_init(&con->semaphore, 0, 0);
  
  *conptr = con;
  return pthread_create(&con->thread, NULL, (void* (*)(void*))consumer_thread_func, con);
}

long consumer_thread_add_stream(consumer_thread_t con, const stream_addr_t* src, const char* nic, int port, struct filter* filter){
  struct stream* st;
  long ret;
  if( (ret=stream_open(&st, src, nic, port)) != 0 ) {
    return ret;
  }
  
  int i;
  for ( i = 0; i < 4; i++ ){
    if ( con->stream[i] ){
      continue;
    }
    con->stream[i] = st;
    con->filter[i] = filter;
    break;
  }
  if ( i == 5 ){
    stream_close(st);
    return -1;
  }

  return 0;
}

int consumer_thread_destroy(consumer_thread_t con){
  /** @todo IMPLEMENT ME! */
  return 0;
}

int consumer_thread_poll(consumer_thread_t con, struct packet* pkt, unsigned int timeout){
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);

  /* create absolute timeout. there is no point in having higher precision than
   * ms for the timeout param. The application cannot guarantee anything and most
   * of the time it waits for longer periods. Dealing with nanosec directly would
   * just mean having to deal with large numbers for no real purpose. */
  struct timespec timeout_ts = ts;
  timeout_ts.tv_nsec += timeout * 1000000; /* to nanosec */
  if ( timeout_ts.tv_nsec > 1000000000 ){
    timeout_ts.tv_sec += 1;
    timeout_ts.tv_nsec -= 1000000000;
  }

  if ( sem_timedwait(&con->semaphore, &timeout_ts) != 0 ){
    if ( errno != ETIMEDOUT ){
      fprintf(stderr, "sem_trywait() returned %d: %s\n", errno, strerror(errno));
    }
    return 0;
  }

  timepico tp = timespec_to_timepico(ts);
  struct packet* tmp;

  pthread_mutex_lock(&con->mutex);
  {
    tmp = &con->pkt[con->read_pos];
    
    if ( timecmp(&tp, &tmp->caphead.ts) < 0 ){
      pthread_mutex_unlock(&con->mutex);
      return 0; /* defer */
    }
    
    con->pkt[con->read_pos].used = 0;
    con->read_pos = (con->read_pos + 1) % con->buffer_size;
  }
  pthread_mutex_unlock(&con->mutex);
    
  memcpy(pkt, tmp, sizeof(struct packet));
  return 1;
}

int consumer_thread_pending(consumer_thread_t con){
  int sval;
  sem_getvalue(&con->semaphore, &sval);
  return max(sval, 0); /* POSIX.1-2001 allows sval to be -(threads blocking) */
}

void consumer_lock(consumer_thread_t con){
  pthread_mutex_lock(&con->mutex);
}

void consumer_unlock(consumer_thread_t con){
  pthread_mutex_unlock(&con->mutex);
}

struct packet* consumer_buffer_get(consumer_thread_t con, unsigned int index){
  return &con->pkt[index];
}
