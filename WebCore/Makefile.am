SUBDIRS = khtml kwq

all-am:
	pbxbuild -buildstyle $(BUILDSTYLE)
clean-am:
	pbxbuild clean

WebCore-combined.exp: WebCore.exp WebCore-tests.exp
	cat $^ > $@

noinst_DATA = WebCore-combined.exp
BUILT_SOURCES = $(noinst_DATA)
CLEANFILES = $(BUILT_SOURCES)
