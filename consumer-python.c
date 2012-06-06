#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <Python.h>
#include "structmember.h"
#include "datetime.h"

#include "consumer.h"
#include "pyconsumer/ethhdr.h"
#include "pyconsumer/iphdr.h"
#include "pyconsumer/tcphdr.h"
#include "pyconsumer/udphdr.h"
#include "pyconsumer/icmphdr.h"
#include "pyconsumer/frame.h"
#include "pyconsumer/iterator.h"
#include "pyconsumer/packet.h"
#include <netinet/ether.h>

static int consumer_init(Consumer* self, PyObject *args, PyObject *kwds){
  static char *kwlist[] = {"packets", "delay", NULL};
  
  long int bufsize = 1000;
  float raw_delay = 0.0;
  if (! PyArg_ParseTupleAndKeywords(args, kwds, "|if", kwlist, &bufsize, &raw_delay) ){
    return -1;
  }

  timepico delay;
  delay.tv_sec = floor(raw_delay);
  delay.tv_psec = ((int)(raw_delay - floor(raw_delay))) * 1000000000000;

  int ret;
  if ( (ret=consumer_thread_init(&self->thread, bufsize, &delay)) != 0 ){
    PyErr_SetString(PyExc_RuntimeError, strerror(ret));
    return -1;
  }

  return 0;
}

static PyObject* consumer_add_stream(Consumer* self, PyObject* args, PyObject* kwargs){
  static char *kwlist[] = {
    "src", "type",
    "iface", /* ethernet */
    "port",  /* tcp/udp */
    "filter",
    NULL
  };
  
  char* src = NULL;
  int type = -1;
  char* iface = NULL;
  int port = -1;
  PyObject* filter = NULL;
  if (! PyArg_ParseTupleAndKeywords(args, kwargs, "si|sio", kwlist, &src, &type, &iface, &port, &filter) ){
    return NULL;
  }

  stream_addr_t dest;
  stream_addr_aton(&dest, src, type, STREAM_ADDR_LOCAL);
  
  switch ( type ){
  case PROTOCOL_LOCAL_FILE:
    /* no additional arguments */
    break;
  case PROTOCOL_ETHERNET_MULTICAST:
    if ( !iface ){
      PyErr_SetString(PyExc_TypeError, "Required argument 'iface' not found");
      return NULL;
    }
    break;
  case PROTOCOL_UDP_MULTICAST:
  case PROTOCOL_TCP_UNICAST:
    if ( !port ){
      PyErr_SetString(PyExc_TypeError, "Required argument 'port' not found");
      return NULL;
    }
    break;
  default:
    PyErr_Format(PyExc_ValueError, "Invalid stream type given (%d)", type);
    return NULL;
  }

  long ret;
  if ( (ret=consumer_thread_add_stream(self->thread, &dest, iface, port, NULL)) != 0 ){
    PyErr_SetString(PyExc_RuntimeError, caputils_error_string(ret));
    return NULL;
  }

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* consumer_poll(Consumer* self, PyObject* args, PyObject* kwargs){
  static char *kwlist[] = {
    "timeout", NULL
  };

  float timeout = 0.0;
  if (! PyArg_ParseTupleAndKeywords(args, kwargs, "|f", kwlist, &timeout) ){
    return NULL;
  }

  const int ms = (int)(timeout * 1000);

  /* no timeout and no packets */
  if ( ms == 0 && consumer_thread_pending(self->thread) == 0 ){
    return Py_BuildValue("(s,s)", NULL, NULL);
  }



  packet_wrapper* pw = PyObject_New(packet_wrapper, &packet_type);

  /* try to read packet */
  int result;

  Py_BEGIN_ALLOW_THREADS;
  {
    memset(&pw->vlan_tci, 0, sizeof(packet_wrapper) - sizeof(struct packet)); /* reset all fields, but leave actual packet data alone (for performance) */
    result = consumer_thread_poll(self->thread, &pw->pkt, ms);
  }
  Py_END_ALLOW_THREADS;

  if ( !result ){
    Py_DECREF(pw);
    return Py_BuildValue("(s,s)", NULL, NULL);
  }

  packet_wrapper_init(pw);
  return Py_BuildValue("(i,O)", pw->pkt.stream_id, pw);
}

static PyObject* consumer_iter(Consumer* self){
  return (PyObject*)iterator_new(self);
}

static PyMethodDef Consumer_methods[] = {
  {"add_stream", (PyCFunction)consumer_add_stream, METH_VARARGS|METH_KEYWORDS, PyDoc_STR("add stream")},
  {"poll", (PyCFunction)consumer_poll, METH_VARARGS|METH_KEYWORDS, PyDoc_STR("Poll for incomming packages (from packet buffer, with delay)")},
  {NULL, NULL},
};

static PyTypeObject consumer_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "consumer.Consumer",       /*tp_name*/
    sizeof(Consumer),          /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    0,                         /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,        /*tp_flags*/
    "consumer thread wrapper", /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    (getiterfunc)consumer_iter,             /* tp_iter */
    0,                         /* tp_iternext */
    Consumer_methods,          /* tp_methods */
    0,             /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)consumer_init,   /* tp_init */
    0,                         /* tp_alloc */
    0,                 /* tp_new */
};

