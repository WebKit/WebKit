kentities.c: kentities.gperf
	$(GPERF) -a -L "ANSI-C" -C -G -c -o -t -k '*' -NfindEntity -D -s 2 $< > $@

doctypes.cpp: doctypes.gperf Makefile.am
	$(GPERF) -CEot -L "ANSI-C" -k "*" -N findDoctypeEntry -F ,PubIDInfo::eAlmostStandards,PubIDInfo::eAlmostStandards $< > $@

noinst_HEADERS = kentities.c doctypes.cpp
BUILT_SOURCES = $(noinst_HEADERS)
CLEANFILES = $(BUILT_SOURCES)
