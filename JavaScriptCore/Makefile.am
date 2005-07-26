all-am:
	defaults write com.apple.Xcode PBXProductDirectory "$(SYMROOTS)"
	defaults write com.apple.Xcode PBXIntermediatesDirectory "$(SYMROOTS)"
	xcodebuild -target All -configuration $(BUILDSTYLE)
clean-am:
	defaults write com.apple.Xcode PBXProductDirectory "$(SYMROOTS)"
	defaults write com.apple.Xcode PBXIntermediatesDirectory "$(SYMROOTS)"
	xcodebuild -target All clean -configuration ${BUILDSTYLE}
