MODULES = DumpRenderTree WebKitTestRunner MiniBrowser ../Source/ThirdParty/gtest/xcode TestWebKitAPI

IOS_DONT_BUILD = WebKitTestRunner MiniBrowser TestWebKitAPI
IPHONEOS_DONT_BUILD = DumpRenderTree

ifneq (,$(findstring iphoneos,$(SDK)))
	MODULES = $(filter-out $(IOS_DONT_BUILD),$(MODULES))
	MODULES = $(filter-out $(IPHONEOS_DONT_BUILD),$(MODULES))
else ifneq (,$(findstring iphonesimulator,$(SDK)))
	MODULES = $(filter-out $(IOS_DONT_BUILD),$(MODULES))
endif

all:
	@for dir in $(MODULES); do ${MAKE} $@ -C $$dir; exit_status=$$?; \
	if [ $$exit_status -ne 0 ]; then exit $$exit_status; fi; done

debug d:
	@for dir in $(MODULES); do ${MAKE} $@ -C $$dir; exit_status=$$?; \
	if [ $$exit_status -ne 0 ]; then exit $$exit_status; fi; done

release r:
	@for dir in $(MODULES); do ${MAKE} $@ -C $$dir; exit_status=$$?; \
	if [ $$exit_status -ne 0 ]; then exit $$exit_status; fi; done

clean:
	@for dir in $(MODULES); do ${MAKE} $@ -C $$dir; exit_status=$$?; \
	if [ $$exit_status -ne 0 ]; then exit $$exit_status; fi; done
