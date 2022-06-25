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

ifeq ($(USE_LLVM_TARGET_TRIPLES_FOR_CLANG),YES)
    WK_CURRENT_ARCH = $(word 1, $(ARCHS))
    TARGET_TRIPLE_FLAGS = -target $(WK_CURRENT_ARCH)-$(LLVM_TARGET_TRIPLE_VENDOR)-$(LLVM_TARGET_TRIPLE_OS_VERSION)$(LLVM_TARGET_TRIPLE_SUFFIX)
endif

FRAMEWORK_FLAGS := $(shell echo $(BUILT_PRODUCTS_DIR) $(FRAMEWORK_SEARCH_PATHS) $(SYSTEM_FRAMEWORK_SEARCH_PATHS) | $(PERL) -e 'print "-F " . join(" -F ", split(" ", <>));')
HEADER_FLAGS := $(shell echo $(BUILT_PRODUCTS_DIR) $(HEADER_SEARCH_PATHS) $(SYSTEM_HEADER_SEARCH_PATHS) | $(PERL) -e 'print "-I" . join(" -I", split(" ", <>));')
FEATURE_AND_PLATFORM_DEFINES := $(shell $(CC) -std=c++2a -x c++ -E -P -dM $(SDK_FLAGS) $(TARGET_TRIPLE_FLAGS) $(FRAMEWORK_FLAGS) $(HEADER_FLAGS) -include "wtf/Platform.h" /dev/null | $(PERL) -ne "print if s/\#define ((HAVE_|USE_|ENABLE_|WTF_PLATFORM_)\w+) 1/\1/")

# FIXME: This should list Platform.h and all the things it includes. Could do that by using the -MD flag in the CC line above.
FEATURE_AND_PLATFORM_DEFINE_DEPENDENCIES = $(WebKitTestRunner)/DerivedSources.make

WEB_PREFERENCES = \
    ${WTF_BUILD_SCRIPTS_DIR}/Preferences/WebPreferences.yaml \
    ${WTF_BUILD_SCRIPTS_DIR}/Preferences/WebPreferencesDebug.yaml \
    ${WTF_BUILD_SCRIPTS_DIR}/Preferences/WebPreferencesExperimental.yaml \
    ${WTF_BUILD_SCRIPTS_DIR}/Preferences/WebPreferencesInternal.yaml \
#

WEB_PREFERENCES_TEMPLATES = \
    $(WebKitTestRunner)/Scripts/PreferencesTemplates/TestOptionsGeneratedKeys.h.erb \
#
WEB_PREFERENCES_FILES = $(basename $(notdir $(WEB_PREFERENCES_TEMPLATES)))
WEB_PREFERENCES_PATTERNS = $(subst .erb,,$(WEB_PREFERENCES_FILES))

all : $(WEB_PREFERENCES_FILES)

$(WEB_PREFERENCES_PATTERNS) : $(WTF_BUILD_SCRIPTS_DIR)/GeneratePreferences.rb $(WEB_PREFERENCES_TEMPLATES) $(WEB_PREFERENCES)
	$(RUBY) $< --frontend WebKit --base ${WTF_BUILD_SCRIPTS_DIR}/Preferences/WebPreferences.yaml --debug ${WTF_BUILD_SCRIPTS_DIR}/Preferences/WebPreferencesDebug.yaml --experimental ${WTF_BUILD_SCRIPTS_DIR}/Preferences/WebPreferencesExperimental.yaml --internal ${WTF_BUILD_SCRIPTS_DIR}/Preferences/WebPreferencesInternal.yaml $(addprefix --template , $(WEB_PREFERENCES_TEMPLATES))


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

.PHONY : all

JS%.h JS%.cpp : %.idl $(SCRIPTS) $(IDL_ATTRIBUTES_FILE) $(FEATURE_AND_PLATFORM_DEFINE_DEPENDENCIES)
	@echo Generating bindings for $*...
	$(PERL) -I $(WebCoreScripts) -I $(WebKitTestRunner)/InjectedBundle/Bindings -I $(WebKitTestRunner)/UIScriptContext/Bindings $(WebCoreScripts)/generate-bindings.pl --defines "$(FEATURE_AND_PLATFORM_DEFINES)" --include InjectedBundle/Bindings --include UIScriptContext/Bindings --outputDir . --generator TestRunner --idlAttributesFile $(IDL_ATTRIBUTES_FILE) $<

all : \
    $(INJECTED_BUNDLE_INTERFACES:%=JS%.h) \
    $(INJECTED_BUNDLE_INTERFACES:%=JS%.cpp) \
    $(UICONTEXT_INTERFACES:%=JS%.h) \
    $(UICONTEXT_INTERFACES:%=JS%.cpp) \
#
