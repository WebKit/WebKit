NULL =

noinst_PROGRAMS = make-mac-encodings
make_mac_encodings_SOURCES = make-mac-encodings.c
make_mac_encodings_LDFLAGS = -framework CoreFoundation

mac-encodings.txt: make-mac-encodings
	$(<D)/$(<F) $@

KWQCharsetData.c: make-charset-table.pl character-sets.txt mac-encodings.txt
	perl $^ $@

BUILT_SOURCES = \
	make-mac-encodings \
	mac-encodings.txt \
	KWQCharsetData.c \
	$(NULL)

CLEANFILES = $(BUILT_SOURCES)
