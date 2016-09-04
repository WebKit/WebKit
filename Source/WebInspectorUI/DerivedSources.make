# Copyright (C) 2016 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

VPATH = \
	$(WebInspectorUI) \
#

PERL = perl

# For Perl modules whose directory needs to be added to Perl's search path.
COMMON_SCRIPTS = \
	$(WEBCORE_PRIVATE_HEADERS_DIR)/preprocessor.pm \
#

SCRIPTS = \
	$(COMMON_SCRIPTS) \
	$(WebInspectorUI)/Scripts/preprocess-main-resource.pl \
#

.PHONY : all

PROCESSED_RESOURCE_FILES = \
	Main.html \
	Test.html \
#

COMMON_RESOURCE_DEFINES=$(ENGINEERING_BUILD_DEFINES)
TEST_RESOURCE_DEFINES=INCLUDE_TEST_RESOURCES $(COMMON_RESOURCE_DEFINES)
MAIN_RESOURCE_DEFINES=INCLUDE_UI_RESOURCES $(COMMON_RESOURCE_DEFINES)

all: \
	$(PROCESSED_RESOURCE_FILES) \
#

preprocess_main_resource = $(PERL) $(addprefix -I , $(sort $(dir $(1)))) $(WebInspectorUI)/Scripts/preprocess-main-resource.pl

Main.html : $(WebInspectorUI)/UserInterface/Inspector.html.in $(SCRIPTS)
	$(call preprocess_main_resource, $(COMMON_SCRIPTS)) --defines="$(MAIN_RESOURCE_DEFINES)" $< > $@

Test.html : $(WebInspectorUI)/UserInterface/Inspector.html.in $(SCRIPTS)
	$(call preprocess_main_resource, $(COMMON_SCRIPTS)) --defines="$(TEST_RESOURCE_DEFINES)" $< > $@
