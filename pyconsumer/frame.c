#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "pyconsumer/frame.h"

#include <Python.h>
#include "structmember.h"
#include "datetime.h"

frame_wrapper* frame_wrapper_new(struct cap_header* head, struct frame_t* frame){
  frame_wrapper* fw = PyObject_New(frame_wrapper, &frame_type);
  frame_wrapper_init(fw, head, frame);

  return fw;
}

void frame_wrapper_init(frame_wrapper* fw, struct cap_header* head, struct frame_t* frame){
  memcpy(&fw->head, head, sizeof(struct cap_header));
  memcpy(&fw->frame, frame, sizeof(struct frame_t));

  {
    time_t time = head->ts.tv_sec;
    struct tm* tm = gmtime(&time);
    fw->timestamp = PyDateTime_FromDateAndTime(
      tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
      tm->tm_hour, tm->tm_min, tm->tm_sec,
      head->ts.tv_psec / 1000
    );
  }

  fw->iface = PyString_FromFormat("%8s", head->nic);
  fw->mampid = PyString_FromFormat("%8s", head->mampid);

  fw->ethhdr = ethhdr_FromStruct(frame->eth);

  fw->iphdr = NULL;
  if ( frame->type & PACKET_IP ){
    fw->iphdr = iphdr_FromStruct(frame->ip);

    fw->tcphdr = NULL;
    if ( frame->type & TRANSPORT_TCP ){
      fw->tcphdr = tcphdr_FromStruct(frame->tcp);
    }

    fw->udphdr = NULL;
    if ( frame->type & TRANSPORT_UDP ){
      fw->udphdr = udphdr_FromStruct(frame->udp);
    }
  }

}

PyObject* frame_str(frame_wrapper* frame){
  static unsigned long long int counter = 1;
  char* buf = NULL;
  size_t len = 0;
  FILE* fp = open_memstream(&buf, &len);
  fprintf(fp, "[%llu]:", counter++);
  fprintf(fp, "%.4s:%.8s:", frame->head.nic, frame->head.mampid);
  fprintf(fp, "%u.%012lu:", frame->head.ts.tv_sec, frame->head.ts.tv_psec);
  fprintf(fp, "LINK(%4d):CAPLEN(%4d):", frame->head.len, frame->head.caplen);
  print_frame(fp, &frame->frame, 0);
  fclose(fp);
  PyObject* str = PyString_FromString(buf);
  free(buf);
  return str;
}

static PyMethodDef frame_methods[] = {
  {NULL, NULL},
};

static PyMemberDef frame_members[] = {
  {"timestamp", T_OBJECT_EX, offsetof(frame_wrapper, timestamp), READONLY, "Actual arrival time"},
  {"tv_sec",  T_UINT,     offsetof(frame_wrapper, head) + offsetof(struct cap_header, ts) + offsetof(struct picotime, tv_sec),  READONLY, ""},
  {"tv_psec", T_ULONGLONG, offsetof(frame_wrapper, head) + offsetof(struct cap_header, ts) + offsetof(struct picotime, tv_psec), READONLY, ""},

  {"len", T_UINT, offsetof(frame_wrapper, head) + offsetof(struct cap_header, len), READONLY, "Frame full length"},
  {"caplen", T_UINT, offsetof(frame_wrapper, head) + offsetof(struct cap_header, caplen), READONLY, "Captured bytes"},
  {"iface", T_OBJECT_EX, offsetof(frame_wrapper, iface), READONLY, "Capture interface"},
  {"mampid", T_OBJECT_EX, offsetof(frame_wrapper, mampid), READONLY, "Capture MAMPid"},

  {"type",   T_INT,       offsetof(frame_wrapper, frame) + offsetof(struct frame_t, type), READONLY, "Frame type"},
  {"ethhdr", T_OBJECT_EX, offsetof(frame_wrapper, ethhdr), READONLY, "Ethernet header"},
  {"ip",     T_OBJECT_EX, offsetof(frame_wrapper, iphdr),  READONLY, "IP header"},
  {"tcp",    T_OBJECT_EX, offsetof(frame_wrapper, tcphdr), READONLY, "TCP header"},
  {"udp",    T_OBJECT_EX, offsetof(frame_wrapper, udphdr), READONLY, "UDP header"},
  {NULL},
};

PyTypeObject frame_type = {
    PyObject_HEAD_INIT(NULL)
    .ob_size = 0,
    .tp_name = "consumer.Frame",
    .tp_doc = "ethernet frame",
    .tp_basicsize = sizeof(frame_wrapper),
    .tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_HAVE_ITER,
    .tp_str = (reprfunc)frame_str,
    .tp_methods = frame_methods,
    .tp_members = frame_members
};
