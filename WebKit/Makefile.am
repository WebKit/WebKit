all-am:
	defaults write com.apple.Xcode PBXProductDirectory "$(SYMROOTS)"
	defaults write com.apple.Xcode PBXIntermediatesDirectory "$(SYMROOTS)"
	xcodebuild -configuration $(BUILDSTYLE)
clean-am:
	defaults write com.apple.Xcode PBXProductDirectory "$(SYMROOTS)"
	defaults write com.apple.Xcode PBXIntermediatesDirectory "$(SYMROOTS)"
	xcodebuild clean -configuration ${BUILDSTYLE}
doc: all
	../Tools/Scripts/make-docs $(SYMROOTS)/Documentation/WebKit $(SYMROOTS)/WebKit.framework/Headers
