SCRIPTS_PATH = ../Scripts

TO_LOWER = $(shell echo $(1) | tr [:upper:] [:lower:])

# For non-macOS SDKs, build a macOS version of ImageDiff.
ifeq (,$(findstring macos,$(call TO_LOWER,$(notdir $(SDKROOT)))))
	OVERRIDE_SDKROOT = default
	OVERRIDE_ARCHS = default
endif

include ../../Makefile.shared
