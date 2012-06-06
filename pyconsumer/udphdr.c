#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "pyconsumer/udphdr.h"

#include <Python.h>
#include "structmember.h"
#include <sys/socket.h>
#include <netinet/udp.h>
#include <arpa/inet.h>

struct udphdr_wrapper {
  PyObject_HEAD
  struct udphdr head;
};
PyTypeObject udphdr_type;

udphdr_wrapper* udphdr_FromStruct(const struct udphdr* udphdr){
  udphdr_wrapper* obj = PyObject_New(udphdr_wrapper, &udphdr_type);
  memcpy(&obj->head, udphdr, sizeof(struct udphdr));

  return obj;
}

static PyObject* udphdr_str(udphdr_wrapper* wrap){
  struct udphdr* udp = &wrap->head;

  return PyString_FromFormat(
    "UDP: {}"
  );
}

static PyMethodDef udphdr_methods[] = {
  {NULL, NULL, 0, NULL} /* sentinel value */
};

static PyMemberDef udphdr_members[] = {
  {NULL},
};

PyTypeObject udphdr_type = {
    PyObject_HEAD_INIT(NULL)
    .ob_size = 0,
    .tp_name = "consumer._udphdr",
    .tp_basicsize = sizeof(udphdr_wrapper),
    .tp_str = (PyObject* (*)(PyObject*))udphdr_str,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "UDP header",
    .tp_methods = udphdr_methods,
    .tp_members = udphdr_members,
};
