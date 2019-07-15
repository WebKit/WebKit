# Copyright (C) 2006, 2007, 2008, 2009, 2011, 2013, 2015 Apple Inc. All rights reserved.
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
# 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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
    $(JavaScriptCore)/wasm/js \
#

PYTHON = python
PERL = perl
RUBY = ruby

JavaScriptCore_SCRIPTS_DIR = $(JavaScriptCore)/Scripts

ifeq ($(OS),Windows_NT)
    DELETE = cmd //C del
else
    DELETE = rm -f
endif

# --------

.PHONY : all
all : \
    udis86_itab.h \
    InjectedScriptSource.h \
    IntlCanonicalizeLanguage.h \
    JSCBuiltins.h \
    Lexer.lut.h \
    KeywordLookup.h \
    RegExpJitTables.h \
    UnicodePatternTables.h \
    yarr/YarrCanonicalizeUnicode.cpp \
    WasmOps.h \
    WasmValidateInlines.h \
    WasmB3IRGeneratorInlines.h \
#

# JavaScript builtins.

BUILTINS_GENERATOR_SCRIPTS = \
    $(JavaScriptCore_SCRIPTS_DIR)/wkbuiltins/__init__.py \
    $(JavaScriptCore_SCRIPTS_DIR)/wkbuiltins/wkbuiltins.py \
    $(JavaScriptCore_SCRIPTS_DIR)/wkbuiltins/builtins_generator.py \
    $(JavaScriptCore_SCRIPTS_DIR)/wkbuiltins/builtins_model.py \
    $(JavaScriptCore_SCRIPTS_DIR)/wkbuiltins/builtins_templates.py \
    $(JavaScriptCore_SCRIPTS_DIR)/wkbuiltins/builtins_generate_combined_header.py \
    $(JavaScriptCore_SCRIPTS_DIR)/wkbuiltins/builtins_generate_combined_implementation.py \
    $(JavaScriptCore_SCRIPTS_DIR)/wkbuiltins/builtins_generate_separate_header.py \
    $(JavaScriptCore_SCRIPTS_DIR)/wkbuiltins/builtins_generate_separate_implementation.py \
    ${JavaScriptCore_SCRIPTS_DIR}/wkbuiltins/builtins_generate_wrapper_header.py \
    ${JavaScriptCore_SCRIPTS_DIR}/wkbuiltins/builtins_generate_wrapper_implementation.py \
    ${JavaScriptCore_SCRIPTS_DIR}/wkbuiltins/builtins_generate_internals_wrapper_header.py \
    ${JavaScriptCore_SCRIPTS_DIR}/wkbuiltins/builtins_generate_internals_wrapper_implementation.py \
    $(JavaScriptCore_SCRIPTS_DIR)/generate-js-builtins.py \
    $(JavaScriptCore_SCRIPTS_DIR)/lazywriter.py \
#

JavaScriptCore_BUILTINS_SOURCES = \
    $(JavaScriptCore)/builtins/AsyncFromSyncIteratorPrototype.js \
    $(JavaScriptCore)/builtins/ArrayConstructor.js \
    $(JavaScriptCore)/builtins/ArrayIteratorPrototype.js \
    $(JavaScriptCore)/builtins/ArrayPrototype.js \
    $(JavaScriptCore)/builtins/AsyncIteratorPrototype.js \
    $(JavaScriptCore)/builtins/AsyncFunctionPrototype.js \
    $(JavaScriptCore)/builtins/AsyncGeneratorPrototype.js \
    $(JavaScriptCore)/builtins/DatePrototype.js \
    $(JavaScriptCore)/builtins/FunctionPrototype.js \
    $(JavaScriptCore)/builtins/GeneratorPrototype.js \
    $(JavaScriptCore)/builtins/GlobalObject.js \
    $(JavaScriptCore)/builtins/GlobalOperations.js \
    $(JavaScriptCore)/builtins/InspectorInstrumentationObject.js \
    $(JavaScriptCore)/builtins/InternalPromiseConstructor.js \
    $(JavaScriptCore)/builtins/IteratorHelpers.js \
    $(JavaScriptCore)/builtins/IteratorPrototype.js \
    $(JavaScriptCore)/builtins/MapIteratorPrototype.js \
    $(JavaScriptCore)/builtins/MapPrototype.js \
    $(JavaScriptCore)/builtins/ModuleLoader.js \
    $(JavaScriptCore)/builtins/NumberConstructor.js \
    $(JavaScriptCore)/builtins/ObjectConstructor.js \
    $(JavaScriptCore)/builtins/PromiseConstructor.js \
    $(JavaScriptCore)/builtins/PromiseOperations.js \
    $(JavaScriptCore)/builtins/PromisePrototype.js \
    $(JavaScriptCore)/builtins/ReflectObject.js \
    $(JavaScriptCore)/builtins/RegExpPrototype.js \
    ${JavaScriptCore}/builtins/RegExpStringIteratorPrototype.js \
    $(JavaScriptCore)/builtins/SetIteratorPrototype.js \
    $(JavaScriptCore)/builtins/SetPrototype.js \
    $(JavaScriptCore)/builtins/StringConstructor.js \
    $(JavaScriptCore)/builtins/StringIteratorPrototype.js \
    $(JavaScriptCore)/builtins/StringPrototype.js \
    $(JavaScriptCore)/builtins/TypedArrayConstructor.js \
    $(JavaScriptCore)/builtins/TypedArrayPrototype.js \
    $(JavaScriptCore)/builtins/WebAssembly.js \
