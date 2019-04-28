ifneq (,$(SDKROOT))
	ifneq (,$(findstring watch,$(SDKROOT)))
		OTHER_OPTIONS += -target boringssl
	endif
endif

include ../Makefile.shared
