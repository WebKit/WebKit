.PHONY: check clean distribution

OBJECTS=serial

all: $(OBJECTS)

serial: serial.c ../../../include/ck_array.h ../../../src/ck_array.c
	$(CC) $(CFLAGS) -o serial serial.c ../../../src/ck_array.c

check: all
	./serial

clean:
	rm -rf *~ *.o $(OBJECTS) *.dSYM *.exe

include ../../../build/regressions.build
CFLAGS+=-D_GNU_SOURCE -ggdb
