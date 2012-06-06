#ifndef CONSUMER_H
#define CONSUMER_H

#include <caputils/caputils.h>
#include <arpa/inet.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_CAPTURE_SIZE 1600
struct llc_pdu_sn {
  uint8_t dsap;
  uint8_t ssap;
  uint8_t ctrl_1;
  uint8_t ctrl_2;
};



struct packet {
  uint16_t used;
  uint16_t stream_id;
  uint32_t packet_id;
  struct cap_header caphead;
  char buf[MAX_CAPTURE_SIZE]; /* this should be determined by looking at the stream header and it's caplen */
};

enum packet_type_t {
  PACKET_IP = (1<<0),
  PACKET_ICMP = (1<<1),
  PACKET_ARP = (1<<2),

  TRANSPORT_TCP = (1<<3),
  TRANSPORT_UDP = (1<<4),
};

struct frame_t {
  uint32_t type; /* bitmask */
  size_t frame_size;
  size_t payload_size;

  struct ethhdr* eth;
  struct ether_vlan_header* vlan; /* NULL if vlan header isn't present */
  union {
    /* IP */
    struct {
      struct ip* ip;
      union {
	struct tcphdr* tcp;
	struct udphdr* udp;
	struct icmphdr* icmp;
      };
    };

    /* ARP */
    struct {
      struct arphdr* header;
      struct my_arpreq* req;
    } arp;
  };
  char* payload;
};

int classify_packet(struct cap_header* cp, struct frame_t* frame);
void print_frame(FILE* dst, const struct frame_t* frame, int show_payload);

typedef struct consumer_thread* consumer_thread_t;
  int consumer_thread_init(consumer_thread_t* con, size_t buffer_size, const timepico* delay);
long consumer_thread_add_stream(consumer_thread_t con, const stream_addr_t* src, const char* nic, int port, struct filter* filter);
int consumer_thread_destroy(consumer_thread_t con);

  /**
   * Read a packet from the buffer.
   *
   * @param pkt Packet will be copied here.
   * @param timeout timeout in ms.
   * @return 1 if a packet was read or 0 if no packet is available yet.
   */
  int consumer_thread_poll(consumer_thread_t con, struct packet* pkt, unsigned int timeout);

  /**
   * Returns the number of unread packets in the buffer.
   */
  int consumer_thread_pending(consumer_thread_t con);

  void consumer_lock(consumer_thread_t con);
  void consumer_unlock(consumer_thread_t con);
  struct packet* consumer_buffer_get(consumer_thread_t con, unsigned int index);

#ifdef __cplusplus
}
#endif

#endif /* CONSUMER_H */
