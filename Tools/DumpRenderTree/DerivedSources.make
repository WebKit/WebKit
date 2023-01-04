# Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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
FEATURE_AND_PLATFORM_DEFINES := $(shell $(CC) -std=c++2a -x c++ -E -P -dM $(SDK_FLAGS) $(TARGET_TRIPLE_FLAGS) $(FRAMEWORK_FLAGS) $(HEADER_FLAGS) $(EXTERNAL_FLAGS) -include "wtf/Platform.h" /dev/null | $(PERL) -ne "print if s/\#define ((HAVE_|USE_|ENABLE_|WTF_PLATFORM_)\w+) 1/\1/")

# FIXME: This should list Platform.h and all the things it includes. Could do that by using the -MD flag in the CC line above.
FEATURE_AND_PLATFORM_DEFINE_DEPENDENCIES = $(DumpRenderTree)/DerivedSources.make

UISCRIPTCONTEXT_DIR = $(DumpRenderTree)/../TestRunnerShared/UIScriptContext/Bindings
DUMPRENDERTREE_PREFERENCES_TEMPLATES_DIR = $(DumpRenderTree)/Scripts/PreferencesTemplates

VPATH = \
    $(UISCRIPTCONTEXT_DIR) \
    $(DUMPRENDERTREE_PREFERENCES_TEMPLATES_DIR) \
#

UICONTEXT_INTERFACES = \
    UIScriptController \
#

SCRIPTS = \
    $(WebCoreScripts)/CodeGenerator.pm \
    $(DumpRenderTree)/Bindings/CodeGeneratorDumpRenderTree.pm \
    $(WebCoreScripts)/IDLParser.pm \
    $(WebCoreScripts)/generate-bindings.pl \
#

IDL_ATTRIBUTES_FILE = $(WebCoreScripts)/IDLAttributes.json

.PHONY : all

JS%.h JS%.cpp : %.idl $(SCRIPTS) $(IDL_ATTRIBUTES_FILE) $(FEATURE_AND_PLATFORM_DEFINE_DEPENDENCIES)
	@echo Generating bindings for $*...
	$(PERL) -I $(WebCoreScripts) -I $(UISCRIPTCONTEXT_DIR) -I $(DumpRenderTree)/Bindings $(WebCoreScripts)/generate-bindings.pl --defines "$(FEATURE_AND_PLATFORM_DEFINES)" --include $(UISCRIPTCONTEXT_DIR) --outputDir . --generator DumpRenderTree --idlAttributesFile $(IDL_ATTRIBUTES_FILE) $<

all : \
    $(UICONTEXT_INTERFACES:%=JS%.h) \
    $(UICONTEXT_INTERFACES:%=JS%.cpp) \
#


WEB_PREFERENCES_GENERATED_FILES = \
    TestOptionsGeneratedWebKitLegacyKeyMapping.cpp \
    TestOptionsGeneratedKeys.h \
#

all : $(WEB_PREFERENCES_GENERATED_FILES)

$(WEB_PREFERENCES_GENERATED_FILES) : % : %.erb $(WTF_BUILD_SCRIPTS_DIR)/GeneratePreferences.rb $(WTF_BUILD_SCRIPTS_DIR)/Preferences/UnifiedWebPreferences.yaml
	$(RUBY) $(WTF_BUILD_SCRIPTS_DIR)/GeneratePreferences.rb --frontend WebKitLegacy --template $< $(WTF_BUILD_SCRIPTS_DIR)/Preferences/UnifiedWebPreferences.yaml
