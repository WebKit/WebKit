.PHONY: clean distribution

OBJECTS=latency

all: $(OBJECTS)

latency: latency.c
	$(CC) $(CFLAGS) -o latency latency.c

clean:
	rm -rf *.dSYM *.exe *~ *.o $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=$(PTHREAD_CFLAGS) -D_GNU_SOURCE
