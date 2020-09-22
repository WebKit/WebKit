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

define build_target_for_each_module
	for dir in $(MODULES); do \
		${MAKE} $@ -C $$dir PATH_FROM_ROOT=$(PATH_FROM_ROOT)/$${dir}; \
		exit_status=$$?; \
		[ $$exit_status -ne 0 ] && exit $$exit_status; \
	done; true
endef

all:
	@$(build_target_for_each_module)

debug d development dev develop:
	@$(build_target_for_each_module)

release r deployment dep deploy:
	@$(build_target_for_each_module)

release+assert ra:
	@$(build_target_for_each_module)

testing t:
	@$(build_target_for_each_module)

analyze:
	@$(build_target_for_each_module)

clean:
	@$(build_target_for_each_module)
