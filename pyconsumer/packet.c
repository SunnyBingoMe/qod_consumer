#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <Python.h>
#include "pyconsumer/packet.h"
#include "pyconsumer/tcphdr.h"
#include "pyconsumer/udphdr.h"
#include "pyconsumer/icmphdr.h"
#include "structmember.h"
#include "datetime.h"

static void init_ipv4(packet_wrapper* pw, const struct ip* ip){
  void* payload = ((char*)ip) + 4*ip->ip_hl;
  void* data = NULL;

  pw->ipv4 = (PyObject*)iphdr_FromStruct(ip);

  switch( ip->ip_p ) {
  case IPPROTO_TCP:
    pw->tcphdr = (PyObject*)tcphdr_FromStruct((const struct tcphdr*)payload);
    data = payload + ((const struct tcphdr*)payload)->doff * 4;
    break;
    
  case IPPROTO_UDP:
    pw->udphdr = (PyObject*)udphdr_FromStruct((const struct udphdr*)payload);
    break;

  case IPPROTO_ICMP:
    pw->icmphdr = (PyObject*)icmphdr_FromStruct((const struct icmphdr*)payload);
    break;
  }

  if ( data ){
    pw->data = PyBuffer_FromMemory(data, pw->pkt.caphead.caplen - (data - (void*)pw->pkt.buf));
  }
}

static void init_eth(packet_wrapper* pw, const struct ethhdr* eth){
  void* payload = ((char*)eth) + sizeof(struct ethhdr);
  uint16_t h_proto = ntohs(eth->h_proto);

  pw->ethhdr = (PyObject*)ethhdr_FromStruct(eth);
  
 begin:
  switch ( h_proto ){
  case ETHERTYPE_VLAN:
    pw->vlan_tci = ((uint16_t*)payload)[0];
    h_proto = ntohs(((uint16_t*)payload)[0]);
    payload = ((char*)eth) + sizeof(struct ethhdr);
    /* @todo setup access to vlan header */
    goto begin;

  case ETHERTYPE_IP:
    init_ipv4(pw, (struct ip*)payload);
    break;

  case ETHERTYPE_IPV6:
    Py_INCREF(Py_True);
    pw->ipv6 = Py_True;
    break;
   
  case ETHERTYPE_ARP:
    /* @todo setup wrapper */
    Py_INCREF(Py_True);
    pw->arp_header = Py_True;
    break;

  case 0x0810: /* MP packet, ignore */
    return;

  default:
    printf("Unknown ethernet protocol (0x%04x)\n", h_proto);
  }
}

static void init_timestamp(packet_wrapper* pw, const struct cap_header* caphead){
  time_t time = caphead->ts.tv_sec;
  struct tm* tm = gmtime(&time);
  pw->timestamp = PyDateTime_FromDateAndTime(
    tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
    tm->tm_hour, tm->tm_min, tm->tm_sec,
    caphead->ts.tv_psec
  );  
}

void packet_wrapper_init(packet_wrapper* pw){
  struct cap_header* caphead = &pw->pkt.caphead;
  init_timestamp(pw, caphead);

  pw->iface = PyString_FromFormat("%8s", caphead->nic);
  pw->mampid = PyString_FromFormat("%8s", caphead->mampid);
  pw->ethhdr = NULL;
  pw->ipv4 = NULL;
  pw->ipv6 = NULL;
  pw->tcphdr = NULL;
  pw->udphdr = NULL;
  pw->arp_header = NULL;
  pw->arp_req = NULL;
  pw->icmphdr = NULL;
  pw->raw = PyBuffer_FromMemory(caphead, sizeof(struct cap_header) + caphead->caplen);
  pw->data = NULL;

  init_eth(pw, caphead->ethhdr);

#define NONE_if_unset(x) if ( !x ){ Py_INCREF(Py_None); x = Py_None; }

  NONE_if_unset(pw->ethhdr);
  NONE_if_unset(pw->ipv4);
  NONE_if_unset(pw->ipv6);
  NONE_if_unset(pw->tcphdr);
  NONE_if_unset(pw->udphdr);
  NONE_if_unset(pw->arp_header);
  NONE_if_unset(pw->arp_req);
  NONE_if_unset(pw->icmphdr);
  NONE_if_unset(pw->raw);
  NONE_if_unset(pw->data);
}

packet_wrapper* packet_wrapper_new(struct packet* packet){
  packet_wrapper* pw = PyObject_New(packet_wrapper, &packet_type);

  memcpy(&pw->pkt, packet, sizeof(struct packet));
  packet_wrapper_init(pw);

  return pw;
}

