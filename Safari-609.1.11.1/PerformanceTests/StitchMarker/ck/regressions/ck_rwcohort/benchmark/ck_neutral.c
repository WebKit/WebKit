#include "../ck_neutral.h"

#ifdef THROUGHPUT
#include "throughput.h"
#elif defined(LATENCY)
#include "latency.h"
#endif
