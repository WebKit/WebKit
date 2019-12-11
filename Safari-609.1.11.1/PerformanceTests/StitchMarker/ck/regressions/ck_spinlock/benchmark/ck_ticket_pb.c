#include "../ck_ticket_pb.h"

#ifdef THROUGHPUT
#include "throughput.h"
#elif defined(LATENCY)
#include "latency.h"
#endif