static void __del__(packet_wrapper* pw){
  if ( pw->timestamp ) Py_DECREF(pw->timestamp);
  if ( pw->iface     ) Py_DECREF(pw->iface);
  if ( pw->mampid    ) Py_DECREF(pw->mampid);
  if ( pw->ethhdr    ) Py_DECREF(pw->ethhdr);
  if ( pw->ipv4      ) Py_DECREF(pw->ipv4);
  if ( pw->ipv6      ) Py_DECREF(pw->ipv6);
  if ( pw->tcphdr    ) Py_DECREF(pw->tcphdr);
  if ( pw->udphdr    ) Py_DECREF(pw->udphdr);
  if ( pw->arp_header) Py_DECREF(pw->arp_header);
  if ( pw->arp_req   ) Py_DECREF(pw->arp_req);
  if ( pw->icmphdr   ) Py_DECREF(pw->icmphdr);
  if ( pw->raw       ) Py_DECREF(pw->raw);
  if ( pw->data      ) Py_DECREF(pw->data);
  packet_type.tp_free(pw);
}

static PyObject* __str__(const packet_wrapper* pw){
  const struct packet* pkt = &pw->pkt;
  char* buf = NULL;
  size_t len = 0;

  FILE* fp = open_memstream(&buf, &len);
  {
    fprintf(fp, "[%d]:", pkt->packet_id);
    fprintf(fp, "%.4s:%.8s:", pkt->caphead.nic, pkt->caphead.mampid);
    fprintf(fp, "%u.%012lu:", pkt->caphead.ts.tv_sec, pkt->caphead.ts.tv_psec);
    fprintf(fp, "LINK(%4d):CAPLEN(%4d):", pkt->caphead.len, pkt->caphead.caplen);
    //print_frame(fp, &pkt->frame, 0);
  }
  fclose(fp);

  PyObject* str = PyString_FromString(buf);
  free(buf);
  return str;
}

#define offset_caphead offsetof(packet_wrapper, pkt) + offsetof(struct packet, caphead)

static PyMemberDef members[] = {
  {"timestamp", T_OBJECT_EX, offsetof(packet_wrapper, timestamp), READONLY, "Actual arrival time"},
  {"tv_sec",    T_UINT,      offset_caphead + offsetof(struct cap_header, ts) + offsetof(struct picotime, tv_sec),  READONLY, ""},
  {"tv_psec",   T_ULONGLONG, offset_caphead + offsetof(struct cap_header, ts) + offsetof(struct picotime, tv_psec), READONLY, ""},

  {"len",       T_UINT,      offset_caphead + offsetof(struct cap_header, len), READONLY, "Frame full length"},
  {"caplen",    T_UINT,      offset_caphead + offsetof(struct cap_header, caplen), READONLY, "Captured bytes"},
  {"iface",     T_OBJECT_EX, offsetof(packet_wrapper, iface), READONLY, "Capture interface"},
  {"mampid",    T_OBJECT_EX, offsetof(packet_wrapper, mampid), READONLY, "Capture MAMPid"},

  //{"type",    T_INT,       offsetof(packet_wrapper, frame) + offsetof(struct packet_t, type), READONLY, "Frame type"},
  {"ethhdr",    T_OBJECT_EX, offsetof(packet_wrapper, ethhdr), READONLY, "Ethernet header"},
  {"ipv4",      T_OBJECT_EX, offsetof(packet_wrapper, ipv4),  READONLY, "IP header"},
  {"ipv6",      T_OBJECT_EX, offsetof(packet_wrapper, ipv6),  READONLY, "IP header"},
  {"tcphdr",    T_OBJECT_EX, offsetof(packet_wrapper, tcphdr), READONLY, "TCP header"},
  {"udphdr",    T_OBJECT_EX, offsetof(packet_wrapper, udphdr), READONLY, "UDP header"},
  {"icmphdr",   T_OBJECT_EX, offsetof(packet_wrapper, icmphdr), READONLY, "ICMP header"},
  {"arp_header", T_OBJECT_EX, offsetof(packet_wrapper, arp_header), READONLY, "ARP"},

  {"raw",       T_OBJECT_EX, offsetof(packet_wrapper, raw), READONLY, "raw access to packet"},
  {"payload",   T_OBJECT_EX, offsetof(packet_wrapper, data), READONLY, "packet data after headers"},
  {NULL},
};

PyTypeObject packet_type = {
    PyObject_HEAD_INIT(NULL)
    .ob_size = 0,
    .tp_name = "consumer._packet_wrapper",
    .tp_basicsize = sizeof(packet_wrapper),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_dealloc = (destructor)__del__,
    .tp_str = (reprfunc)__str__,
    .tp_members = members
};

void packet_init(){
  PyDateTime_IMPORT;
}
