all-am: previous-clean-timestamp
	xcodebuild -buildstyle $(BUILDSTYLE) OBJROOT=$(SYMROOTS) SYMROOT=$(SYMROOTS) DSTROOT=$(SYMROOTS) 
clean-am:
	xcodebuild clean OBJROOT=$(SYMROOTS) SYMROOT=$(SYMROOTS) DSTROOT=$(SYMROOTS) 
	rm -rf $(SYMROOTS)/WebKit.build
	rm -rf $(SYMROOTS)/WebKit.framework
	rm -rf $(SYMROOTS)/Safari.app/Frameworks/WebKit.framework

previous-clean-timestamp: force-clean-timestamp
	make clean && touch $@

doc: all
	../Tools/Scripts/make-docs $(SYMROOTS)/Documentation/WebKit $(SYMROOTS)/WebKit.framework/Headers
