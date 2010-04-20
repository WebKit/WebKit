##
## Copyright 2009, The Android Open Source Project
##
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions
## are met:
##  * Redistributions of source code must retain the above copyright
##    notice, this list of conditions and the following disclaimer.
##  * Redistributions in binary form must reproduce the above copyright
##    notice, this list of conditions and the following disclaimer in the
##    documentation and/or other materials provided with the distribution.
##
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
## EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
## IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
## PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
## CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
## EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
## PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
## PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
## OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
## (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
## OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
##

LOCAL_SRC_FILES := \
	API/JSValueRef.cpp \
	API/JSCallbackObject.cpp \
	API/OpaqueJSString.cpp \
	\
	bytecode/CodeBlock.cpp \
	bytecode/JumpTable.cpp \
	bytecode/Opcode.cpp \
	bytecode/SamplingTool.cpp \
	bytecode/StructureStubInfo.cpp \
	\
	bytecompiler/BytecodeGenerator.cpp \
	bytecompiler/NodesCodegen.cpp \
	\
	debugger/Debugger.cpp \
	debugger/DebuggerActivation.cpp \
	debugger/DebuggerCallFrame.cpp \
	\
	interpreter/CallFrame.cpp \
	interpreter/Interpreter.cpp \
	interpreter/RegisterFile.cpp \
	\
	jit/ExecutableAllocator.cpp\
	jit/ExecutableAllocatorFixedVMPool.cpp \
	jit/ExecutableAllocatorPosix.cpp \
	jit/JIT.cpp \
	jit/JITArithmetic.cpp \
	jit/JITCall.cpp \
	jit/JITOpcodes.cpp \
	jit/JITPropertyAccess.cpp \
	jit/JITStubs.cpp \
	\
	parser/Lexer.cpp \
	parser/Nodes.cpp \
	parser/Parser.cpp \
	parser/ParserArena.cpp \
	\
	pcre/pcre_compile.cpp \
	pcre/pcre_exec.cpp \
	pcre/pcre_tables.cpp \
	pcre/pcre_ucp_searchfuncs.cpp \
	pcre/pcre_xclass.cpp \
	\
	profiler/Profile.cpp \
	profiler/ProfileGenerator.cpp \
	profiler/ProfileNode.cpp \
	profiler/Profiler.cpp \
	\
	runtime/ArgList.cpp \
	runtime/Arguments.cpp \
	runtime/ArrayConstructor.cpp \
	runtime/ArrayPrototype.cpp \
	runtime/BooleanConstructor.cpp \
	runtime/BooleanObject.cpp \
	runtime/BooleanPrototype.cpp \
	runtime/CallData.cpp \
	runtime/Collector.cpp \
	runtime/CommonIdentifiers.cpp \
	runtime/Completion.cpp \
	runtime/ConstructData.cpp \
	runtime/DateConstructor.cpp \
	runtime/DateConversion.cpp \
	runtime/DateInstance.cpp \
	runtime/DatePrototype.cpp \
	runtime/Error.cpp \
	runtime/ErrorConstructor.cpp \
	runtime/ErrorInstance.cpp \
	runtime/ErrorPrototype.cpp \
	runtime/ExceptionHelpers.cpp \
	runtime/Executable.cpp \
	runtime/FunctionConstructor.cpp \
	runtime/FunctionPrototype.cpp \
	runtime/GetterSetter.cpp \
	runtime/GlobalEvalFunction.cpp \
	runtime/Identifier.cpp \
	runtime/InitializeThreading.cpp \
	runtime/InternalFunction.cpp \
	runtime/JSAPIValueWrapper.cpp \
	runtime/JSActivation.cpp \
	runtime/JSArray.cpp \
	runtime/JSByteArray.cpp \
	runtime/JSCell.cpp \
	runtime/JSFunction.cpp \
	runtime/JSGlobalData.cpp \
	runtime/JSGlobalObject.cpp \
	runtime/JSGlobalObjectFunctions.cpp \
	runtime/JSImmediate.cpp \
	runtime/JSLock.cpp \
	runtime/JSNotAnObject.cpp \
	runtime/JSNumberCell.cpp \
	runtime/JSONObject.cpp \
	runtime/JSObject.cpp \
	runtime/JSPropertyNameIterator.cpp \
	runtime/JSStaticScopeObject.cpp \
	runtime/JSString.cpp \
	runtime/JSValue.cpp \
	runtime/JSVariableObject.cpp \
	runtime/JSWrapperObject.cpp \
	runtime/LiteralParser.cpp \
	runtime/Lookup.cpp \
	runtime/MarkStack.cpp \
	runtime/MarkStackPosix.cpp \
	runtime/MathObject.cpp \
	runtime/NativeErrorConstructor.cpp \
	runtime/NativeErrorPrototype.cpp \
	runtime/NumberConstructor.cpp \
	runtime/NumberObject.cpp \
	runtime/NumberPrototype.cpp \
	runtime/ObjectConstructor.cpp \
	runtime/ObjectPrototype.cpp \
	runtime/Operations.cpp \
	runtime/PropertyDescriptor.cpp \
	runtime/PropertyNameArray.cpp \
	runtime/PropertySlot.cpp \
	runtime/PrototypeFunction.cpp \
	runtime/RegExp.cpp \
	runtime/RegExpConstructor.cpp \
	runtime/RegExpObject.cpp \
	runtime/RegExpPrototype.cpp \
	runtime/ScopeChain.cpp \
	runtime/SmallStrings.cpp \
	runtime/StringConstructor.cpp \
	runtime/StringObject.cpp \
	runtime/StringPrototype.cpp \
	runtime/Structure.cpp \
	runtime/StructureChain.cpp \
	runtime/TimeoutChecker.cpp \
	runtime/UString.cpp \
	\
	wtf/Assertions.cpp \
	wtf/ByteArray.cpp \
	wtf/CurrentTime.cpp \
	wtf/DateMath.cpp \
	wtf/FastMalloc.cpp \
	wtf/HashTable.cpp \
	wtf/MainThread.cpp \
	wtf/RandomNumber.cpp \
	wtf/RefCountedLeakCounter.cpp \
	wtf/TCSystemAlloc.cpp \
	wtf/ThreadIdentifierDataPthreads.cpp \
	wtf/Threading.cpp \
	wtf/ThreadingPthreads.cpp \
	wtf/WTFThreadData.cpp \
	\
	wtf/TypeTraits.cpp \
	wtf/dtoa.cpp \
	\
	wtf/android/MainThreadAndroid.cpp \
	\
	wtf/text/AtomicString.cpp \
	wtf/text/CString.cpp \
	wtf/text/StringImpl.cpp \
	wtf/text/WTFString.cpp \
	\
	wtf/unicode/CollatorDefault.cpp \
	wtf/unicode/UTF8.cpp \
	\
	wtf/unicode/icu/CollatorICU.cpp \
	\
	yarr/RegexCompiler.cpp \
	yarr/RegexInterpreter.cpp \
	yarr/RegexJIT.cpp

