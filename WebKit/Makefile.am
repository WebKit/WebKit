all-am:
	xcodebuild -buildstyle $(BUILDSTYLE)
clean-am:
	xcodebuild clean
doc: all
	../Tools/Scripts/make-docs $(SYMROOTS)/Documentation/WebKit $(SYMROOTS)/WebKit.framework/Headers
