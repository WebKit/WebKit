kentities.c: kentities.gperf
	$(GPERF) -a -L "ANSI-C" -C -G -c -o -t -k '*' -NfindEntity -D -s 2 $< > $@

noinst_HEADERS = kentities.c
BUILT_SOURCES = $(noinst_HEADERS)
CLEANFILES = $(BUILT_SOURCES)
