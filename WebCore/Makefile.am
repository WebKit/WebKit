all-am:
	defaults write com.apple.Xcode PBXProductDirectory "$(SYMROOTS)"
	defaults delete com.apple.Xcode PBXIntermediatesDirectory || echo -n
	xcodebuild -buildstyle $(BUILDSTYLE)
clean-am:
	defaults write com.apple.Xcode PBXProductDirectory "$(SYMROOTS)"
	defaults delete com.apple.Xcode PBXIntermediatesDirectory || echo -n
	xcodebuild clean
