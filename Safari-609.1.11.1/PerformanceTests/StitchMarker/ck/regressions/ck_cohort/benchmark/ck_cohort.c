#include "../ck_cohort.h"

#include <ck_cohort.h>
#ifdef THROUGHPUT
#include "../../ck_spinlock/benchmark/throughput.h"
#elif defined(LATENCY)
#include "../../ck_spinlock/benchmark/latency.h"
#endif
