MODULES = WTF JavaScriptCore ThirdParty/ANGLE WebCore WebKit WebKit2

IOS_DONT_BUILD = WebKit2

ifneq (,$(findstring iphoneos,$(SDK)))
	MODULES = $(filter-out $(IOS_DONT_BUILD),$(MODULES))
else ifneq (,$(findstring iphonesimulator,$(SDK)))
	MODULES = $(subst $(IOS_DONT_BUILD),$(MODULES))
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
