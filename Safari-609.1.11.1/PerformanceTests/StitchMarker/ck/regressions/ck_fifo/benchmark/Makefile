.PHONY: clean distribution

OBJECTS=latency

all: $(OBJECTS)

latency: latency.c
	$(CC) $(CFLAGS) -o latency latency.c

clean:
	rm -rf *~ *.o *.dSYM *.exe $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=$(PTHREAD_CFLAGS) -D_GNU_SOURCE
