all-am:
	xcodebuild -buildstyle $(BUILDSTYLE) OBJROOT=$(SYMROOTS) SYMROOT=$(SYMROOTS) DSTROOT=$(SYMROOTS) 
clean-am:
	xcodebuild clean OBJROOT=$(SYMROOTS) SYMROOT=$(SYMROOTS) DSTROOT=$(SYMROOTS) 
	rm -rf $(SYMROOTS)/WebCore.build
	rm -rf $(SYMROOTS)/WebCore.framework
