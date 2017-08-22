.PHONY: check clean distribution

OBJECTS=validate

all: $(OBJECTS)

validate: validate.c ../../../include/ck_tflock.h ../../../include/ck_elide.h
	$(CC) $(CFLAGS) -o validate validate.c

check: all
	./validate $(CORES) 1

clean:
	rm -rf *.dSYM *.exe *~ *.o $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=$(PTHREAD_CFLAGS) -D_GNU_SOURCE