#

# The combined output file depends on the contents of builtins and generator scripts, so
# adding, modifying, or removing builtins or scripts will trigger regeneration of files.

JavaScriptCore_BUILTINS_DEPENDENCIES_LIST : $(JavaScriptCore_SCRIPTS_DIR)/UpdateContents.py DerivedSources.make
	$(PYTHON) $(JavaScriptCore_SCRIPTS_DIR)/UpdateContents.py '$(JavaScriptCore_BUILTINS_SOURCES) $(BUILTINS_GENERATOR_SCRIPTS)' $@

JSCBuiltins.h: $(BUILTINS_GENERATOR_SCRIPTS) $(JavaScriptCore_BUILTINS_SOURCES) JavaScriptCore_BUILTINS_DEPENDENCIES_LIST
	$(PYTHON) $(JavaScriptCore_SCRIPTS_DIR)/generate-js-builtins.py --combined --output-directory . --framework JavaScriptCore $(JavaScriptCore_BUILTINS_SOURCES)

# Perfect hash lookup tables for JavaScript classes.

OBJECT_LUT_HEADERS = \
    AsyncFromSyncIteratorPrototype.lut.h \
    ArrayConstructor.lut.h \
    AsyncGeneratorPrototype.lut.h \
    BigIntConstructor.lut.h \
    BigIntPrototype.lut.h \
    BooleanPrototype.lut.h \
    DateConstructor.lut.h \
    DatePrototype.lut.h \
    ErrorPrototype.lut.h \
    GeneratorPrototype.lut.h \
    InspectorInstrumentationObject.lut.h \
    IntlCollatorConstructor.lut.h \
    IntlCollatorPrototype.lut.h \
    IntlDateTimeFormatConstructor.lut.h \
    IntlDateTimeFormatPrototype.lut.h \
    IntlNumberFormatConstructor.lut.h \
    IntlNumberFormatPrototype.lut.h \
    IntlObject.lut.h \
    IntlPluralRulesConstructor.lut.h \
    IntlPluralRulesPrototype.lut.h \
    JSDataViewPrototype.lut.h \
    JSGlobalObject.lut.h \
    JSInternalPromiseConstructor.lut.h \
    JSModuleLoader.lut.h \
    JSONObject.lut.h \
    JSPromiseConstructor.lut.h \
    JSPromisePrototype.lut.h \
    JSWebAssembly.lut.h \
    MapPrototype.lut.h \
    NumberConstructor.lut.h \
    NumberPrototype.lut.h \
    ObjectConstructor.lut.h \
    ReflectObject.lut.h \
    RegExpConstructor.lut.h \
    SetPrototype.lut.h \
    StringConstructor.lut.h \
    StringPrototype.lut.h \
    SymbolConstructor.lut.h \
    SymbolPrototype.lut.h \
    WebAssemblyCompileErrorConstructor.lut.h \
    WebAssemblyCompileErrorPrototype.lut.h \
    WebAssemblyInstanceConstructor.lut.h \
    WebAssemblyInstancePrototype.lut.h \
    WebAssemblyLinkErrorConstructor.lut.h \
    WebAssemblyLinkErrorPrototype.lut.h \
    WebAssemblyMemoryConstructor.lut.h \
    WebAssemblyMemoryPrototype.lut.h \
    WebAssemblyModuleConstructor.lut.h \
    WebAssemblyModulePrototype.lut.h \
    WebAssemblyRuntimeErrorConstructor.lut.h \
    WebAssemblyRuntimeErrorPrototype.lut.h \
    WebAssemblyTableConstructor.lut.h \
    WebAssemblyTablePrototype.lut.h \
