TO_LOWER = $(shell echo $(1) | tr [:upper:] [:lower:])

ifneq (,$(SDKROOT))
	ifneq (,$(findstring watch,$(call TO_LOWER,$(notdir $(SDKROOT)))))
		XCODE_TARGET = -target boringssl
	endif
endif

include ../Makefile.shared
