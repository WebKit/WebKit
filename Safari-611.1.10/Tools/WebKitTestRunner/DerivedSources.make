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

RUBY = ruby

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
#

IDL_ATTRIBUTES_FILE = $(WebCoreScripts)/IDLAttributes.json

.PHONY : all

JS%.h JS%.cpp : %.idl $(SCRIPTS) $(IDL_ATTRIBUTES_FILE)
	@echo Generating bindings for $*...
	@perl -I $(WebCoreScripts) -I $(WebKitTestRunner)/InjectedBundle/Bindings -I $(WebKitTestRunner)/UIScriptContext/Bindings $(WebCoreScripts)/generate-bindings.pl --defines "" --include InjectedBundle/Bindings --include UIScriptContext/Bindings --outputDir . --generator TestRunner --idlAttributesFile $(IDL_ATTRIBUTES_FILE) $<

all : \
    $(INJECTED_BUNDLE_INTERFACES:%=JS%.h) \
    $(INJECTED_BUNDLE_INTERFACES:%=JS%.cpp) \
    $(UICONTEXT_INTERFACES:%=JS%.h) \
    $(UICONTEXT_INTERFACES:%=JS%.cpp) \
#
