.PHONY: check clean distribution

OBJECTS=serial

all: $(OBJECTS)

serial: serial.c ../../../include/ck_hs.h ../../../src/ck_hs.c
	$(CC) $(CFLAGS) -o serial serial.c ../../../src/ck_hs.c

check: all
	./serial

clean:
	rm -rf *~ *.o $(OBJECTS) *.dSYM *.exe

include ../../../build/regressions.build
CFLAGS+=-D_GNU_SOURCE
