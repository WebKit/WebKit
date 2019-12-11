.PHONY: clean distribution

OBJECTS=latency throughput
OBJECTS=ck_neutral.THROUGHPUT ck_neutral.LATENCY		\
	ck_rp.THROUGHPUT ck_rp.LATENCY				\
	ck_wp.THROUGHPUT ck_wp.LATENCY

all: $(OBJECTS)

ck_neutral.THROUGHPUT: ck_neutral.c
	$(CC) -DTHROUGHPUT $(CFLAGS) -o ck_neutral.THROUGHPUT ck_neutral.c

ck_neutral.LATENCY: ck_neutral.c
	$(CC) -DLATENCY $(CFLAGS) -o ck_neutral.LATENCY ck_neutral.c

ck_rp.THROUGHPUT: ck_rp.c
	$(CC) -DTHROUGHPUT $(CFLAGS) -o ck_rp.THROUGHPUT ck_rp.c

ck_rp.LATENCY: ck_rp.c
	$(CC) -DLATENCY $(CFLAGS) -o ck_rp.LATENCY ck_rp.c

ck_wp.THROUGHPUT: ck_wp.c
	$(CC) -DTHROUGHPUT $(CFLAGS) -o ck_wp.THROUGHPUT ck_wp.c

ck_wp.LATENCY: ck_wp.c
	$(CC) -DLATENCY $(CFLAGS) -o ck_wp.LATENCY ck_wp.c

clean:
	rm -rf *.dSYM *~ *.o $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=$(PTHREAD_CFLAGS) -D_GNU_SOURCE
