.PHONY: check clean distribution

OBJECTS=serial mpnc_push mpmc_push upmc_push spinlock_push spinlock_eb_push \
	mpmc_pop upmc_pop spinlock_pop spinlock_eb_pop			    \
	upmc_trypop mpmc_trypop mpmc_trypair				    \
	mpmc_pair spinlock_pair spinlock_eb_pair pthreads_pair		    \
	mpmc_trypush upmc_trypush

all: $(OBJECTS)

check: all
	./serial
	./mpmc_pair $(CORES) 1 0
	./upmc_trypop $(CORES) 1 0
	./mpmc_trypop $(CORES) 1 0
	./mpmc_trypair $(CORES) 1 0
	./mpmc_pop $(CORES) 1 0
	./upmc_pop $(CORES) 1 0
	./mpnc_push $(CORES) 1 0
	./mpmc_push $(CORES) 1 0
	./upmc_push $(CORES) 1 0
	./mpmc_trypush $(CORES) 1 0
	./upmc_trypush $(CORES) 1 0

serial: serial.c
	$(CC) $(CFLAGS) -o serial serial.c

mpmc_trypush upmc_trypush mpnc_push mpmc_push upmc_push spinlock_push spinlock_eb_push: push.c
	$(CC) -DTRYUPMC $(CFLAGS) -o upmc_trypush push.c
	$(CC) -DTRYMPMC $(CFLAGS) -o mpmc_trypush push.c
	$(CC) -DMPNC $(CFLAGS) -o mpnc_push push.c
	$(CC) -DMPMC $(CFLAGS) -o mpmc_push push.c
	$(CC) -DUPMC $(CFLAGS) -o upmc_push push.c
	$(CC) -DSPINLOCK $(CFLAGS) -o spinlock_push push.c
	$(CC) -DSPINLOCK -DEB $(CFLAGS) -o spinlock_eb_push push.c

upmc_trypop mpmc_trypop mpmc_pop tryupmc_pop upmc_pop spinlock_pop spinlock_eb_pop: pop.c
	$(CC) -DTRYMPMC $(CFLAGS) -o mpmc_trypop pop.c
	$(CC) -DTRYUPMC $(CFLAGS) -o upmc_trypop pop.c
	$(CC) -DMPMC $(CFLAGS) -o mpmc_pop pop.c
	$(CC) -DUPMC $(CFLAGS) -o upmc_pop pop.c
	$(CC) -DSPINLOCK $(CFLAGS) -o spinlock_pop pop.c
	$(CC) -DEB -DSPINLOCK $(CFLAGS) -o spinlock_eb_pop pop.c

mpmc_trypair mpmc_pair spinlock_pair spinlock_eb_pair pthreads_pair: pair.c
	$(CC) -DTRYMPMC $(CFLAGS) -o mpmc_trypair pair.c
	$(CC) -DMPMC $(CFLAGS) -o mpmc_pair pair.c
	$(CC) -DSPINLOCK $(CFLAGS) -o spinlock_pair pair.c
	$(CC) -DEB -DSPINLOCK $(CFLAGS) -o spinlock_eb_pair pair.c
	$(CC) -DPTHREADS $(CFLAGS) -o pthreads_pair pair.c

clean:
	rm -rf *~ *.o *.dSYM *.exe $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=$(PTHREAD_CFLAGS) -D_GNU_SOURCE
