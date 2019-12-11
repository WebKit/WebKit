.PHONY: check clean

OBJECTS=serial

all: $(OBJECTS)

serial: serial.c ../../../include/ck_bitmap.h
	$(CC) $(CFLAGS) -o serial serial.c

clean:
	rm -rf *~ *.o $(OBJECTS) *.dSYM *.exe

check: all
	./serial

include ../../../build/regressions.build
CFLAGS+=-D_GNU_SOURCE
