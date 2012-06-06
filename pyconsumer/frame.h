#ifndef PYCONSUMER_FRAME_H
#define PYCONSUMER_FRAME_H

#include <Python.h>
#include "../consumer.h"
#include "pyconsumer/iphdr.h"
#include "pyconsumer/ethhdr.h"
#include "pyconsumer/tcphdr.h"
#include "pyconsumer/udphdr.h"

typedef struct {
  PyObject_HEAD
  struct cap_header head;
  struct frame_t frame;

  PyObject* timestamp;
  PyObject* iface; /* from caphead */
  PyObject* mampid; /* from caphead */
  ethhdr_wrapper* ethhdr;
  iphdr_wrapper* iphdr;
  tcphdr_wrapper* tcphdr;
  udphdr_wrapper* udphdr;
} frame_wrapper;

PyTypeObject frame_type;

frame_wrapper* frame_wrapper_new(struct cap_header* head, struct frame_t* frame);
void frame_wrapper_init(frame_wrapper* fw, struct cap_header* head, struct frame_t* frame);

#endif /* PYCONSUMER_ITERATOR_H */
