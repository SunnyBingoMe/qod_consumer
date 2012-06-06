#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "pyconsumer/icmphdr.h"

#include <Python.h>
#include "structmember.h"
#include <netinet/ip_icmp.h>

struct icmphdr_wrapper {
  PyObject_HEAD
  struct icmphdr head;
};
PyTypeObject icmphdr_type;

icmphdr_wrapper* icmphdr_FromStruct(const struct icmphdr* icmphdr){
  icmphdr_wrapper* obj = PyObject_New(icmphdr_wrapper, &icmphdr_type);
  memcpy(&obj->head, icmphdr, sizeof(struct icmphdr));

  return obj;
}

static PyObject* icmphdr_str(icmphdr_wrapper* wrap){
  struct icmphdr* icmp = &wrap->head;

  return PyString_FromFormat(
    "ICMP: {}"
  );
}

static PyMethodDef icmphdr_methods[] = {
  {NULL, NULL, 0, NULL} /* sentinel value */
};

static PyMemberDef icmphdr_members[] = {
  {NULL},
};

PyTypeObject icmphdr_type = {
    PyObject_HEAD_INIT(NULL)
    .ob_size = 0,
    .tp_name = "consumer._icmphdr",
    .tp_basicsize = sizeof(icmphdr_wrapper),
    .tp_str = (PyObject* (*)(PyObject*))icmphdr_str,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "ICMP header",
    .tp_methods = icmphdr_methods,
    .tp_members = icmphdr_members,
};
