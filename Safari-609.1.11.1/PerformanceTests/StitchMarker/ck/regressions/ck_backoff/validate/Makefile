.PHONY: check clean

all: validate

validate: validate.c ../../../include/ck_backoff.h
	$(CC) $(CFLAGS) -o validate validate.c

check: all
	./validate

clean:
	rm -rf validate *.dSYM *.exe

include ../../../build/regressions.build
CFLAGS+=-D_GNU_SOURCE
