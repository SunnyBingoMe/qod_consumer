#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "pyconsumer/ethhdr.h"

#include <Python.h>
#include "structmember.h"

struct ethhdr_wrapper {
  PyObject_HEAD
  struct ethhdr head;
};
PyTypeObject ethhdr_type;

ethhdr_wrapper* ethhdr_FromStruct(const struct ethhdr* ethhdr){
  ethhdr_wrapper* obj = PyObject_New(ethhdr_wrapper, &ethhdr_type);
  memcpy(&obj->head, ethhdr, sizeof(struct ethhdr));
  return obj;
}

static PyObject* ethhdr_str(ethhdr_wrapper* wrap){
  struct ethhdr* ethhdr = &wrap->head;
  char h_dest[6*3+1];
  char h_source[6*3+1];
  ether_ntoa_r((struct ether_addr*)ethhdr->h_dest, h_dest);
  ether_ntoa_r((struct ether_addr*)ethhdr->h_source, h_source);

  return PyString_FromFormat(
    "{'h_dest': '%s', 'h_source': '%s', 'h_proto': '%d'}",
    h_dest, h_source, ethhdr->h_proto);
}

static PyObject* ethhdr_h_dest(ethhdr_wrapper* self, PyObject* args, PyObject* kwargs){
  static char *kwlist[] = {
    "convert", NULL
  };

  int convert = 1;
  if (! PyArg_ParseTupleAndKeywords(args, kwargs, "|i", kwlist, &convert) ){
    return NULL;
  }

  if ( convert ){
    char buffer[6*3+1] = {0,};
    ether_ntoa_r((struct ether_addr*)self->head.h_dest, buffer);
    return PyString_FromString(buffer);
  } else {
    return PyByteArray_FromStringAndSize((char*)self->head.h_dest, ETH_ALEN);
  }
}

static PyObject* ethhdr_h_source(ethhdr_wrapper* self, PyObject* args, PyObject* kwargs){
  static char *kwlist[] = {
    "convert", NULL
  };

  int convert = 1;
  if (! PyArg_ParseTupleAndKeywords(args, kwargs, "|i", kwlist, &convert) ){
    return NULL;
  }

  if ( convert ){
    char buffer[6*3+1] = {0,};
    ether_ntoa_r((struct ether_addr*)self->head.h_source, buffer);
    return PyString_FromString(buffer);
  } else {
    return PyByteArray_FromStringAndSize((char*)self->head.h_source, ETH_ALEN);
  }
}

static PyMethodDef ethhdr_methods[] = {
  {"h_dest",   (PyCFunction)ethhdr_h_dest,   METH_VARARGS|METH_KEYWORDS, PyDoc_STR("Ethernet destination")},
  {"h_source", (PyCFunction)ethhdr_h_source, METH_VARARGS|METH_KEYWORDS, PyDoc_STR("Ethernet source")},

  {NULL, NULL, 0, NULL} /* sentinel value */
};

static PyMemberDef ethhdr_members[] = {
  {"h_proto",  T_UINT,   offsetof(ethhdr_wrapper, head) + offsetof(struct ethhdr, h_proto),  READONLY, "Ethernet protocol"},
  {NULL},
};

PyTypeObject ethhdr_type = {
    PyObject_HEAD_INIT(NULL)
    .ob_size = 0,
    .tp_name = "consumer._ethhdr",
    .tp_basicsize = sizeof(ethhdr_wrapper),
    .tp_str = (PyObject* (*)(PyObject*))ethhdr_str,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "ethernet header",
    .tp_methods = ethhdr_methods,
    .tp_members = ethhdr_members,
};
