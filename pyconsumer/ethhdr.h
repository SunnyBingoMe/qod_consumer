#ifndef PYCONSUMER_ETHHDR_H
#define PYCONSUMER_ETHHDR_H

#include <Python.h>
#include <netinet/ether.h>

typedef struct ethhdr_wrapper ethhdr_wrapper;
PyTypeObject ethhdr_type;
ethhdr_wrapper* ethhdr_FromStruct(const struct ethhdr* ethhdr);

#endif /* PYCONSUMER_ETHHDR_H */