static PyMethodDef methods[] = {
  {NULL, NULL, 0, NULL} /* sentinel value */
};

void packet_init(); /* *sigh* must exec PyDateTime_IMPORT for each translation unit as it is static */

PyMODINIT_FUNC initconsumer(void){
  PyDateTime_IMPORT;
  packet_init();

  PyObject* m = Py_InitModule3("consumer", methods, "Python binding for NPL consumer");
  if ( !m ){
    return;
  }

#define INIT_TYPE(TYPE, NAME)			\
  do {						\
    (TYPE).tp_new = PyType_GenericNew;		\
    if ( PyType_Ready(&(TYPE)) < 0){		\
      return;					\
    }						\
    						\
    Py_INCREF(&(TYPE));							\
    PyModule_AddObject(m, NAME, (PyObject *)&(TYPE));			\
  } while (0)
  
  consumer_Type.tp_new = PyType_GenericNew;
  if ( PyType_Ready(&consumer_Type) < 0){
    return;
  }

  frame_type.tp_new = PyType_GenericNew;
  if ( PyType_Ready(&frame_type) < 0){
    return;
  }

  INIT_TYPE(ethhdr_type, "_ethhdr");
  INIT_TYPE(iphdr_type, "_iphdr");
  INIT_TYPE(tcphdr_type, "_tcphdr");
  INIT_TYPE(udphdr_type, "_udphdr");
  INIT_TYPE(icmphdr_type, "_icmpphdr");
  INIT_TYPE(iterator_type, "_buffer_iterator");
  INIT_TYPE(packet_type, "_packet_wrapper");

  Py_INCREF(&consumer_Type);
  PyModule_AddObject(m, "Consumer", (PyObject *)&consumer_Type);

  Py_INCREF(&frame_type);
  PyModule_AddObject(m, "Frame", (PyObject *)&frame_type);

  PyModule_AddIntConstant(m, "FRAME_PACKET_IP", PACKET_IP);
  PyModule_AddIntConstant(m, "FRAME_PACKET_ICMP", PACKET_ICMP);
  PyModule_AddIntConstant(m, "FRAME_PACKET_ARP", PACKET_ARP);
  PyModule_AddIntConstant(m, "FRAME_TRANSPORT_TCP", TRANSPORT_TCP);
  PyModule_AddIntConstant(m, "FRAME_TRANSPORT_UDP", TRANSPORT_UDP);
  PyModule_AddIntConstant(m, "SOURCE_FILE", PROTOCOL_LOCAL_FILE);
  PyModule_AddIntConstant(m, "SOURCE_ETHERNET", PROTOCOL_ETHERNET_MULTICAST);
  PyModule_AddIntConstant(m, "SOURCE_UDP", PROTOCOL_UDP_MULTICAST);
  PyModule_AddIntConstant(m, "SOURCE_TCP", PROTOCOL_TCP_UNICAST);

  PyModule_AddIntConstant(m, "IP_RF", IP_RF);
  PyModule_AddIntConstant(m, "IP_DF", IP_DF);
  PyModule_AddIntConstant(m, "IP_MF", IP_MF);
  PyModule_AddIntConstant(m, "IP_OFFMASK", IP_OFFMASK);
}
