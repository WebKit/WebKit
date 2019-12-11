.PHONY: check clean distribution

OBJECTS=ck_fifo_spsc ck_fifo_mpmc ck_fifo_spsc_iterator ck_fifo_mpmc_iterator

all: $(OBJECTS)

check: all
	./ck_fifo_spsc $(CORES) 1 64000
	./ck_fifo_mpmc $(CORES) 1 16000
	./ck_fifo_spsc_iterator
	./ck_fifo_mpmc_iterator

ck_fifo_spsc: ck_fifo_spsc.c ../../../include/ck_fifo.h
	$(CC) $(CFLAGS) -o ck_fifo_spsc ck_fifo_spsc.c

ck_fifo_mpmc: ck_fifo_mpmc.c ../../../include/ck_fifo.h
	$(CC) $(CFLAGS) -o ck_fifo_mpmc ck_fifo_mpmc.c

ck_fifo_spsc_iterator: ck_fifo_spsc_iterator.c ../../../include/ck_fifo.h
	$(CC) $(CFLAGS) -o ck_fifo_spsc_iterator ck_fifo_spsc_iterator.c

ck_fifo_mpmc_iterator: ck_fifo_mpmc_iterator.c ../../../include/ck_fifo.h
	$(CC) $(CFLAGS) -o ck_fifo_mpmc_iterator ck_fifo_mpmc_iterator.c

clean:
	rm -rf *.dSYM *.exe *~ *.o $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=$(PTHREAD_CFLAGS) -D_GNU_SOURCE
