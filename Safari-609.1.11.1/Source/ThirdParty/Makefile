LIBWEBRTC_MODULE = 

ifeq (iosmac,$(SDK_VARIANT))
	DISABLE_LIBWEBRTC = 1
endif

ifneq (,$(SDKROOT))
	ifneq (,$(findstring iphone,$(SDKROOT)))
		LIBWEBRTC_MODULE = libwebrtc
	endif
	ifneq (,$(findstring macosx,$(SDKROOT)))
		LIBWEBRTC_MODULE = libwebrtc
	endif
	ifneq (,$(findstring watch,$(SDKROOT)))
		LIBWEBRTC_MODULE = libwebrtc
	endif
else
	LIBWEBRTC_MODULE = libwebrtc
endif

ifneq (,$(DISABLE_LIBWEBRTC))
	LIBWEBRTC_MODULE =
endif

MODULES = ANGLE $(LIBWEBRTC_MODULE)

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
