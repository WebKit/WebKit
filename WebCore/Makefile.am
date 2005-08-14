all-am:
	defaults write com.apple.Xcode PBXProductDirectory "$(SYMROOTS)"
	defaults write com.apple.Xcode PBXIntermediatesDirectory "$(SYMROOTS)"
	../WebKitTools/Scripts/set-webkit-configuration --$(BUILDSTYLE)
	xcodebuild -configuration $(BUILDSTYLE)
clean-am:
	defaults write com.apple.Xcode PBXProductDirectory "$(SYMROOTS)"
	defaults write com.apple.Xcode PBXIntermediatesDirectory "$(SYMROOTS)"
	../WebKitTools/Scripts/set-webkit-configuration --$(BUILDSTYLE)
	xcodebuild clean -configuration $(BUILDSTYLE)
