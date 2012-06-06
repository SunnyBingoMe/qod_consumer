#ifndef PYCONSUMER_TCPHDR_H
#define PYCONSUMER_TCPHDR_H

#include <Python.h>
#include <netinet/tcp.h>

typedef struct tcphdr_wrapper tcphdr_wrapper;
PyTypeObject tcphdr_type;
tcphdr_wrapper* tcphdr_FromStruct(const struct tcphdr* tcphdr);

#endif /* PYCONSUMER_TCPHDR_H */
