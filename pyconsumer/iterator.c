#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "pyconsumer/iterator.h"
#include "pyconsumer/packet.h"

#include <Python.h>
#include "structmember.h"
#include <pthread.h>

buffer_iterator* iterator_new(Consumer* consumer){
  buffer_iterator* it = PyObject_New(buffer_iterator, &iterator_type);

  Py_INCREF(consumer);
  it->consumer = consumer;
  it->index = 0;

  return it;
}

static void iterator_dealloc(buffer_iterator* self){
  Py_DECREF(self->consumer);
  iterator_type.tp_free(self);
}

static PyObject* iterator_next(buffer_iterator* self){
  packet_wrapper* pw = NULL;
  consumer_thread_t con = self->consumer->thread;

  consumer_lock(con);
  {
    struct packet* pkt = consumer_buffer_get(con, self->index++);

    /* no more packages in buffer */
    if ( !pkt->caphead.caplen ){
      consumer_unlock(con);
      PyErr_SetNone(PyExc_StopIteration);
      return NULL;
    }

    /* memory will be copied */
     pw = packet_wrapper_new(pkt);
  }
  consumer_unlock(con);

  return (PyObject*)pw;
}

PyTypeObject iterator_type = {
    PyObject_HEAD_INIT(NULL)
    .ob_size = 0,
    .tp_name = "consumer._iterator",
    .tp_basicsize = sizeof(buffer_iterator),
    .tp_dealloc = (destructor)iterator_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_HAVE_ITER,
    .tp_iternext = (iternextfunc)iterator_next
};
