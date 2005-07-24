all-am:
	defaults write com.apple.Xcode PBXProductDirectory "$(SYMROOTS)"
	xcodebuild -target All -configuration $(BUILDSTYLE)
clean-am:
	defaults write com.apple.Xcode PBXProductDirectory "$(SYMROOTS)"
	xcodebuild -target All clean -configuration ${BUILDSTYLE}
