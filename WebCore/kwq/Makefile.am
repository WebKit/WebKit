KWQCharsetData.c: make-charset-table.pl character-sets.txt mac-encodings.txt
	perl $^ > $@

KWQColorData.c: KWQColorData.gperf Makefile.am
	gperf -CDEot -L 'ANSI-C' -k '*' -N findColor $< > $@ 

BUILT_SOURCES = KWQCharsetData.c KWQColorData.c
CLEANFILES = $(BUILT_SOURCES)
