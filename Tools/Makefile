MODULES = DumpRenderTree WebKitTestRunner MiniBrowser ../Source/ThirdParty/gtest/xcode TestWebKitAPI

ifneq (,$(findstring iphone,$(SDKROOT)))
	# FIXME: Build projects ../Source/ThirdParty/gtest/xcode, TestWebKitAPI for both device and simulator
	# and build project DumpRenderTree for simulator.
	MODULES = WebKitTestRunner
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

clean:
	@for dir in $(MODULES); do ${MAKE} $@ -C $$dir; exit_status=$$?; \
	if [ $$exit_status -ne 0 ]; then exit $$exit_status; fi; done
