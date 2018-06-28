MODULES = DumpRenderTree WebKitTestRunner MiniBrowser ../Source/ThirdParty/gtest/xcode TestWebKitAPI

ifneq (,$(SDKROOT))
	ifeq (,$(findstring macosx,$(SDKROOT)))
		# Embedded OS
		MODULES = DumpRenderTree WebKitTestRunner ../Source/ThirdParty/gtest/xcode TestWebKitAPI
	endif
	ifneq (,$(findstring macosx,$(SDKROOT)))
		MODULES += lldb/lldbWebKitTester
	endif
	ifneq (,$(findstring iphone,$(SDKROOT)))
		MODULES += MobileMiniBrowser
	endif
endif
ifeq (,$(DO_NOT_BUILD_IMAGE_DIFF))
	MODULES += ImageDiff
endif

all:
	@for dir in $(MODULES); do ${MAKE} $@ -C $$dir; exit_status=$$?; \
	if [ $$exit_status -ne 0 ]; then exit $$exit_status; fi; done

debug d development dev develop:
	@for dir in $(MODULES); do ${MAKE} $@ -C $$dir; exit_status=$$?; \
	if [ $$exit_status -ne 0 ]; then exit $$exit_status; fi; done

release r deployment dep deploy:
	@for dir in $(MODULES); do ${MAKE} $@ -C $$dir; exit_status=$$?; \
	if [ $$exit_status -ne 0 ]; then exit $$exit_status; fi; done

analyze:
	@for dir in $(MODULES); do ${MAKE} $@ -C $$dir; exit_status=$$?; \
	if [ $$exit_status -ne 0 ]; then exit $$exit_status; fi; done

clean:
	@for dir in $(MODULES); do ${MAKE} $@ -C $$dir; exit_status=$$?; \
	if [ $$exit_status -ne 0 ]; then exit $$exit_status; fi; done
