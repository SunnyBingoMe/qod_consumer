#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "consumer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <getopt.h>
#include <errno.h>

#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>




#define STPBRIDGES 0x0026
#define CDPVTP 0x016E

struct {
  int print_content;
  int cDate;
  unsigned long long max_pkts;
} args;

int clone_stream(struct stream* dst, struct stream* src, const struct filter* filter, unsigned long long* matches){
  cap_head* cp;
  size_t len = sizeof(struct cap_header);
  long ret;

  *matches = 0;
  while ( 1 ){
    ret = stream_read(src, &cp, filter, NULL);
    if ( ret == EAGAIN ){
      continue;
    } else if ( ret != 0 ){
      break;
    }

    (*matches)++;
    if( (ret=stream_write(dst, (char*)cp, cp->caplen + len)) != 0 ){
	    fprintf(stderr, "stream_write() returned 0x%08lx: %s\n", ret, caputils_error_string(ret));
    }
  }

  return 0;
}

static void print_tcp(FILE* dst, const struct ip* ip, const struct tcphdr* tcp){
  fprintf(dst, "TCP(HDR[%d]DATA[%0x]):\t [",4*tcp->doff, ntohs(ip->ip_len) - 4*tcp->doff - 4*ip->ip_hl);
  if(tcp->syn) {
    fprintf(dst, "S");
  }
  if(tcp->fin) {
    fprintf(dst, "F");
  }
  if(tcp->ack) {
      fprintf(dst, "A");
  }
  if(tcp->psh) {
    fprintf(dst, "P");
  }
  if(tcp->urg) {
    fprintf(dst, "U");
  }
  if(tcp->rst) {
    fprintf(dst, "R");
  }
  
  fprintf(dst, "] %s:%d ",inet_ntoa(ip->ip_src),(u_int16_t)ntohs(tcp->source));
  fprintf(dst, " --> %s:%d",inet_ntoa(ip->ip_dst),(u_int16_t)ntohs(tcp->dest));
  fprintf(dst, "\n");
}

static void print_udp(FILE* dst, const struct ip* ip, const struct udphdr* udp){
  fprintf(dst, "UDP(HDR[8]DATA[%d]):\t %s:%d ",(u_int16_t)(ntohs(udp->len)-8),inet_ntoa(ip->ip_src),(u_int16_t)ntohs(udp->source));
  fprintf(dst, " --> %s:%d", inet_ntoa(ip->ip_dst),(u_int16_t)ntohs(udp->dest));
  fprintf(dst, "\n");
}

static void print_icmp(FILE* dst, const struct ip* ip, const struct icmphdr* icmp){
  fprintf(dst, "ICMP:\t %s ",inet_ntoa(ip->ip_src));
  fprintf(dst, " --> %s ",inet_ntoa(ip->ip_dst));
  fprintf(dst, "Type %d , code %d", icmp->type, icmp->code);
  if( icmp->type==0 && icmp->code==0){
    fprintf(dst, " echo reply: SEQNR = %d ", icmp->un.echo.sequence);
  }
  if( icmp->type==8 && icmp->code==0){
    fprintf(dst, " echo reqest: SEQNR = %d ", icmp->un.echo.sequence);
  }
  fprintf(dst, "\n");
}

static void print_ipv4(FILE* dst, const struct ip* ip){
  void* payload = ((char*)ip) + 4*ip->ip_hl;
  fprintf(dst, "IPv4(HDR[%d])[", 4*ip->ip_hl);
  fprintf(dst, "Len=%d:",(u_int16_t)ntohs(ip->ip_len));
  fprintf(dst, "ID=%d:",(u_int16_t)ntohs(ip->ip_id));
  fprintf(dst, "TTL=%d:",(u_int8_t)ip->ip_ttl);
  fprintf(dst, "Chk=%d:",(u_int16_t)ntohs(ip->ip_sum));
  
  if(ntohs(ip->ip_off) & IP_DF) {
    fprintf(dst, "DF");
  }
  if(ntohs(ip->ip_off) & IP_MF) {
    fprintf(dst, "MF");
  }
  
  fprintf(dst, " Tos:%0x]:\t",(u_int8_t)ip->ip_tos);

  switch( ip->ip_p ) {
  case IPPROTO_TCP:
    print_tcp(dst, ip, (const struct tcphdr*)payload);
    break;
    
  case IPPROTO_UDP:
    print_udp(dst, ip, (const struct udphdr*)payload);
    break;

  case IPPROTO_ICMP:
    print_icmp(dst, ip, (const struct icmphdr*)payload);
    break;

  default:
    fprintf(dst, "Unknown transport protocol: %d \n", ip->ip_p);
    break;
  }
}
static void print_ieee8023(FILE* dst, const struct llc_pdu_sn* llc){
  fprintf(dst,"dsap=%02x ssap=%02x ctrl1 = %02x ctrl2 = %02x\n", llc->dsap, llc->ssap, llc->ctrl_1, llc->ctrl_2);


}

