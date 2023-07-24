ifeq ($(MAKECMDGOALS),installsrc)
USE_WORKSPACE = NO
endif

ifneq ($(USE_WORKSPACE),NO)

include ../Makefile.shared
SCHEME = Everything up to WebKit

else

WEBINSPECTORUI_MODULE =

TO_LOWER = $(shell echo $(1) | tr [:upper:] [:lower:])

ifneq (,$(SDKROOT))
	ifneq (,$(findstring macosx,$(call TO_LOWER,$(notdir $(SDKROOT)))))
		WEBINSPECTORUI_MODULE = WebInspectorUI
	endif
	ifeq ($(MAKECMDGOALS),installsrc)
		WEBINSPECTORUI_MODULE = WebInspectorUI
	endif
else
	WEBINSPECTORUI_MODULE = WebInspectorUI
endif

ifeq (iosmac,$(SDK_VARIANT))
	WEBINSPECTORUI_MODULE =
endif

MODULES = bmalloc WTF JavaScriptCore WebGPU ThirdParty WebCore $(WEBINSPECTORUI_MODULE) WebKitLegacy WebKit

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
	@$(build_target_for_each_module)
	cp -R ../Source/cmake $(SRCROOT)/Source/

endif # USE_WORKSPACE