#

$(OBJECT_LUT_HEADERS): %.lut.h : %.cpp $(JavaScriptCore)/create_hash_table
	$(PERL) $(JavaScriptCore)/create_hash_table $< > $@

Lexer.lut.h: Keywords.table $(JavaScriptCore)/create_hash_table
	$(PERL) $(JavaScriptCore)/create_hash_table $< > $@

# character tables for Yarr

RegExpJitTables.h: yarr/create_regex_tables
	$(PYTHON) $^ > $@

KeywordLookup.h: KeywordLookupGenerator.py Keywords.table
	$(PYTHON) $^ > $@

# udis86 instruction tables

udis86_itab.h: $(JavaScriptCore)/disassembler/udis86/ud_itab.py $(JavaScriptCore)/disassembler/udis86/optable.xml
	$(PYTHON) $(JavaScriptCore)/disassembler/udis86/ud_itab.py $(JavaScriptCore)/disassembler/udis86/optable.xml .

# Bytecode files

BYTECODE_FILES = \
    Bytecodes.h \
    BytecodeIndices.h \
    BytecodeStructs.h \
    InitBytecodes.asm \
#
BYTECODE_FILES_PATTERNS = $(subst .,%,$(BYTECODE_FILES))

all : $(BYTECODE_FILES)

