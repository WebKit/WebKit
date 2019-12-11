.PHONY: check clean distribution

OBJECTS=ck_pr_cas ck_pr_faa ck_pr_inc ck_pr_dec ck_pr_bts \
	ck_pr_btr ck_pr_btc ck_pr_load ck_pr_store 	  \
	ck_pr_and ck_pr_or ck_pr_xor ck_pr_add ck_pr_sub  \
	ck_pr_fas ck_pr_bin ck_pr_btx ck_pr_fax ck_pr_n	  \
	ck_pr_unary

all: $(OBJECTS)

check: all
	for d in $(OBJECTS) ; do        \
		echo $$d;		\
		./$$d || exit 1;     	\
	done;

ck_pr_cas: ck_pr_cas.c
	$(CC) $(CFLAGS) -o ck_pr_cas ck_pr_cas.c

ck_pr_inc: ck_pr_inc.c
	$(CC) $(CFLAGS) -o ck_pr_inc ck_pr_inc.c

ck_pr_dec: ck_pr_dec.c
	$(CC) $(CFLAGS) -o ck_pr_dec ck_pr_dec.c

ck_pr_faa: ck_pr_faa.c
	$(CC) $(CFLAGS) -o ck_pr_faa ck_pr_faa.c

ck_pr_btc: ck_pr_btc.c
	$(CC) $(CFLAGS) -o ck_pr_btc ck_pr_btc.c

ck_pr_btr: ck_pr_btr.c
	$(CC) $(CFLAGS) -o ck_pr_btr ck_pr_btr.c

ck_pr_bts: ck_pr_bts.c
	$(CC) $(CFLAGS) -o ck_pr_bts ck_pr_bts.c

ck_pr_load: ck_pr_load.c
	$(CC) $(CFLAGS) -o ck_pr_load ck_pr_load.c

ck_pr_store: ck_pr_store.c
	$(CC) $(CFLAGS) -o ck_pr_store ck_pr_store.c

ck_pr_and: ck_pr_and.c
	$(CC) $(CFLAGS) -o ck_pr_and ck_pr_and.c

ck_pr_or: ck_pr_or.c
	$(CC) $(CFLAGS) -o ck_pr_or ck_pr_or.c

ck_pr_xor: ck_pr_xor.c
	$(CC) $(CFLAGS) -o ck_pr_xor ck_pr_xor.c

ck_pr_add: ck_pr_add.c
	$(CC) $(CFLAGS) -o ck_pr_add ck_pr_add.c

ck_pr_sub: ck_pr_sub.c
	$(CC) $(CFLAGS) -o ck_pr_sub ck_pr_sub.c

ck_pr_fas: ck_pr_fas.c
	$(CC) $(CFLAGS) -o ck_pr_fas ck_pr_fas.c

ck_tp: ck_tp.c
	$(CC) $(CFLAGS) -o ck_tp ck_tp.c

ck_pr_bin: ck_pr_bin.c
	$(CC) $(CFLAGS) -o ck_pr_bin ck_pr_bin.c

ck_pr_btx: ck_pr_btx.c
	$(CC) $(CFLAGS) -o ck_pr_btx ck_pr_btx.c

ck_pr_fax: ck_pr_fax.c
	$(CC) $(CFLAGS) -o ck_pr_fax ck_pr_fax.c

ck_pr_n: ck_pr_n.c
	$(CC) $(CFLAGS) -o ck_pr_n ck_pr_n.c

ck_pr_unary: ck_pr_unary.c
	$(CC) $(CFLAGS) -o ck_pr_unary ck_pr_unary.c

clean:
	rm -rf *~ *.o $(OBJECTS) *.dSYM *.exe

include ../../../build/regressions.build
CFLAGS+=-D_GNU_SOURCE
