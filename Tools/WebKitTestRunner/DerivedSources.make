# Copyright (C) 2010 Apple Inc. All rights reserved.
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

.PHONY : all print_all_generated_files

JS%.h JS%.cpp : %.idl $(SCRIPTS) $(IDL_ATTRIBUTES_FILE)
	@echo Generating bindings for $*...
	@perl -I $(WebCoreScripts) -I $(WebKitTestRunner)/InjectedBundle/Bindings -I $(WebKitTestRunner)/UIScriptContext/Bindings $(WebCoreScripts)/generate-bindings.pl --defines "" --include InjectedBundle/Bindings --include UIScriptContext/Bindings --outputDir . --generator TestRunner --idlAttributesFile $(IDL_ATTRIBUTES_FILE) $<

ALL_GENERATED_FILES = \
    $(INJECTED_BUNDLE_INTERFACES:%=JS%.h) \
    $(INJECTED_BUNDLE_INTERFACES:%=JS%.cpp) \
    $(UICONTEXT_INTERFACES:%=JS%.h) \
    $(UICONTEXT_INTERFACES:%=JS%.cpp) \
#

all : $(ALL_GENERATED_FILES)

print_all_generated_files :
	@for target in $(ALL_GENERATED_FILES); \
	do \
		echo $${target}; \
	done
