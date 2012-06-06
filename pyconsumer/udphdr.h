#ifndef PYCONSUMER_UDPHDR_H
#define PYCONSUMER_UDPHDR_H

#include <Python.h>
#include <netinet/udp.h>

typedef struct udphdr_wrapper udphdr_wrapper;
PyTypeObject udphdr_type;
udphdr_wrapper* udphdr_FromStruct(const struct udphdr* udphdr);

#endif /* PYCONSUMER_UDPHDR_H */
