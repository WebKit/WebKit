# Copyright (C) 2006, 2007, 2008, 2009, 2011 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer. 
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution. 
# 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
#     its contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission. 
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

VPATH = \
    $(JavaScriptCore) \
    $(JavaScriptCore)/parser \
    $(JavaScriptCore)/docs \
    $(JavaScriptCore)/runtime \
    $(JavaScriptCore)/interpreter \
    $(JavaScriptCore)/jit \
#

.PHONY : all
all : \
    ArrayPrototype.lut.h \
    DatePrototype.lut.h \
    HeaderDetection.h \
    JSONObject.lut.h \
    JavaScriptCore.JSVALUE32_64.exp \
    JavaScriptCore.JSVALUE64.exp \
    Lexer.lut.h \
    MathObject.lut.h \
    NumberConstructor.lut.h \
    ObjectConstructor.lut.h \
    RegExpConstructor.lut.h \
    RegExpJitTables.h \
    RegExpObject.lut.h \
    StringPrototype.lut.h \
    docs/bytecode.html \
#

# lookup tables for classes

%.lut.h: create_hash_table %.cpp
	$^ -i > $@
Lexer.lut.h: create_hash_table Keywords.table
	$^ > $@

docs/bytecode.html: make-bytecode-docs.pl Interpreter.cpp 
	perl $^ $@

# character tables for Yarr

RegExpJitTables.h: create_regex_tables
	python $^ > $@

# export files

JavaScriptCore.JSVALUE32_64.exp: JavaScriptCore.exp JavaScriptCore.JSVALUE32_64only.exp
	cat $^ > $@

JavaScriptCore.JSVALUE64.exp: JavaScriptCore.exp JavaScriptCore.JSVALUE64only.exp
	cat $^ > $@


# header detection

ifeq ($(OS),MACOS)

HeaderDetection.h : DerivedSources.make /System/Library/CoreServices/SystemVersion.plist
	rm -f $@
	echo "/* This is a generated file. Do not edit. */" > $@
	if [ -f $(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders/pthread_machdep.h ]; then echo "#define HAVE_PTHREAD_MACHDEP_H 1" >> $@; else echo >> $@; fi

else

HeaderDetection.h :
	echo > $@

endif
