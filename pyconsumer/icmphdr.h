#ifndef PYCONSUMER_ICMPUDR_H
#define PYCONSUMER_ICMPHDR_H

#include <Python.h>
#include <netinet/ip_icmp.h>

typedef struct icmphdr_wrapper icmphdr_wrapper;
PyTypeObject icmphdr_type;
icmphdr_wrapper* icmphdr_FromStruct(const struct icmphdr* icmphdr);

#endif /* PYCONSUMER_ICMPHDR_H */
