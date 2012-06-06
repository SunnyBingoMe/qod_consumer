#ifndef PYCONSUMER_H
#define PYCONSUMER_H

#include "../consumer.h"

typedef struct {
  PyObject_HEAD
  consumer_thread_t thread;
} Consumer;

#endif /* PYCONSUMER_ITERATOR_H */