static void print_eth(FILE* dst, const struct ethhdr* eth){
  void* payload = ((char*)eth) + sizeof(struct ethhdr);
  uint16_t h_proto = ntohs(eth->h_proto);
  uint16_t vlan_tci;


 begin:

  if(h_proto<0x05DC){
    fprintf(dst, "IEEE802.3 ");
    fprintf(dst, "  %02x:%02x:%02x:%02x:%02x:%02x -> %02x:%02x:%02x:%02x:%02x:%02x ", 
	    eth->h_source[0],eth->h_source[1],eth->h_source[2],eth->h_source[3],eth->h_source[4],eth->h_source[5],
	    eth->h_dest[0],  eth->h_dest[1],  eth->h_dest[2],  eth->h_dest[3],  eth->h_dest[4],  eth->h_dest[5]);
    print_ieee8023(dst,(struct llc_pdu_sn*)payload);
  } else {
    switch ( h_proto ){
    case ETHERTYPE_VLAN:
      vlan_tci = ((uint16_t*)payload)[0];
      h_proto = ntohs(((uint16_t*)payload)[0]);
      payload = ((char*)eth) + sizeof(struct ethhdr);
      fprintf(dst, "802.1Q vlan# %d: ", 0x0FFF&ntohs(vlan_tci));
      goto begin;
      
    case ETHERTYPE_IP:
      print_ipv4(dst, (struct ip*)payload);
      break;
      
    case ETHERTYPE_IPV6:
      printf("ipv6\n");
      break;
      
    case ETHERTYPE_ARP:
      printf("arp\n");
      break;
      
    case 0x0810:
      fprintf(dst, "MP packet\n");
      break;
      
    case STPBRIDGES:
      fprintf(dst, "STP(0x%04x): (spanning-tree for bridges)\n", h_proto);
      break;
      
    case CDPVTP:
      fprintf(dst, "CDP(0x%04x): (CISCO Discovery Protocol)\n", h_proto);
      break;
      
      
      
    default:
      fprintf(dst, "Unknown ethernet protocol (0x%04x),  ", h_proto);
      fprintf(dst, " %02x:%02x:%02x:%02x:%02x:%02x -> %02x:%02x:%02x:%02x:%02x:%02x \n", 
	      eth->h_source[0],eth->h_source[1],eth->h_source[2],eth->h_source[3],eth->h_source[4],eth->h_source[5],
	      eth->h_dest[0],  eth->h_dest[1],  eth->h_dest[2],  eth->h_dest[3],  eth->h_dest[4],  eth->h_dest[5]);
      break;
    }
  }
}

int display_stream(struct stream* src, const struct filter* filter, unsigned long long* matches){
  cap_head* cp;
  time_t time;
  long ret;

  *matches = 0;
  while ( 1 ) {
    ret = stream_read(src, &cp, filter, NULL);
    if ( ret == EAGAIN ){
      continue;
    } else if ( ret != 0 ){
      break;
    }

    if ( cp->caplen == 0 ){
      fprintf(stderr, "caplen is zero, will skip this packet but most likely the stream got out-of-sync and will crash later.\n");
      continue;
    }
    
    (*matches)++;
    time = (time_t)cp->ts.tv_sec;
    
    fprintf(stdout, "[%4llu]:%.4s:%.8s:", *matches, cp->nic, cp->mampid);
    if( args.cDate == 0 ) {
      fprintf(stdout, "%u.", cp->ts.tv_sec);
    } else {
      static char timeStr[25];
      struct tm tm = *gmtime(&time);
      strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tm);
      fprintf(stdout, "%s.", timeStr);
    }

    fprintf(stdout, "%012"PRId64":LINK(%4d):CAPLEN(%4d):", cp->ts.tv_psec, cp->len, cp->caplen);

    print_eth(stdout, (struct ethhdr*)cp->payload);

    if ( args.max_pkts > 0 && *matches + 1 > args.max_pkts) {
      /* Read enough pkts lets break. */
      printf("read enought packages\n");
      break;
    }
  }

  if ( ret == -1 ){ /* EOF, TCP shutdown, etc */
    fprintf(stderr, "Finished\n");
    return 0;
  } 

  fprintf(stderr, "stream_read() returned 0x%08lx: %s\n", ret, caputils_error_string(ret));
  return 0;  
}

