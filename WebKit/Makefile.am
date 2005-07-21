all-am:
	defaults write com.apple.Xcode PBXProductDirectory "$(SYMROOTS)"
	xcodebuild -configuration $(BUILDSTYLE)
clean-am:
	defaults write com.apple.Xcode PBXProductDirectory "$(SYMROOTS)"
	xcodebuild clean
doc: all
	../Tools/Scripts/make-docs $(SYMROOTS)/Documentation/WebKit $(SYMROOTS)/WebKit.framework/Headers
