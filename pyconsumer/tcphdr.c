#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "pyconsumer/tcphdr.h"

#include <Python.h>
#include "structmember.h"
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

struct tcphdr_wrapper {
  PyObject_HEAD
  struct tcphdr head;
};
PyTypeObject tcphdr_type;

tcphdr_wrapper* tcphdr_FromStruct(const struct tcphdr* tcphdr){
  tcphdr_wrapper* obj = PyObject_New(tcphdr_wrapper, &tcphdr_type);
  memcpy(&obj->head, tcphdr, sizeof(struct tcphdr));

  return obj;
}

static PyObject* tcphdr_str(tcphdr_wrapper* wrap){
  struct tcphdr* tcp = &wrap->head;

  return PyString_FromFormat(
    "TCP: {'source': %d, 'dest': %d, 'seq': %d, flags:%s%s%s%s%s%s}",
    ntohs(tcp->source), ntohs(tcp->dest),
    ntohl(tcp->seq),
    tcp->urg?" URG":"", tcp->ack?" ACK":"", tcp->psh?" PSH":"", tcp->rst?" RST":"", tcp->syn?" SYN":"", tcp->fin?" FIN":""
  );
}

static PyMethodDef tcphdr_methods[] = {
  {NULL, NULL, 0, NULL} /* sentinel value */
};

static PyMemberDef tcphdr_members[] = {
  {"source",  T_USHORT,   offsetof(tcphdr_wrapper, head) + offsetof(struct tcphdr, source), READONLY, ""},
  {"dest",    T_USHORT,   offsetof(tcphdr_wrapper, head) + offsetof(struct tcphdr, dest), READONLY, ""},
  {"seq",     T_UINT,     offsetof(tcphdr_wrapper, head) + offsetof(struct tcphdr, seq), READONLY, ""},
  {NULL},
};

PyTypeObject tcphdr_type = {
    PyObject_HEAD_INIT(NULL)
    .ob_size = 0,
    .tp_name = "consumer._tcphdr",
    .tp_basicsize = sizeof(tcphdr_wrapper),
    .tp_str = (PyObject* (*)(PyObject*))tcphdr_str,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "TCP header",
    .tp_methods = tcphdr_methods,
    .tp_members = tcphdr_members,
};
