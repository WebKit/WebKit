SUBDIRS = khtml kwq

all-am:
	pbxbuild -buildstyle $(BUILDSTYLE)
clean-am:
	pbxbuild clean
	rm -rf $(SYMROOTS)/WebCore.build
	rm -rf $(SYMROOTS)/WebCore.framework
	rm -rf $(SYMROOTS)/Alexander.app/Frameworks/WebCore.framework

WebCore-combined.exp: WebCore.exp WebCore-tests.exp
	cat $^ > $@

noinst_DATA = WebCore-combined.exp
BUILT_SOURCES = $(noinst_DATA)
CLEANFILES = $(BUILT_SOURCES)
