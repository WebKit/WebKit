SCRIPTS_PATH = ../Scripts
# Opt out of workspace builds, as ImageDiff is not part of any workspace.
USE_WORKSPACE = NO

TO_LOWER = $(shell echo $(1) | tr [:upper:] [:lower:])

# For non-macOS SDKs, build a macOS version of ImageDiff.
ifeq (,$(findstring macos,$(call TO_LOWER,$(notdir $(SDKROOT)))))
	OVERRIDE_SDKROOT = default
	OVERRIDE_ARCHS = default
endif

include ../../Makefile.shared
