all-am:
	defaults write com.apple.Xcode PBXProductDirectory "$(SYMROOTS)"
	xcodebuild -target All -buildstyle $(BUILDSTYLE)
clean-am:
	defaults write com.apple.Xcode PBXProductDirectory "$(SYMROOTS)"
	xcodebuild -target All clean
