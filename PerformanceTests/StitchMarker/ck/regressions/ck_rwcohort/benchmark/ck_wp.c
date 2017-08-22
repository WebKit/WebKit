#include "../ck_wp.h"

#ifdef THROUGHPUT
#include "throughput.h"
#elif defined(LATENCY)
#include "latency.h"
#endif
