# Copyright (C) 2010-2020 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

VPATH = \
    $(WebKitTestRunner)/InjectedBundle/Bindings \
    $(WebKitTestRunner)/../TestRunnerShared/UIScriptContext/Bindings \
#

PERL = perl
RUBY = ruby

ifneq ($(SDKROOT),)
    SDK_FLAGS = -isysroot $(SDKROOT)
endif

WK_CURRENT_ARCH = $(word 1, $(ARCHS))
TARGET_TRIPLE_FLAGS = -target $(WK_CURRENT_ARCH)-$(LLVM_TARGET_TRIPLE_VENDOR)-$(LLVM_TARGET_TRIPLE_OS_VERSION)$(LLVM_TARGET_TRIPLE_SUFFIX)

FRAMEWORK_FLAGS := $(addprefix -F, $(BUILT_PRODUCTS_DIR) $(FRAMEWORK_SEARCH_PATHS) $(SYSTEM_FRAMEWORK_SEARCH_PATHS))
HEADER_FLAGS := $(addprefix -I, $(BUILT_PRODUCTS_DIR) $(HEADER_SEARCH_PATHS) $(SYSTEM_HEADER_SEARCH_PATHS))
EXTERNAL_FLAGS := -DRELEASE_WITHOUT_OPTIMIZATIONS $(addprefix -D, $(GCC_PREPROCESSOR_DEFINITIONS))

FEATURE_AND_PLATFORM_FLAGS_RESPONSE_FILE = platform-enabled-swift-args.$(WK_CURRENT_ARCH).resp
FEATURE_AND_PLATFORM_FLAGS := $(shell cat $(FEATURE_AND_PLATFORM_FLAGS_RESPONSE_FILE))
FEATURE_AND_PLATFORM_DEFINES := $(patsubst -D%, %, $(filter -D%, $(FEATURE_AND_PLATFORM_FLAGS)))

WEB_PREFERENCES_TEMPLATES = \
    $(WebKitTestRunner)/Scripts/PreferencesTemplates/TestOptionsGeneratedKeys.h.erb \
#
WEB_PREFERENCES_FILES = $(basename $(notdir $(WEB_PREFERENCES_TEMPLATES)))
WEB_PREFERENCES_PATTERNS = $(subst .erb,,$(WEB_PREFERENCES_FILES))

all : $(WEB_PREFERENCES_FILES)

$(WEB_PREFERENCES_PATTERNS) : $(WTF_BUILD_SCRIPTS_DIR)/GeneratePreferences.rb $(WEB_PREFERENCES_TEMPLATES) $(WTF_BUILD_SCRIPTS_DIR)/Preferences/UnifiedWebPreferences.yaml
	$(RUBY) $< --frontend WebKit $(addprefix --template , $(WEB_PREFERENCES_TEMPLATES)) $(WTF_BUILD_SCRIPTS_DIR)/Preferences/UnifiedWebPreferences.yaml


INJECTED_BUNDLE_INTERFACES = \
    AccessibilityController \
    AccessibilityTextMarker \
    AccessibilityTextMarkerRange \
    AccessibilityUIElement \
    EventSendingController \
    GCController \
    TestRunner \
    TextInputController \
#

UICONTEXT_INTERFACES = \
    UIScriptController \
#

SCRIPTS = \
    $(WebCoreScripts)/CodeGenerator.pm \
    $(WebKitTestRunner)/InjectedBundle/Bindings/CodeGeneratorTestRunner.pm \
    $(WebCoreScripts)/IDLParser.pm \
    $(WebCoreScripts)/generate-bindings.pl \
    $(WebCoreScripts)/preprocessor.pm \
#

IDL_ATTRIBUTES_FILE = $(WebCoreScripts)/IDLAttributes.json

IDL_FILE_NAMES_LIST = IDLFileNamesList.txt
$(IDL_FILE_NAMES_LIST) : $(INJECTED_BUNDLE_INTERFACES:%=%.idl) $(UICONTEXT_INTERFACES:%=%.idl)
	echo $^ | tr " " "\n" > $@

.PHONY : all

JS%.h JS%.cpp : %.idl $(SCRIPTS) $(IDL_ATTRIBUTES_FILE) $(IDL_FILE_NAMES_LIST) $(FEATURE_AND_PLATFORM_FLAGS_RESPONSE_FILE)
	@echo Generating bindings for $*...
	$(PERL) -I $(WebCoreScripts) -I $(WebKitTestRunner)/InjectedBundle/Bindings -I $(WebKitTestRunner)/UIScriptContext/Bindings $(WebCoreScripts)/generate-bindings.pl --defines "$(FEATURE_AND_PLATFORM_DEFINES)" --idlFileNamesList $(IDL_FILE_NAMES_LIST) --outputDir . --generator TestRunner --idlAttributesFile $(IDL_ATTRIBUTES_FILE) $<

all : \
    $(INJECTED_BUNDLE_INTERFACES:%=JS%.h) \
    $(INJECTED_BUNDLE_INTERFACES:%=JS%.cpp) \
    $(UICONTEXT_INTERFACES:%=JS%.h) \
    $(UICONTEXT_INTERFACES:%=JS%.cpp) \
#
