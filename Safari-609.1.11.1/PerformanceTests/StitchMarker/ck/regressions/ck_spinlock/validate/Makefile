.PHONY: check clean

all: ck_ticket ck_mcs ck_dec ck_cas ck_fas ck_clh linux_spinlock \
     ck_ticket_pb ck_anderson ck_spinlock ck_hclh

check: all
	./ck_ticket $(CORES) 1
	./ck_mcs $(CORES) 1
	./ck_dec $(CORES) 1
	./ck_cas $(CORES) 1
	./ck_fas $(CORES) 1
	./ck_clh $(CORES) 1
	./ck_hclh $(CORES) 1
	./linux_spinlock $(CORES) 1
	./ck_ticket_pb $(CORES) 1
	./ck_anderson $(CORES) 1
	./ck_spinlock $(CORES) 1

linux_spinlock: linux_spinlock.c
	$(CC) $(CFLAGS) -o linux_spinlock linux_spinlock.c

ck_spinlock: ck_spinlock.c
	$(CC) $(CFLAGS) -o ck_spinlock ck_spinlock.c

ck_ticket_pb: ck_ticket_pb.c
	$(CC) $(CFLAGS) -o ck_ticket_pb ck_ticket_pb.c

ck_clh: ck_clh.c
	$(CC) $(CFLAGS) -o ck_clh ck_clh.c

ck_hclh: ck_hclh.c
	$(CC) $(CFLAGS) -o ck_hclh ck_hclh.c

ck_anderson: ck_anderson.c
	$(CC) $(CFLAGS) -o ck_anderson ck_anderson.c

ck_fas: ck_fas.c
	$(CC) $(CFLAGS) -o ck_fas ck_fas.c

ck_ticket: ck_ticket.c
	$(CC) $(CFLAGS) -o ck_ticket ck_ticket.c

ck_cas: ck_cas.c
	$(CC) $(CFLAGS) -o ck_cas ck_cas.c

ck_mcs: ck_mcs.c
	$(CC) $(CFLAGS) -o ck_mcs ck_mcs.c

ck_dec: ck_dec.c
	$(CC) $(CFLAGS) -o ck_dec ck_dec.c

clean:
	rm -rf ck_ticket ck_mcs ck_dec ck_cas ck_fas ck_clh linux_spinlock ck_ticket_pb \
		ck_anderson ck_spinlock ck_hclh *.dSYM *.exe

include ../../../build/regressions.build
CFLAGS+=$(PTHREAD_CFLAGS) -D_GNU_SOURCE -lm
