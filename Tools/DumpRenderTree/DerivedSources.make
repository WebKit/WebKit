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

RUBY = ruby

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

JS%.h JS%.cpp : %.idl $(SCRIPTS) $(IDL_ATTRIBUTES_FILE)
	@echo Generating bindings for $*...
	@perl -I $(WebCoreScripts) -I $(UISCRIPTCONTEXT_DIR) -I $(DumpRenderTree)/Bindings $(WebCoreScripts)/generate-bindings.pl --defines "" --include $(UISCRIPTCONTEXT_DIR) --outputDir . --generator DumpRenderTree --idlAttributesFile $(IDL_ATTRIBUTES_FILE) $<

all : \
    $(UICONTEXT_INTERFACES:%=JS%.h) \
    $(UICONTEXT_INTERFACES:%=JS%.cpp) \
#


WEB_PREFERENCES = \
    ${WTF_BUILD_SCRIPTS_DIR}/Preferences/WebPreferences.yaml \
    ${WTF_BUILD_SCRIPTS_DIR}/Preferences/WebPreferencesDebug.yaml \
    ${WTF_BUILD_SCRIPTS_DIR}/Preferences/WebPreferencesExperimental.yaml \
    ${WTF_BUILD_SCRIPTS_DIR}/Preferences/WebPreferencesInternal.yaml \
#

WEB_PREFERENCES_GENERATED_FILES = \
    TestOptionsGeneratedWebKitLegacyKeyMapping.cpp \
    TestOptionsGeneratedKeys.h \
#

all : $(WEB_PREFERENCES_GENERATED_FILES)

$(WEB_PREFERENCES_GENERATED_FILES) : % : %.erb $(WEB_PREFERENCES) $(WTF_BUILD_SCRIPTS_DIR)/GeneratePreferences.rb
	$(RUBY) $(WTF_BUILD_SCRIPTS_DIR)/GeneratePreferences.rb --frontend WebKitLegacy --base ${WTF_BUILD_SCRIPTS_DIR}/Preferences/WebPreferences.yaml --debug ${WTF_BUILD_SCRIPTS_DIR}/Preferences/WebPreferencesDebug.yaml --experimental ${WTF_BUILD_SCRIPTS_DIR}/Preferences/WebPreferencesExperimental.yaml --internal ${WTF_BUILD_SCRIPTS_DIR}/Preferences/WebPreferencesInternal.yaml --template $<
