all-am:
	xcodebuild -target All -buildstyle $(BUILDSTYLE) OBJROOT=$(SYMROOTS) SYMROOT=$(SYMROOTS) DSTROOT=$(SYMROOTS)
clean-am:
	xcodebuild -target All clean OBJROOT=$(SYMROOTS) SYMROOT=$(SYMROOTS) DSTROOT=$(SYMROOTS)
	rm -rf $(SYMROOTS)/JavaScriptCore.build
	rm -rf $(SYMROOTS)/JavaScriptCore.framework
