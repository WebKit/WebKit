SUBDIRS = khtml kwq

all-am: previous-clean-timestamp previous-js-clean-timestamp
	pbxbuild -buildstyle $(BUILDSTYLE) OBJROOT=$(SYMROOTS) SYMROOT=$(SYMROOTS) DSTROOT=$(SYMROOTS) 
clean-am:
	pbxbuild clean OBJROOT=$(SYMROOTS) SYMROOT=$(SYMROOTS) DSTROOT=$(SYMROOTS) 
	rm -rf $(SYMROOTS)/WebCore.build
	rm -rf $(SYMROOTS)/WebCore.framework
	rm -rf $(SYMROOTS)/Safari.app/Frameworks/WebCore.framework

WebCore-combined.exp: WebCore.exp WebCore-tests.exp
	cat $^ > $@

config.h: ../config.h
	grep -v NO_LICENSE ../config.h > new-config.h
	if cmp -s config.h new-config.h; then \
		rm -f new-config.h; \
	else \
		mv new-config.h config.h; \
	fi

noinst_DATA = WebCore-combined.exp
noinst_HEADERS = config.h
BUILT_SOURCES = $(noinst_DATA) $(noinst_HEADERS)
CLEANFILES = $(BUILT_SOURCES)

previous-clean-timestamp: force-clean-timestamp
	make clean && touch $@ && touch previous-js-clean-timestamp

previous-js-clean-timestamp: force-js-clean-timestamp
	../Tools/Scripts/clean-webcore-js && touch $@
