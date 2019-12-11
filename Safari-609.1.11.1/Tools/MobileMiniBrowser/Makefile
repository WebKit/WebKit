
SCRIPTS_PATH = ../Scripts
include ../../Makefile.shared

test-iphone: all
	( $(SET_COLOR_DIAGNOSTICS_ARG); xcodebuild $(OTHER_OPTIONS) $(XCODE_OPTIONS) -scheme MobileMiniBrowserUITests -destination 'platform=iOS Simulator,name=iPhone 5s For WebKit Development' test | $(OUTPUT_FILTER) && exit $${PIPESTATUS[0]} )

test-ipad: all
	( $(SET_COLOR_DIAGNOSTICS_ARG); xcodebuild $(OTHER_OPTIONS) $(XCODE_OPTIONS) -scheme MobileMiniBrowserUITests -destination 'platform=iOS Simulator,name=iPad Retina' test | $(OUTPUT_FILTER) && exit $${PIPESTATUS[0]} )

test: test-iphone test-ipad
	