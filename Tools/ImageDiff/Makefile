SCRIPTS_PATH = ../Scripts

TO_LOWER = $(shell echo $(1) | tr [:upper:] [:lower:])

ifneq (,$(findstring iphone,$(call TO_LOWER,$(notdir $(SDKROOT)))))
	OVERRIDE_SDKROOT = default
	OVERRIDE_ARCHS = default
endif
ifneq (,$(findstring watch,$(call TO_LOWER,$(notdir $(SDKROOT)))))
	OVERRIDE_SDKROOT = default
	OVERRIDE_ARCHS = default
endif
ifneq (,$(findstring tv,$(call TO_LOWER,$(notdir $(SDKROOT)))))
	OVERRIDE_SDKROOT = default
	OVERRIDE_ARCHS = default
endif

include ../../Makefile.shared
