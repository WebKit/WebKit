all-am:
	defaults write com.apple.Xcode PBXProductDirectory "$(SYMROOTS)"
	defaults delete com.apple.Xcode PBXIntermediatesDirectory || echo -n
	xcodebuild -buildstyle $(BUILDSTYLE)
clean-am:
	defaults write com.apple.Xcode PBXProductDirectory "$(SYMROOTS)"
	defaults delete com.apple.Xcode PBXIntermediatesDirectory || echo -n
	xcodebuild clean
doc: all
	../Tools/Scripts/make-docs $(SYMROOTS)/Documentation/WebKit $(SYMROOTS)/WebKit.framework/Headers
