.PHONY: clean distribution

OBJECTS=fifo_latency stack_latency

all: $(OBJECTS)

fifo_latency: fifo_latency.c
	$(CC) $(CFLAGS) -o fifo_latency ../../../src/ck_hp.c fifo_latency.c

stack_latency: stack_latency.c
	$(CC) $(CFLAGS) -o stack_latency ../../../src/ck_hp.c stack_latency.c

clean:
	rm -rf *~ *.o *.dSYM *.exe $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=$(PTHREAD_CFLAGS) -D_GNU_SOURCE
