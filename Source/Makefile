MODULES = WTF JavaScriptCore ThirdParty/ANGLE WebCore WebInspectorUI WebKit WebKit2

ifneq (,$(findstring iphoneos,$(SDKROOT)))
	MODULES = WTF JavaScriptCore ThirdParty/ANGLE WebCore WebKit WebKit2
else ifneq (,$(findstring iphonesimulator,$(SDKROOT)))
	MODULES = WTF JavaScriptCore ThirdParty/ANGLE WebCore WebKit WebKit2
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
