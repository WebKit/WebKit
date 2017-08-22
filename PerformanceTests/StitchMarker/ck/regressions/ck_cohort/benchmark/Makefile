.PHONY: all clean

OBJECTS=ck_cohort.THROUGHPUT ck_cohort.LATENCY

all: $(OBJECTS)

ck_cohort.THROUGHPUT: ck_cohort.c
	$(CC) $(CFLAGS) -o ck_cohort.THROUGHPUT throughput.c -lm

ck_cohort.LATENCY: ck_cohort.c
	$(CC) -DLATENCY $(CFLAGS) -o ck_cohort.LATENCY ck_cohort.c

clean:
	rm -rf *.dSYM *.exe $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=$(PTHREAD_CFLAGS) -D_GNU_SOURCE -lm
