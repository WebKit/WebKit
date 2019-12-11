.PHONY: all clean

OBJECTS=ck_ticket.THROUGHPUT ck_ticket.LATENCY			\
	ck_mcs.THROUGHPUT ck_mcs.LATENCY			\
	ck_dec.THROUGHPUT ck_dec.LATENCY			\
	ck_cas.THROUGHPUT ck_cas.LATENCY			\
	ck_fas.THROUGHPUT ck_fas.LATENCY			\
	ck_clh.THROUGHPUT ck_clh.LATENCY			\
	linux_spinlock.THROUGHPUT linux_spinlock.LATENCY	\
	ck_ticket_pb.THROUGHPUT ck_ticket_pb.LATENCY		\
	ck_anderson.THROUGHPUT ck_anderson.LATENCY		\
	ck_spinlock.THROUGHPUT ck_spinlock.LATENCY		\
	ck_hclh.THROUGHPUT ck_hclh.LATENCY

all: $(OBJECTS)

ck_spinlock.THROUGHPUT: ck_spinlock.c
	$(CC) -DTHROUGHPUT $(CFLAGS) -o ck_spinlock.THROUGHPUT ck_spinlock.c -lm

ck_spinlock.LATENCY: ck_spinlock.c
	$(CC) -DLATENCY $(CFLAGS) -o ck_spinlock.LATENCY ck_spinlock.c -lm

ck_ticket.THROUGHPUT: ck_ticket.c
	$(CC) -DTHROUGHPUT $(CFLAGS) -o ck_ticket.THROUGHPUT ck_ticket.c -lm

ck_ticket.LATENCY: ck_ticket.c
	$(CC) -DLATENCY $(CFLAGS) -o ck_ticket.LATENCY ck_ticket.c -lm

ck_mcs.THROUGHPUT: ck_mcs.c
	$(CC) -DTHROUGHPUT $(CFLAGS) -o ck_mcs.THROUGHPUT ck_mcs.c -lm

ck_mcs.LATENCY: ck_mcs.c
	$(CC) -DLATENCY $(CFLAGS) -o ck_mcs.LATENCY ck_mcs.c -lm

ck_dec.THROUGHPUT: ck_dec.c
	$(CC) -DTHROUGHPUT $(CFLAGS) -o ck_dec.THROUGHPUT ck_dec.c -lm

ck_dec.LATENCY: ck_dec.c
	$(CC) -DLATENCY $(CFLAGS) -o ck_dec.LATENCY ck_dec.c -lm

ck_cas.THROUGHPUT: ck_cas.c
	$(CC) -DTHROUGHPUT $(CFLAGS) -o ck_cas.THROUGHPUT ck_cas.c -lm

ck_cas.LATENCY: ck_cas.c
	$(CC) -DLATENCY $(CFLAGS) -o ck_cas.LATENCY ck_cas.c -lm

ck_fas.THROUGHPUT: ck_fas.c
	$(CC) -DTHROUGHPUT $(CFLAGS) -o ck_fas.THROUGHPUT ck_fas.c -lm

ck_fas.LATENCY: ck_fas.c
	$(CC) -DLATENCY $(CFLAGS) -o ck_fas.LATENCY ck_fas.c -lm

ck_clh.THROUGHPUT: ck_clh.c
	$(CC) -DTHROUGHPUT $(CFLAGS) -o ck_clh.THROUGHPUT ck_clh.c -lm

ck_clh.LATENCY: ck_clh.c
	$(CC) -DLATENCY $(CFLAGS) -o ck_clh.LATENCY ck_clh.c -lm

ck_hclh.THROUGHPUT: ck_hclh.c
	$(CC) -DTHROUGHPUT $(CFLAGS) -o ck_hclh.THROUGHPUT ck_hclh.c -lm

ck_hclh.LATENCY: ck_hclh.c
	$(CC) -DLATENCY $(CFLAGS) -o ck_hclh.LATENCY ck_hclh.c -lm

linux_spinlock.THROUGHPUT: linux_spinlock.c
	$(CC) -DTHROUGHPUT $(CFLAGS) -o linux_spinlock.THROUGHPUT linux_spinlock.c -lm

linux_spinlock.LATENCY: linux_spinlock.c
	$(CC) -DLATENCY $(CFLAGS) -o linux_spinlock.LATENCY linux_spinlock.c -lm

ck_ticket_pb.THROUGHPUT: ck_ticket_pb.c
	$(CC) -DTHROUGHPUT $(CFLAGS) -o ck_ticket_pb.THROUGHPUT ck_ticket_pb.c -lm

ck_ticket_pb.LATENCY: ck_ticket_pb.c
	$(CC) -DLATENCY $(CFLAGS) -o ck_ticket_pb.LATENCY ck_ticket_pb.c -lm

ck_anderson.THROUGHPUT: ck_anderson.c
	$(CC) -DTHROUGHPUT $(CFLAGS) -o ck_anderson.THROUGHPUT ck_anderson.c -lm

ck_anderson.LATENCY: ck_anderson.c
	$(CC) -DLATENCY $(CFLAGS) -o ck_anderson.LATENCY ck_anderson.c -lm

clean:
	rm -rf *.dSYM *.exe $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=$(PTHREAD_CFLAGS) -D_GNU_SOURCE
