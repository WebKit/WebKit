all-am: previous-clean-timestamp
	pbxbuild -buildstyle $(BUILDSTYLE)
clean-am:
	pbxbuild clean
	rm -rf $(SYMROOTS)/WebKit.build
	rm -rf $(SYMROOTS)/WebKit.framework
	rm -rf $(SYMROOTS)/Alexander.app/Frameworks/WebKit.framework

previous-clean-timestamp: force-clean-timestamp
	make clean && touch $@

doc: all
	../Tools/Scripts/make-docs $(SYMROOTS)/Documentation/WebKit $(SYMROOTS)/WebKit.framework/Headers
