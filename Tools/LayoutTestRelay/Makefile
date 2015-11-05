SCRIPTS_PATH = ../Scripts

ifneq (,$(SDKROOT))
	ifeq ($(findstring iphonesimulator,$(SDKROOT)),iphonesimulator)
		SAVED_SDK_ROOT = $(SDKROOT)
		override SDKROOT =
	endif
endif

include ../../Makefile.shared

ifneq (,$(SAVED_SDK_ROOT))
	override SDKROOT = $(SAVED_SDK_ROOT)
	undefine SAVED_SDK_ROOT
endif
