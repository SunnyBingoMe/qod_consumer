#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "pyconsumer/iphdr.h"

#include <Python.h>
#include "structmember.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

struct iphdr_wrapper {
  PyObject_HEAD
  unsigned int ip_hl;
  unsigned int ip_v;
  struct ip head;
};
PyTypeObject iphdr_type;

iphdr_wrapper* iphdr_FromStruct(const struct ip* iphdr){
  iphdr_wrapper* obj = PyObject_New(iphdr_wrapper, &iphdr_type);
  memcpy(&obj->head, iphdr, sizeof(struct ip));

  /* copy these to make it easier to acces the bit-field members from python */
  obj->ip_hl = iphdr->ip_hl;
  obj->ip_v  = iphdr->ip_v;

  return obj;
}

static PyObject* iphdr_str(iphdr_wrapper* wrap){
  struct ip* ip = &wrap->head;

  char ip_src[4*3+3+1]; /* 4 parts with at most 3 digits, 3 dots inbetween, and a null terminator */
  char ip_dst[4*3+3+1];
  strcpy(ip_src, inet_ntoa(ip->ip_src));
  strcpy(ip_dst, inet_ntoa(ip->ip_dst));

  return PyString_FromFormat(
    "IP: {'length': %d, 'version': 'IPV%d', 'ip_tos': %d, 'ip_len': %d, 'ip_id': %d, 'ip_off': %d, 'ip_ttl': %d, 'ip_p': %d, 'ip_sum': %d, 'src': '%s', 'dst': '%s'}",
    ip->ip_hl, ip->ip_v,
    ip->ip_tos, ip->ip_len, ip->ip_id, ip->ip_off, ip->ip_ttl, ip->ip_p, ip->ip_sum,
    ip_src, ip_dst
  );
}

static PyObject* iphdr_src(iphdr_wrapper* self, PyObject* args, PyObject* kwargs){
  struct ip* ip = &self->head;
  static char *kwlist[] = {
    "convert", NULL
  };

  int convert = 1;
  if (! PyArg_ParseTupleAndKeywords(args, kwargs, "|i", kwlist, &convert) ){
    return NULL;
  }

  if ( convert ){
    char buffer[4*3+3+1] = {0,};
    strcpy(buffer, inet_ntoa(ip->ip_src));
    return PyString_FromString(buffer);
  } else {
    return PyByteArray_FromStringAndSize((char*)&ip->ip_src, 4);
  }
}

static PyObject* iphdr_dst(iphdr_wrapper* self, PyObject* args, PyObject* kwargs){
  struct ip* ip = &self->head;
  static char *kwlist[] = {
    "convert", NULL
  };

  int convert = 1;
  if (! PyArg_ParseTupleAndKeywords(args, kwargs, "|i", kwlist, &convert) ){
    return NULL;
  }

  if ( convert ){
    char buffer[4*3+3+1] = {0,};
    strcpy(buffer, inet_ntoa(ip->ip_dst));
    return PyString_FromString(buffer);
  } else {
    return PyByteArray_FromStringAndSize((char*)&ip->ip_dst, 4);
  }
}

static PyMethodDef iphdr_methods[] = {
  {"src", (PyCFunction)iphdr_src, METH_VARARGS|METH_KEYWORDS, PyDoc_STR("IP source")},
  {"dst", (PyCFunction)iphdr_dst, METH_VARARGS|METH_KEYWORDS, PyDoc_STR("IP destination")},
  {NULL, NULL, 0, NULL} /* sentinel value */
};

static PyMemberDef iphdr_members[] = {
  {"ip_hl",  T_UINT,   offsetof(iphdr_wrapper, ip_hl), READONLY, "IP header length"},
  {"ip_v",   T_UINT,   offsetof(iphdr_wrapper, ip_v),  READONLY, "IP version"},
  {"ip_tos", T_UBYTE,  offsetof(iphdr_wrapper, head) + offsetof(struct ip, ip_tos), READONLY, "Type Of Service"},
  {"ip_len", T_USHORT, offsetof(iphdr_wrapper, head) + offsetof(struct ip, ip_len), READONLY, "Total length"},
  {"ip_id",  T_USHORT, offsetof(iphdr_wrapper, head) + offsetof(struct ip, ip_id),  READONLY, "Identification"},
  {"ip_off", T_UBYTE,  offsetof(iphdr_wrapper, head) + offsetof(struct ip, ip_off), READONLY, "Fragment offset field"},
  {"ip_ttl", T_UBYTE,  offsetof(iphdr_wrapper, head) + offsetof(struct ip, ip_ttl), READONLY, "Time To Live"},
  {"ip_p",   T_UBYTE,  offsetof(iphdr_wrapper, head) + offsetof(struct ip, ip_p),   READONLY, "Protocol"},
  {"ip_sum", T_USHORT, offsetof(iphdr_wrapper, head) + offsetof(struct ip, ip_sum), READONLY, "Checksum"},
  {NULL},
};

PyTypeObject iphdr_type = {
    PyObject_HEAD_INIT(NULL)
    .ob_size = 0,
    .tp_name = "consumer._iphdr",
    .tp_basicsize = sizeof(iphdr_wrapper),
    .tp_str = (PyObject* (*)(PyObject*))iphdr_str,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "IP header",
    .tp_methods = iphdr_methods,
    .tp_members = iphdr_members,
};
