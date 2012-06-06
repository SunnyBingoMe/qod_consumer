#ifndef PYCONSUMER_PACKET_H
#define PYCONSUMER_PACKET_H

#include <Python.h>
#include "../consumer.h"
#include "pyconsumer/ethhdr.h"
#include "pyconsumer/iphdr.h"

typedef struct {
  PyObject_HEAD

  /* accessors */
  uint16_t vlan_tci;
  PyObject* timestamp;
  PyObject* iface;
  PyObject* mampid;
  PyObject* ethhdr;
  PyObject* ipv4;
  PyObject* ipv6;
  PyObject* tcphdr;
  PyObject* udphdr;
  PyObject* arp_header;
  PyObject* arp_req;
  PyObject* icmphdr;
  PyObject* raw;  /* raw access to payload */
  PyObject* data; /* data after headers */

  /* actual packet */
  struct packet pkt;
} packet_wrapper;

PyTypeObject packet_type;

packet_wrapper* packet_wrapper_new(struct packet* packet);
void packet_wrapper_init(packet_wrapper* pw);

#endif /* PYCONSUMER_ITERATOR_H */
