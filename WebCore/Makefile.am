SUBDIRS = khtml kwq

all-am: previous-clean-timestamp previous-js-clean-timestamp
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

previous-clean-timestamp: force-clean-timestamp
	make clean && touch $@ && touch previous-js-clean-timestamp

previous-js-clean-timestamp: force-js-clean-timestamp
	../Tools/Scripts/clean-webcore-js && touch $@
