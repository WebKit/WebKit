all-am:
	defaults write com.apple.Xcode PBXProductDirectory "$(SYMROOTS)"
	defaults write com.apple.Xcode PBXIntermediatesDirectory "$(SYMROOTS)"
	../WebKitTools/Scripts/set-webkit-configuration --$(BUILDSTYLE)
	xcodebuild -configuration $(BUILDSTYLE) $(WEBCORE_TARGET)
clean-am:
	defaults write com.apple.Xcode PBXProductDirectory "$(SYMROOTS)"
	defaults write com.apple.Xcode PBXIntermediatesDirectory "$(SYMROOTS)"
	../WebKitTools/Scripts/set-webkit-configuration --$(BUILDSTYLE)
	xcodebuild clean -configuration $(BUILDSTYLE) $(WEBCORE_TARGET)
