#ifndef PYCONSUMER_ITERATOR_H
#define PYCONSUMER_ITERATOR_H

#include <Python.h>
#include "pyconsumer/consumer.h"

typedef struct {
  PyObject_HEAD
  Consumer* consumer;
  unsigned int index;
} buffer_iterator;

PyTypeObject iterator_type;

buffer_iterator* iterator_new(Consumer* consumer);

#endif /* PYCONSUMER_ITERATOR_H */
