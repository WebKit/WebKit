SUBDIRS = khtml kwq

all-am: previous-clean-timestamp previous-js-clean-timestamp config-timestamp
	pbxbuild -buildstyle $(BUILDSTYLE) OBJROOT=$(SYMROOTS) SYMROOT=$(SYMROOTS) DSTROOT=$(SYMROOTS) 
clean-am:
	pbxbuild clean OBJROOT=$(SYMROOTS) SYMROOT=$(SYMROOTS) DSTROOT=$(SYMROOTS) 
	rm -rf $(SYMROOTS)/WebCore.build
	rm -rf $(SYMROOTS)/WebCore.framework
	rm -rf $(SYMROOTS)/Safari.app/Frameworks/WebCore.framework

WebCore-combined.exp: WebCore.exp WebCore-tests.exp previous-clean-timestamp
	cat $^ > $@

config.h: config-timestamp

config-timestamp: ../config.h previous-clean-timestamp
	perl -ne 'print unless /HAVE_FOUNDATION_SOURCES/ || /DLFCN/' < ../config.h > munged-config.h
	if ! cmp -s config.h munged-config.h; then \
	    cat munged-config.h > config.h; \
	fi
	rm munged-config.h
	touch $@

noinst_DATA = WebCore-combined.exp
noinst_HEADERS = config.h
BUILT_SOURCES = $(noinst_DATA) $(noinst_HEADERS)
CLEANFILES = $(BUILT_SOURCES) config-timestamp

previous-clean-timestamp: force-clean-timestamp
	make clean && touch $@ && touch previous-js-clean-timestamp

previous-js-clean-timestamp: force-js-clean-timestamp
	../Tools/Scripts/clean-webcore-js && touch $@
