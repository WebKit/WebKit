all-am:
	pbxbuild -buildstyle $(BUILDSTYLE)
clean-am:
	pbxbuild clean
	rm -rf $(SYMROOTS)/WebKit.build
	rm -rf $(SYMROOTS)/WebKit.framework
	rm -rf $(SYMROOTS)/Alexander.app/Frameworks/WebKit.framework

WebKit-combined.exp: WebKit.exp WebKit-tests.exp
	cat $^ > $@

noinst_DATA = WebKit-combined.exp
BUILT_SOURCES = $(noinst_DATA)
CLEANFILES = $(BUILT_SOURCES)

all: previous-clean-timestamp
previous-clean-timestamp: force-clean-timestamp
	make clean && touch $@
