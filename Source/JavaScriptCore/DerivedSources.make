# Copyright (C) 2006, 2007, 2008, 2009, 2011, 2013 Apple Inc. All rights reserved.
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
    $(JavaScriptCore)/runtime \
	$(JavaScriptCore)/interpreter \
	$(JavaScriptCore)/jit \
	$(JavaScriptCore)/builtins \
#

.PHONY : all
all : \
    ArrayConstructor.lut.h \
    ArrayPrototype.lut.h \
    BooleanPrototype.lut.h \
    DateConstructor.lut.h \
    DatePrototype.lut.h \
    ErrorPrototype.lut.h \
    JSDataViewPrototype.lut.h \
    JSONObject.lut.h \
    JSGlobalObject.lut.h \
    JSPromisePrototype.lut.h \
    JSPromiseConstructor.lut.h \
    KeywordLookup.h \
    Lexer.lut.h \
    NamePrototype.lut.h \
    NumberConstructor.lut.h \
    NumberPrototype.lut.h \
    ObjectConstructor.lut.h \
    RegExpConstructor.lut.h \
    RegExpPrototype.lut.h \
    RegExpJitTables.h \
    RegExpObject.lut.h \
    StringConstructor.lut.h \
    udis86_itab.h \
    JSCBuiltins \
#

# builtin functions
.PHONY: JSCBuiltins

JSCBuiltins: $(JavaScriptCore)/generate-js-builtins JSCBuiltins.h JSCBuiltins.cpp
JSCBuiltins.h: $(JavaScriptCore)/generate-js-builtins $(JavaScriptCore)/builtins/*.js
	python $^ $@
																				 
JSCBuiltins.cpp: JSCBuiltins.h

# lookup tables for classes

%.lut.h: create_hash_table %.cpp
	$^ -i > $@
Lexer.lut.h: create_hash_table Keywords.table
	$^ > $@

# character tables for Yarr

RegExpJitTables.h: create_regex_tables
	python $^ > $@

KeywordLookup.h: KeywordLookupGenerator.py Keywords.table
	python $^ > $@

# udis86 instruction tables

udis86_itab.h: $(JavaScriptCore)/disassembler/udis86/itab.py $(JavaScriptCore)/disassembler/udis86/optable.xml
	(PYTHONPATH=$(JavaScriptCore)/disassembler/udis86 python $(JavaScriptCore)/disassembler/udis86/itab.py $(JavaScriptCore)/disassembler/udis86/optable.xml || exit 1)


# Inspector interfaces

INSPECTOR_DOMAINS = \
    $(JavaScriptCore)/inspector/protocol/Debugger.json \
    $(JavaScriptCore)/inspector/protocol/GenericTypes.json \
    $(JavaScriptCore)/inspector/protocol/InspectorDomain.json \
    $(JavaScriptCore)/inspector/protocol/Runtime.json \
#

INSPECTOR_GENERATOR_SCRIPTS = \
	$(JavaScriptCore)/inspector/scripts/CodeGeneratorInspector.py \
	$(JavaScriptCore)/inspector/scripts/CodeGeneratorInspectorStrings.py \
#

all : \
    InspectorJS.json \
    InspectorJSFrontendDispatchers.h \
    InjectedScriptSource.h \
#

InspectorJS.json : inspector/scripts/generate-combined-inspector-json.py $(INSPECTOR_DOMAINS)
	python $(JavaScriptCore)/inspector/scripts/generate-combined-inspector-json.py $(JavaScriptCore)/inspector/protocol > ./InspectorJS.json

# Inspector Backend Dispatchers, Frontend Dispatchers, Type Builders
InspectorJSFrontendDispatchers.h : InspectorJS.json $(INSPECTOR_GENERATOR_SCRIPTS)
	python $(JavaScriptCore)/inspector/scripts/CodeGeneratorInspector.py ./InspectorJS.json --output_h_dir . --output_cpp_dir . --output_js_dir . --output_type JavaScript

InjectedScriptSource.h : inspector/InjectedScriptSource.js $(JavaScriptCore)/inspector/scripts/jsmin.py $(JavaScriptCore)/inspector/scripts/xxd.pl
	python $(JavaScriptCore)/inspector/scripts/jsmin.py < $(JavaScriptCore)/inspector/InjectedScriptSource.js > ./InjectedScriptSource.min.js
	perl $(JavaScriptCore)/inspector/scripts/xxd.pl InjectedScriptSource_js ./InjectedScriptSource.min.js InjectedScriptSource.h
	rm -f ./InjectedScriptSource.min.js
