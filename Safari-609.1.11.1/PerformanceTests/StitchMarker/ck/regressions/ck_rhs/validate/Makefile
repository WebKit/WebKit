.PHONY: check clean distribution

OBJECTS=serial

all: $(OBJECTS)

serial: serial.c ../../../include/ck_rhs.h ../../../src/ck_rhs.c
	$(CC) $(CFLAGS) -o serial serial.c ../../../src/ck_rhs.c

check: all
	./serial

clean:
	rm -rf *~ *.o $(OBJECTS) *.dSYM *.exe

include ../../../build/regressions.build
CFLAGS+=-D_GNU_SOURCE
