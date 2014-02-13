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

ifneq (,$(filter iphonesimulator, $(PLATFORM_NAME)))

TARGET_BUILD_DIR ?= .
OUTPUT_DIR=$(TARGET_BUILD_DIR)

WRAPPER_DIR=$(OUTPUT_DIR)/DerivedSources/DumpRenderTree
ifneq ($(PLATFORM_NAME),iphoneos)
WRAPPER=$(WRAPPER_DIR)/DumpRenderTreeSupport_wrap.c
PERL_MODULE=$(OUTPUT_DIR)/DumpRenderTreeSupport.pm
DYLIB=$(OUTPUT_DIR)/DumpRenderTreeSupport.dylib
endif
DUMPRENDERTREE=$(OUTPUT_DIR)/DumpRenderTree
PERL=/usr/bin/perl

DUMPRENDERTREE=$(OUTPUT_DIR)/DumpRenderTree.app/DumpRenderTree

# FIXME: We shouldn't hard code these architectures. See <rdar://problem/9047171> for more discussion on why we have to.
ARCHS=i386 x86_64


all: $(DYLIB) $(PERL_MODULE)

$(WRAPPER): DumpRenderTreeSupport_wrapPregenerated.c $(DUMPRENDERTREE)
	mkdir -p $(WRAPPER_DIR)
	cp DumpRenderTreeSupport_wrapPregenerated.c $(WRAPPER)

$(PERL_MODULE): DumpRenderTreeSupportPregenerated.pm $(DUMPRENDERTREE)
	cp DumpRenderTreeSupportPregenerated.pm $(PERL_MODULE)

$(DYLIB): DumpRenderTreeSupport.c $(WRAPPER)
	xcrun -sdk macosx clang -g -dynamiclib -o $(DYLIB) `$(PERL) -MExtUtils::Embed -eperl_inc` `$(PERL) -MExtUtils::Embed -e'my $$opts = ldopts(0); $$opts =~ s/-arch [^ ]*( |$$)//g; print $$opts, " -arch ", join(" -arch ", split(" ",$$ENV{ARCHS}))'` $^

IPHONE_SIMULATOR_NOTIFCATION_BUNDLE=$(OUTPUT_DIR)/lib/perl5/darwin-thread-multi-2level/auto/IPhoneSimulatorNotification/IPhoneSimulatorNotification.bundle
IPHONE_SIMULATOR_NOTIFCATION_MODULE=$(OUTPUT_DIR)/lib/perl5/darwin-thread-multi-2level/IPhoneSimulatorNotification.pm

all: $(IPHONE_SIMULATOR_NOTIFCATION_BUNDLE) $(IPHONE_SIMULATOR_NOTIFCATION_MODULE)

$(IPHONE_SIMULATOR_NOTIFCATION_BUNDLE) $(IPHONE_SIMULATOR_NOTIFCATION_MODULE): IPhoneSimulatorNotification/IPhoneSimulatorNotification.xs IPhoneSimulatorNotification/Makefile.PL IPhoneSimulatorNotification/lib/IPhoneSimulatorNotification.pm IPhoneSimulatorNotification/ppport.h IPhoneSimulatorNotification/t/IPhoneSimulatorNotification.t
	cd IPhoneSimulatorNotification && SDKROOT=iphonesimulator perl Makefile.PL INSTALL_BASE=$(OUTPUT_DIR) 
	make -C IPhoneSimulatorNotification test
	make -C IPhoneSimulatorNotification install
	make -C IPhoneSimulatorNotification realclean
	# Touch the built perl module and its bundle since IPhoneSimulatorNotification.xs and Makefile.PL always have newer dates after building.
	touch $(IPHONE_SIMULATOR_NOTIFCATION_BUNDLE) $(IPHONE_SIMULATOR_NOTIFCATION_MODULE)

clean:
	rm -f $(WRAPPER) $(PERL_MODULE) $(DYLIB)

installhdrs installsrc:

INSTALL_LOCATION=$(DSTROOT)/$(SYSTEM_LIBRARY_DIR)/Frameworks/$(WEBKIT_FRAMEWORK_RESOURCES_PATH)

install: all
	mkdir -p $(INSTALL_LOCATION)
	cp $(DYLIB) $(INSTALL_LOCATION)/DumpRenderTreeSupport.dylib
	cp $(PERL_MODULE) $(INSTALL_LOCATION)/DumpRenderTreeSupport.pm

else

install:

all:

clean:

installhdrs:

installsrc:

endif
