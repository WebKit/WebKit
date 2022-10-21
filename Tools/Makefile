ifeq ($(MAKECMDGOALS),installsrc)
USE_WORKSPACE = NO
endif

ifneq ($(USE_WORKSPACE),NO)

include ../Makefile.shared
SCHEME = All WebKit Tools

else

MODULES = DumpRenderTree WebKitTestRunner MiniBrowser ../Source/ThirdParty/gtest/xcode TestWebKitAPI

TO_LOWER = $(shell echo $(1) | tr [:upper:] [:lower:])

ifneq (,$(SDKROOT))
	ifeq (,$(findstring macosx,$(call TO_LOWER,$(notdir $(SDKROOT)))))
		# Embedded OS
		MODULES = DumpRenderTree WebKitTestRunner ../Source/ThirdParty/gtest/xcode TestWebKitAPI
	endif
	ifneq (,$(findstring macosx,$(call TO_LOWER,$(notdir $(SDKROOT)))))
		MODULES += lldb/lldbWebKitTester
	endif
	ifneq (,$(findstring iphone,$(call TO_LOWER,$(notdir $(SDKROOT)))))
		MODULES += MobileMiniBrowser
	endif
endif
ifeq (,$(DO_NOT_BUILD_IMAGE_DIFF))
	MODULES += ImageDiff
endif

ifeq (iosmac,$(SDK_VARIANT))
	MODULES = DumpRenderTree WebKitTestRunner ../Source/ThirdParty/gtest/xcode TestWebKitAPI
endif

define build_target_for_each_module
	for dir in $(MODULES); do \
		${MAKE} $@ -C $$dir PATH_FROM_ROOT=$(PATH_FROM_ROOT)/$${dir}; \
		exit_status=$$?; \
		[ $$exit_status -ne 0 ] && exit $$exit_status; \
	done; true
endef

all:
	@$(build_target_for_each_module)

debug d:
	@$(build_target_for_each_module)

release r:
	@$(build_target_for_each_module)

release+assert ra:
	@$(build_target_for_each_module)

testing t:
	@$(build_target_for_each_module)

analyze:
	@$(build_target_for_each_module)

clean:
	@$(build_target_for_each_module)

installsrc:
ifeq ($(FORCE_TOOL_INSTALL),YES)
	@$(build_target_for_each_module)
	ditto TestRunnerShared "$(SRCROOT)/Tools/TestRunnerShared"
else
	@true
endif

endif # USE_WORKSPACE