# Rule to build grammar.y with our custom bison.
GEN := $(intermediates)/parser/Grammar.cpp
$(GEN) : PRIVATE_YACCFLAGS := -p jscyy
$(GEN) : $(LOCAL_PATH)/parser/Grammar.y
	$(call local-transform-y-to-cpp,.cpp)
$(GEN) : $(LOCAL_BISON)
LOCAL_GENERATED_SOURCES += $(GEN)

# generated headers
JSC_OBJECTS := $(addprefix $(intermediates)/runtime/, \
				ArrayPrototype.lut.h \
				DatePrototype.lut.h \
				JSONObject.lut.h \
				MathObject.lut.h \
				NumberConstructor.lut.h \
				RegExpConstructor.lut.h \
				RegExpObject.lut.h \
				StringPrototype.lut.h \
			)
$(JSC_OBJECTS): PRIVATE_PATH := $(LOCAL_PATH)
$(JSC_OBJECTS): PRIVATE_CUSTOM_TOOL = perl $(PRIVATE_PATH)/create_hash_table $< -i > $@
$(JSC_OBJECTS): $(LOCAL_PATH)/create_hash_table
$(JSC_OBJECTS): $(intermediates)/%.lut.h : $(LOCAL_PATH)/%.cpp
	$(transform-generated-source)


LEXER_HEADER := $(intermediates)/Lexer.lut.h
$(LEXER_HEADER): PRIVATE_PATH := $(LOCAL_PATH)
$(LEXER_HEADER): PRIVATE_CUSTOM_TOOL = perl $(PRIVATE_PATH)/create_hash_table $< -i > $@
$(LEXER_HEADER): $(LOCAL_PATH)/create_hash_table
$(LEXER_HEADER): $(intermediates)/%.lut.h : $(LOCAL_PATH)/parser/Keywords.table
	$(transform-generated-source)

CHARTABLES := $(intermediates)/chartables.c
$(CHARTABLES): PRIVATE_PATH := $(LOCAL_PATH)
$(CHARTABLES): PRIVATE_CUSTOM_TOOL = perl $(PRIVATE_PATH)/pcre/dftables $@
$(CHARTABLES): $(LOCAL_PATH)/pcre/dftables
$(CHARTABLES): $(LOCAL_PATH)/pcre/pcre_internal.h
	$(transform-generated-source)

LOCAL_GENERATED_SOURCES += $(JSC_OBJECTS) $(LEXER_HEADER) $(CHARTABLES)