int main(int argc, char **argv){
  extern int optind, opterr, optopt;

  int option_index;
  static struct option long_options[]= {
    {"content",0,0,'c'},
    {"output",1,0, 'o'},
    {"pkts", 1, 0, 'p'},
    {"help", 0, 0, 'h'},
    {"if", 1,0,'i'},
    {"tcp", 1,0,'t'},
    {"udp", 1,0,'u'},
    {"port", 1,0, 'v'},
    {"calender",0,0,'d'},
    {0, 0, 0, 0}
  };
  
  /* defaults */
  args.print_content = 0;
  args.cDate = 0; /* Way to display date, cDate=0 => seconds since 1970. cDate=1 => calender date */  
  args.max_pkts = 0; /* 0: all */

  char* outFilename=0;
  int capOutfile=0;
  int portnumber=0;
  char* nic = NULL;
  int streamType = 0; // Default a file
  unsigned long long pktCount = 0;
  int ret = 0;

  struct filter myfilter;
  filter_from_argv(&argc, argv, &myfilter);
  struct stream* inStream;
  struct stream* outStream;
  
  if(argc<2){
    fprintf(stderr, "use %s -h or --help for help\n",argv[0]);
    exit(0);
  }
  
  while (1) {
    option_index = 0;
    
    int op = getopt_long  (argc, argv, "hp:o:cdi:tuv:",
		       long_options, &option_index);
    if (op == -1)
      break;

    switch (op){
    case 0: /* long opt */
      break;

      case 'd':
	fprintf(stderr, "Calender date\n");
	args.cDate=1;
	break;
      case 'p':
	fprintf(stderr, "No packets. Argument %s\n", optarg);
	args.max_pkts=atoi(optarg);
	break;
      case 'c':
	fprintf(stderr, "Content printing..\n");
	args.print_content=1;
	break;
      case 'i':
	fprintf(stderr, "Ethernet Argument %s\n", optarg);
	nic=strdup(optarg);
	streamType=1;
	break;
      case 'u':
	fprintf(stderr, "UDP \n");
	streamType=2;
	break;
      case 't':
	fprintf(stderr, "TCP \n");
	streamType=3;
	break;
      case 'v':
	fprintf(stderr, "port %d\n", atoi(optarg));
	portnumber=atoi(optarg);
	break;	
      case 'o':
	fprintf(stderr, "Output to file.\n");
	outFilename=strdup(optarg);
	capOutfile=1;              
	fprintf(stderr, "Output to data file %s\n",outFilename);
	break;	  
      case 'h':
	fprintf(stderr, "-------------------------------------------------------\n");
	fprintf(stderr, "Application Version " VERSION "\n");
	fprintf(stderr, "Application Options\n");
	fprintf(stderr, "-p or --pkts   <NO>     Number of pkts to show [default all]\n");
	fprintf(stderr, "-o or --output <name>   Store results to a CAP file. \n");
	fprintf(stderr, "-d or --calender        Display date/time in YYYY-MM-DD HH:MM:SS.xx.\n");
	fprintf(stderr, "-i or --if <NIC>        Listen to NIC for Ethernet multicast address,\n");
	fprintf(stderr, "                        identified by <INPUT> (01:00:00:00:00:01).\n");
	fprintf(stderr, "-t or --tcp             Listen to a TCP stream.\n");
	fprintf(stderr, "                        identified by <INPUT> (192.168.0.10). \n");
	fprintf(stderr, "-u or --udp             Listen to a UDP multicast address.\n");
	fprintf(stderr, "                        identified by <INPUT> (225.10.11.10).\n");
	fprintf(stderr, "-v or --port            TCP/UDP port to listen to. Default 0x0810.\n");
	fprintf(stderr, "<INPUT>                 If n,t or u hasn't been declared, this \n");
	fprintf(stderr, "                        is interpreted as a filename.\n");
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "%s [filter options] [application options] <INPUT>\n", argv[0]);

	fprintf(stderr, "Sizeof(capture_header) = %zd bytes\n",sizeof(cap_head));
	exit(0);
	break;
      default:
	printf ("?? getopt returned character code 0%o ??\n", op);
    }
  }

  const char* filename = NULL;
  switch (argc - optind){
  case 0:
    switch ( streamType ){
    case 0:
      fprintf(stderr, "filename required\n");
      return 1;
    default:
      filename = "01:00:00:00:00:01";
    }
    break;
  default:
    filename = argv[optind];
  }

  printf("Opening stream %s\n", filename);
  stream_addr_t src;
  stream_addr_aton(&src, filename, streamType, STREAM_ADDR_LOCAL);

  if( (ret=stream_open(&inStream, &src, nic, portnumber)) != 0 ) {
    fprintf(stderr, "stream_open failed with code 0x%08X: %s\n", ret, caputils_error_string(ret));
    return 1;
  }

  if(capOutfile==1) {
    fprintf(stderr, "Creating FILE!\n.");
    stream_addr_t dst;
    stream_addr_str(&dst, outFilename, 0);
    stream_create(&outStream, &dst, NULL, stream_get_mampid(inStream), stream_get_comment(inStream));
    fprintf(stderr, "OK.\n");
  }

  struct file_version version;
  stream_get_version(inStream, &version);

//output fileheader
  fprintf(stderr, "ver: %d.%d id: %s \n comments: %s\n",
	  version.major, 
	  version.minor, 
	  stream_get_mampid(inStream), 
	  stream_get_comment(inStream));

  fprintf(stderr, "myFilter.index = %u \n", myfilter.index);
  fprintf(stderr, "----------------------------\n");

  if ( capOutfile == 1 ){
    ret = clone_stream(outStream, inStream, &myfilter, &pktCount);
  } else {
    ret = display_stream(inStream, &myfilter, &pktCount);
  }
  
  stream_close(inStream);

  if(capOutfile==1) {
    stream_close(outStream);
  }

  filter_close(&myfilter);

  fprintf(stderr, "There was a total of %lld pkts that matched the filter.\n", pktCount);
  return 0;
}
