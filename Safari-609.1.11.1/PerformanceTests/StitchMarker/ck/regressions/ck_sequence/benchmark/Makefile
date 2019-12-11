.PHONY: clean distribution

OBJECTS=ck_sequence

all: $(OBJECTS)

ck_sequence: ck_sequence.c ../../../include/ck_sequence.h
	$(CC) $(CFLAGS) -o ck_sequence ck_sequence.c

check: all
	./ck_sequence $(CORES) 1

clean:
	rm -rf *~ *.o $(OBJECTS) *.dSYM *.exe

include ../../../build/regressions.build
CFLAGS+=-D_GNU_SOURCE

