all-am:
	defaults write com.apple.Xcode PBXProductDirectory "$(SYMROOTS)"
	xcodebuild -buildstyle $(BUILDSTYLE)
clean-am:
	defaults write com.apple.Xcode PBXProductDirectory "$(SYMROOTS)"
	xcodebuild clean
