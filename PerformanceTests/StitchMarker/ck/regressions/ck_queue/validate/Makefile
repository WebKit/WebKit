.PHONY: check clean distribution

HEADER=../../../include/ck_queue.h
OBJECTS=ck_list ck_slist ck_stailq

all: $(OBJECTS)

check: all
	./ck_list $(CORES) 5
	./ck_slist $(CORES) 5
	./ck_stailq $(CORES) 1000000

ck_list: $(HEADER) ck_list.c
	$(CC) $(CFLAGS) -o ck_list ck_list.c

ck_slist: $(HEADER) ck_slist.c
	$(CC) $(CFLAGS) -o ck_slist ck_slist.c

ck_stailq: $(HEADER) ck_stailq.c
	$(CC) $(CFLAGS) -o ck_stailq ck_stailq.c

clean:
	rm -rf *~ *.o $(OBJECTS) *.dSYM *.exe

include ../../../build/regressions.build
CFLAGS+=$(PTHREAD_CFLAGS) -D_GNU_SOURCE
