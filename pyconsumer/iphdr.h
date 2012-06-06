#ifndef PYCONSUMER_IPHDR_H
#define PYCONSUMER_IPHDR_H

#include <Python.h>
#include <netinet/ip.h>

typedef struct iphdr_wrapper iphdr_wrapper;
PyTypeObject iphdr_type;
iphdr_wrapper* iphdr_FromStruct(const struct ip* iphdr);

#endif /* PYCONSUMER_IPHDR_H */
