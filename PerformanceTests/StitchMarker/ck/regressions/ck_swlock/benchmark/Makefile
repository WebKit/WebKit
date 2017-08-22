.PHONY: clean distribution

OBJECTS=latency throughput

all: $(OBJECTS)

latency: latency.c ../../../include/ck_swlock.h
	$(CC) $(CFLAGS) -o latency latency.c

throughput: throughput.c ../../../include/ck_swlock.h
	$(CC) $(CFLAGS) -o throughput throughput.c

clean:
	rm -rf *.dSYM *.exe *~ *.o $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=$(PTHREAD_CFLAGS) -D_GNU_SOURCE