$(BYTECODE_FILES_PATTERNS): $(wildcard $(JavaScriptCore)/generator/*.rb) $(JavaScriptCore)/bytecode/BytecodeList.rb
	$(RUBY) $(JavaScriptCore)/generator/main.rb $(JavaScriptCore)/bytecode/BytecodeList.rb --bytecode_structs_h BytecodeStructs.h --init_bytecodes_asm InitBytecodes.asm --bytecodes_h Bytecodes.h --bytecode_indices_h BytecodeIndices.h

# Inspector interfaces

INSPECTOR_DOMAINS := \
    $(JavaScriptCore)/inspector/protocol/ApplicationCache.json \
    $(JavaScriptCore)/inspector/protocol/Audit.json \
    $(JavaScriptCore)/inspector/protocol/CSS.json \
    $(JavaScriptCore)/inspector/protocol/Canvas.json \
    $(JavaScriptCore)/inspector/protocol/Console.json \
    $(JavaScriptCore)/inspector/protocol/DOM.json \
    $(JavaScriptCore)/inspector/protocol/DOMDebugger.json \
    $(JavaScriptCore)/inspector/protocol/DOMStorage.json \
    $(JavaScriptCore)/inspector/protocol/Database.json \
    $(JavaScriptCore)/inspector/protocol/Debugger.json \
    $(JavaScriptCore)/inspector/protocol/GenericTypes.json \
    $(JavaScriptCore)/inspector/protocol/Heap.json \
    $(JavaScriptCore)/inspector/protocol/Inspector.json \
    $(JavaScriptCore)/inspector/protocol/LayerTree.json \
    $(JavaScriptCore)/inspector/protocol/Network.json \
    $(JavaScriptCore)/inspector/protocol/Page.json \
    $(JavaScriptCore)/inspector/protocol/Recording.json \
    $(JavaScriptCore)/inspector/protocol/Runtime.json \
    $(JavaScriptCore)/inspector/protocol/ScriptProfiler.json \
    $(JavaScriptCore)/inspector/protocol/Security.json \
    $(JavaScriptCore)/inspector/protocol/Target.json \
    $(JavaScriptCore)/inspector/protocol/Timeline.json \
    $(JavaScriptCore)/inspector/protocol/Worker.json \
#

ifeq ($(findstring ENABLE_INDEXED_DATABASE,$(FEATURE_DEFINES)), ENABLE_INDEXED_DATABASE)
    INSPECTOR_DOMAINS := $(INSPECTOR_DOMAINS) $(JavaScriptCore)/inspector/protocol/IndexedDB.json
endif

ifeq ($(findstring ENABLE_RESOURCE_USAGE,$(FEATURE_DEFINES)), ENABLE_RESOURCE_USAGE)
    INSPECTOR_DOMAINS := $(INSPECTOR_DOMAINS) $(JavaScriptCore)/inspector/protocol/CPUProfiler.json
    INSPECTOR_DOMAINS := $(INSPECTOR_DOMAINS) $(JavaScriptCore)/inspector/protocol/Memory.json
endif

ifeq ($(findstring ENABLE_SERVICE_WORKER,$(FEATURE_DEFINES)), ENABLE_SERVICE_WORKER)
    INSPECTOR_DOMAINS := $(INSPECTOR_DOMAINS) $(JavaScriptCore)/inspector/protocol/ServiceWorker.json
endif

INSPECTOR_GENERATOR_SCRIPTS = \
	$(JavaScriptCore)/inspector/scripts/codegen/__init__.py \
	$(JavaScriptCore)/inspector/scripts/codegen/cpp_generator_templates.py \
	$(JavaScriptCore)/inspector/scripts/codegen/cpp_generator.py \
	$(JavaScriptCore)/inspector/scripts/codegen/generate_cpp_backend_dispatcher_header.py \
	$(JavaScriptCore)/inspector/scripts/codegen/generate_cpp_backend_dispatcher_implementation.py \
	$(JavaScriptCore)/inspector/scripts/codegen/generate_cpp_frontend_dispatcher_header.py \
	$(JavaScriptCore)/inspector/scripts/codegen/generate_cpp_frontend_dispatcher_implementation.py \
	$(JavaScriptCore)/inspector/scripts/codegen/generate_cpp_protocol_types_header.py \
	$(JavaScriptCore)/inspector/scripts/codegen/generate_cpp_protocol_types_implementation.py \
	$(JavaScriptCore)/inspector/scripts/codegen/generate_js_backend_commands.py \
	$(JavaScriptCore)/inspector/scripts/codegen/generator_templates.py \
	$(JavaScriptCore)/inspector/scripts/codegen/generator.py \
	$(JavaScriptCore)/inspector/scripts/codegen/models.py \
	$(JavaScriptCore)/inspector/scripts/generate-inspector-protocol-bindings.py \
	$(JavaScriptCore_SCRIPTS_DIR)/generate-combined-inspector-json.py \
#

# TODO: Is there some way to not hardcode this? Can we get it from
# generate-inspector-protocol-bindings.py and ./CombinedDomains.json?
INSPECTOR_DISPATCHER_FILES = \
    inspector/InspectorAlternateBackendDispatchers.h \
    inspector/InspectorBackendCommands.js \
    inspector/InspectorBackendDispatchers.cpp \
    inspector/InspectorBackendDispatchers.h \
    inspector/InspectorFrontendDispatchers.cpp \
    inspector/InspectorFrontendDispatchers.h \
    inspector/InspectorProtocolObjects.cpp \
    inspector/InspectorProtocolObjects.h \
#
INSPECTOR_DISPATCHER_FILES_PATTERNS = $(subst .,%,$(INSPECTOR_DISPATCHER_FILES))

all : $(INSPECTOR_DISPATCHER_FILES)

# The combined JSON file depends on the actual set of domains and their file contents, so that
# adding, modifying, or removing domains will trigger regeneration of inspector files.

.PHONY: force
EnabledInspectorDomains : $(JavaScriptCore_SCRIPTS_DIR)/UpdateContents.py force
	$(PYTHON) $(JavaScriptCore_SCRIPTS_DIR)/UpdateContents.py '$(INSPECTOR_DOMAINS)' $@

CombinedDomains.json : $(JavaScriptCore_SCRIPTS_DIR)/generate-combined-inspector-json.py $(INSPECTOR_DOMAINS) EnabledInspectorDomains
	$(PYTHON) $(JavaScriptCore_SCRIPTS_DIR)/generate-combined-inspector-json.py $(INSPECTOR_DOMAINS) > ./CombinedDomains.json

# Inspector Backend Dispatchers, Frontend Dispatchers, Type Builders
$(INSPECTOR_DISPATCHER_FILES_PATTERNS) : CombinedDomains.json $(INSPECTOR_GENERATOR_SCRIPTS)
	$(PYTHON) $(JavaScriptCore)/inspector/scripts/generate-inspector-protocol-bindings.py --framework JavaScriptCore --outputDir inspector ./CombinedDomains.json

InjectedScriptSource.h : inspector/InjectedScriptSource.js $(JavaScriptCore_SCRIPTS_DIR)/jsmin.py $(JavaScriptCore_SCRIPTS_DIR)/xxd.pl
	echo "//# sourceURL=__InjectedScript_InjectedScriptSource.js" > ./InjectedScriptSource.min.js
	$(PYTHON) $(JavaScriptCore_SCRIPTS_DIR)/jsmin.py < $(JavaScriptCore)/inspector/InjectedScriptSource.js >> ./InjectedScriptSource.min.js
	$(PERL) $(JavaScriptCore_SCRIPTS_DIR)/xxd.pl InjectedScriptSource_js ./InjectedScriptSource.min.js InjectedScriptSource.h
	$(DELETE) InjectedScriptSource.min.js

AIR_OPCODE_FILES = \
    AirOpcode.h \
    AirOpcodeUtils.h \
    AirOpcodeGenerated.h \
#
AIR_OPCODE_FILES_PATTERNS = $(subst .,%,$(AIR_OPCODE_FILES))

all : $(AIR_OPCODE_FILES)

$(AIR_OPCODE_FILES_PATTERNS) : $(JavaScriptCore)/b3/air/opcode_generator.rb $(JavaScriptCore)/b3/air/AirOpcode.opcodes
	$(RUBY) $^

UnicodePatternTables.h: $(JavaScriptCore)/yarr/generateYarrUnicodePropertyTables.py $(JavaScriptCore)/yarr/hasher.py $(JavaScriptCore)/ucd/DerivedBinaryProperties.txt $(JavaScriptCore)/ucd/DerivedCoreProperties.txt $(JavaScriptCore)/ucd/DerivedNormalizationProps.txt $(JavaScriptCore)/ucd/PropList.txt $(JavaScriptCore)/ucd/PropertyAliases.txt $(JavaScriptCore)/ucd/PropertyValueAliases.txt $(JavaScriptCore)/ucd/ScriptExtensions.txt $(JavaScriptCore)/ucd/Scripts.txt $(JavaScriptCore)/ucd/UnicodeData.txt $(JavaScriptCore)/ucd/emoji-data.txt
	$(PYTHON) $(JavaScriptCore)/yarr/generateYarrUnicodePropertyTables.py $(JavaScriptCore)/ucd ./UnicodePatternTables.h

yarr/YarrCanonicalizeUnicode.cpp: $(JavaScriptCore)/yarr/generateYarrCanonicalizeUnicode $(JavaScriptCore)/ucd/CaseFolding.txt
	$(PYTHON) $(JavaScriptCore)/yarr/generateYarrCanonicalizeUnicode $(JavaScriptCore)/ucd/CaseFolding.txt ./yarr/YarrCanonicalizeUnicode.cpp

IntlCanonicalizeLanguage.h: $(JavaScriptCore)/Scripts/generateIntlCanonicalizeLanguage.py $(JavaScriptCore)/ucd/language-subtag-registry.txt
	$(PYTHON) $(JavaScriptCore)/Scripts/generateIntlCanonicalizeLanguage.py $(JavaScriptCore)/ucd/language-subtag-registry.txt ./IntlCanonicalizeLanguage.h

WasmOps.h: $(JavaScriptCore)/wasm/generateWasmOpsHeader.py $(JavaScriptCore)/wasm/generateWasm.py $(JavaScriptCore)/wasm/wasm.json
	$(PYTHON) $(JavaScriptCore)/wasm/generateWasmOpsHeader.py $(JavaScriptCore)/wasm/wasm.json ./WasmOps.h

WasmValidateInlines.h: $(JavaScriptCore)/wasm/generateWasmValidateInlinesHeader.py $(JavaScriptCore)/wasm/generateWasm.py $(JavaScriptCore)/wasm/wasm.json
	$(PYTHON) $(JavaScriptCore)/wasm/generateWasmValidateInlinesHeader.py $(JavaScriptCore)/wasm/wasm.json ./WasmValidateInlines.h

WasmB3IRGeneratorInlines.h: $(JavaScriptCore)/wasm/generateWasmB3IRGeneratorInlinesHeader.py $(JavaScriptCore)/wasm/generateWasm.py $(JavaScriptCore)/wasm/wasm.json
	$(PYTHON) $(JavaScriptCore)/wasm/generateWasmB3IRGeneratorInlinesHeader.py $(JavaScriptCore)/wasm/wasm.json ./WasmB3IRGeneratorInlines.h

# Dynamically-defined targets are listed below. Static targets belong up top.

all : \
    $(OBJECT_LUT_HEADERS) \
#

.PHONY : BytecodeCacheVersion.h

BytecodeCacheVersion.h:
	echo "#define JSC_BYTECODE_CACHE_VERSION $(shell date '+%s')" > BytecodeCacheVersion.h

all : BytecodeCacheVersion.h
