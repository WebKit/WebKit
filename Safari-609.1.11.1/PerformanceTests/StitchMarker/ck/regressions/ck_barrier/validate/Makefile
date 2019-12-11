.PHONY: check clean distribution

OBJECTS=barrier_centralized barrier_combining barrier_dissemination barrier_tournament barrier_mcs

all: $(OBJECTS)

barrier_centralized: barrier_centralized.c ../../../include/ck_barrier.h ../../../src/ck_barrier_centralized.c
	$(CC) $(CFLAGS) -o barrier_centralized barrier_centralized.c ../../../src/ck_barrier_centralized.c

barrier_combining: barrier_combining.c ../../../include/ck_barrier.h ../../../src/ck_barrier_combining.c
	$(CC) $(CFLAGS) -o barrier_combining barrier_combining.c ../../../src/ck_barrier_combining.c

barrier_dissemination: barrier_dissemination.c ../../../include/ck_barrier.h ../../../src/ck_barrier_dissemination.c
	$(CC) $(CFLAGS) -o barrier_dissemination barrier_dissemination.c ../../../src/ck_barrier_dissemination.c

barrier_tournament: barrier_tournament.c ../../../include/ck_barrier.h ../../../src/ck_barrier_tournament.c
	$(CC) $(CFLAGS) -o barrier_tournament barrier_tournament.c ../../../src/ck_barrier_tournament.c

barrier_mcs: barrier_mcs.c ../../../include/ck_barrier.h ../../../src/ck_barrier_mcs.c
	$(CC) $(CFLAGS) -o barrier_mcs barrier_mcs.c ../../../src/ck_barrier_mcs.c

check: all
	rc=0;                                                   \
	for d in $(OBJECTS) ; do                                \
		echo $$d;					\
		./$$d $(CORES) 1 1 || rc=1;     		\
	done;                                                   \
	exit $$rc

clean:
	rm -rf *.dSYM *.exe *~ *.o $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=$(PTHREAD_CFLAGS) -D_GNU_SOURCE
