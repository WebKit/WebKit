LIBWEBRTC_MODULE = 
WEBINSPECTORUI_MODULE =

ifneq (,$(SDKROOT))
	ifneq (,$(findstring iphone,$(SDKROOT)))
		LIBWEBRTC_MODULE = ThirdParty/libwebrtc
	endif
	ifneq (,$(findstring macosx,$(SDKROOT)))
		LIBWEBRTC_MODULE = ThirdParty/libwebrtc
		WEBINSPECTORUI_MODULE = WebInspectorUI
	endif
else
	LIBWEBRTC_MODULE = ThirdParty/libwebrtc
	WEBINSPECTORUI_MODULE = WebInspectorUI
endif

ifneq (,$(DISABLE_LIBWEBRTC))
	LIBWEBRTC_MODULE =
endif

MODULES = bmalloc WTF JavaScriptCore ThirdParty/ANGLE $(LIBWEBRTC_MODULE) WebCore $(WEBINSPECTORUI_MODULE) WebKitLegacy WebKit

all:
	@for dir in $(MODULES); do ${MAKE} $@ -C $$dir; exit_status=$$?; \
	if [ $$exit_status -ne 0 ]; then exit $$exit_status; fi; done

debug d:
	@for dir in $(MODULES); do ${MAKE} $@ -C $$dir; exit_status=$$?; \
	if [ $$exit_status -ne 0 ]; then exit $$exit_status; fi; done

release r:
	@for dir in $(MODULES); do ${MAKE} $@ -C $$dir; exit_status=$$?; \
	if [ $$exit_status -ne 0 ]; then exit $$exit_status; fi; done

analyze:
	@for dir in $(MODULES); do ${MAKE} $@ -C $$dir; exit_status=$$?; \
	if [ $$exit_status -ne 0 ]; then exit $$exit_status; fi; done

clean:
	@for dir in $(MODULES); do ${MAKE} $@ -C $$dir; exit_status=$$?; \
	if [ $$exit_status -ne 0 ]; then exit $$exit_status; fi; done
