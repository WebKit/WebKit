# Copyright (C) 2009 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

CONFIGURATION_BUILD_DIR ?= .
OUTPUT_DIR=$(CONFIGURATION_BUILD_DIR)

WRAPPER=$(OUTPUT_DIR)/DerivedSources/DumpRenderTree/DumpRenderTreeSupport_wrap.c
PERL_MODULE=$(OUTPUT_DIR)/DumpRenderTreeSupport.pm
DYLIB=$(OUTPUT_DIR)/DumpRenderTreeSupport.dylib

OSX_VERSION=$(shell sw_vers -productVersion | cut -d. -f 2)

ifneq "$(OSX_VERSION)" "4"


all: $(DYLIB) $(PERL_MODULE)

$(WRAPPER) $(PERL_MODULE): DumpRenderTreeSupport.c
	mkdir -p $$(dirname $(WRAPPER))
	swig -o $(WRAPPER) -outdir $(OUTPUT_DIR) -perl -module DumpRenderTreeSupport $<

$(DYLIB): DumpRenderTreeSupport.c $(WRAPPER)
	gcc -g -dynamiclib -o $(DYLIB) `perl -MExtUtils::Embed -eperl_inc` `perl -MExtUtils::Embed -eldopts` $^


else


all: $(PERL_MODULE)

$(PERL_MODULE): DumpRenderTreeSupportTiger.pm
	cp $^ $(PERL_MODULE)


endif

clean:
	rm -f $(WRAPPER) $(PERL_MODULE) $(DYLIB)

installhdrs installsrc install:
