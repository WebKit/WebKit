all-am:
	pbxbuild -buildstyle $(BUILDSTYLE)
clean-am:
	pbxbuild clean
	rm -rf $(SYMROOTS)/WebKit.build
	rm -rf $(SYMROOTS)/WebKit.framework
	rm -rf $(SYMROOTS)/Alexander.app/Frameworks/WebKit.framework

all: previous-clean-timestamp
previous-clean-timestamp: force-clean-timestamp
	make clean && touch $@
