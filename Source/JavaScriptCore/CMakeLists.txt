set(JavaScriptCore_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${JAVASCRIPTCORE_DIR}"
    "${JAVASCRIPTCORE_DIR}/API"
    "${JAVASCRIPTCORE_DIR}/ForwardingHeaders"
    "${JAVASCRIPTCORE_DIR}/assembler"
    "${JAVASCRIPTCORE_DIR}/bytecode"
    "${JAVASCRIPTCORE_DIR}/bytecompiler"
    "${JAVASCRIPTCORE_DIR}/dfg"
    "${JAVASCRIPTCORE_DIR}/disassembler"
    "${JAVASCRIPTCORE_DIR}/heap"
    "${JAVASCRIPTCORE_DIR}/debugger"
    "${JAVASCRIPTCORE_DIR}/interpreter"
    "${JAVASCRIPTCORE_DIR}/jit"
    "${JAVASCRIPTCORE_DIR}/llint"
    "${JAVASCRIPTCORE_DIR}/parser"
    "${JAVASCRIPTCORE_DIR}/profiler"
    "${JAVASCRIPTCORE_DIR}/runtime"
    "${JAVASCRIPTCORE_DIR}/tools"
    "${JAVASCRIPTCORE_DIR}/yarr"
    "${WTF_DIR}"
    "${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}"
    "${CMAKE_SOURCE_DIR}/Source"
)

set(JavaScriptCore_SOURCES
    API/JSBase.cpp
    API/JSCallbackConstructor.cpp
    API/JSCallbackFunction.cpp
    API/JSCallbackObject.cpp
    API/JSClassRef.cpp
    API/JSContextRef.cpp
    API/JSObjectRef.cpp
    API/JSProfilerPrivate.cpp
    API/JSStringRef.cpp
    API/JSValueRef.cpp
    API/JSWeakObjectMapRefPrivate.cpp
    API/OpaqueJSString.cpp

    assembler/MacroAssembler.cpp
    assembler/LinkBuffer.cpp

    bytecode/ArrayAllocationProfile.cpp
    bytecode/ArrayProfile.cpp
    bytecode/CallLinkInfo.cpp
    bytecode/CallLinkStatus.cpp
    bytecode/CodeBlock.cpp
    bytecode/CodeBlockHash.cpp
    bytecode/CodeOrigin.cpp
    bytecode/CodeType.cpp
    bytecode/DFGExitProfile.cpp
    bytecode/ExecutionCounter.cpp
    bytecode/ExitKind.cpp
    bytecode/GetByIdStatus.cpp
    bytecode/JumpTable.cpp
    bytecode/LazyOperandValueProfile.cpp
    bytecode/MethodOfGettingAValueProfile.cpp
    bytecode/Opcode.cpp
    bytecode/PolymorphicPutByIdList.cpp
    bytecode/SpeculatedType.cpp
    bytecode/PutByIdStatus.cpp
    bytecode/ReduceWhitespace.cpp
    bytecode/ResolveGlobalStatus.cpp
    bytecode/SamplingTool.cpp
    bytecode/SpecialPointer.cpp
    bytecode/StructureStubClearingWatchpoint.cpp
    bytecode/StructureStubInfo.cpp
    bytecode/UnlinkedCodeBlock.cpp
    bytecode/Watchpoint.cpp

    bytecompiler/BytecodeGenerator.cpp
    bytecompiler/NodesCodegen.cpp

    dfg/DFGAbstractState.cpp
    dfg/DFGArgumentsSimplificationPhase.cpp
    dfg/DFGArrayMode.cpp
    dfg/DFGAssemblyHelpers.cpp
    dfg/DFGByteCodeParser.cpp
    dfg/DFGCapabilities.cpp
    dfg/DFGCFAPhase.cpp
    dfg/DFGCFGSimplificationPhase.cpp
    dfg/DFGConstantFoldingPhase.cpp
    dfg/DFGCSEPhase.cpp
    dfg/DFGDisassembler.cpp
    dfg/DFGDominators.cpp
    dfg/DFGDriver.cpp
    dfg/DFGFixupPhase.cpp
    dfg/DFGGraph.cpp
    dfg/DFGJITCompiler.cpp
    dfg/DFGMinifiedNode.cpp
    dfg/DFGNodeFlags.cpp
    dfg/DFGOSREntry.cpp
    dfg/DFGOSRExit.cpp
    dfg/DFGOSRExitCompiler.cpp
    dfg/DFGOSRExitCompiler32_64.cpp
    dfg/DFGOSRExitCompiler64.cpp
    dfg/DFGOperations.cpp
    dfg/DFGPhase.cpp
    dfg/DFGPredictionPropagationPhase.cpp
    dfg/DFGRepatch.cpp
    dfg/DFGSpeculativeJIT.cpp
    dfg/DFGSpeculativeJIT32_64.cpp
    dfg/DFGSpeculativeJIT64.cpp
    dfg/DFGStructureCheckHoistingPhase.cpp
    dfg/DFGThunks.cpp
    dfg/DFGValueSource.cpp
    dfg/DFGVariableAccessDataDump.cpp
    dfg/DFGVariableEvent.cpp
    dfg/DFGVariableEventStream.cpp
    dfg/DFGValidate.cpp
    dfg/DFGVirtualRegisterAllocationPhase.cpp

    disassembler/Disassembler.cpp

    heap/BlockAllocator.cpp
    heap/CopiedSpace.cpp
    heap/CopyVisitor.cpp
    heap/ConservativeRoots.cpp
    heap/DFGCodeBlocks.cpp
    heap/GCThread.cpp
    heap/GCThreadSharedData.cpp
    heap/HandleSet.cpp
    heap/HandleStack.cpp
    heap/Heap.cpp
    heap/HeapStatistics.cpp
    heap/HeapTimer.cpp
    heap/IncrementalSweeper.cpp
    heap/JITStubRoutineSet.cpp
    heap/MachineStackMarker.cpp
    heap/MarkedAllocator.cpp
    heap/MarkedBlock.cpp
    heap/MarkedSpace.cpp
    heap/MarkStack.cpp
    heap/SlotVisitor.cpp
    heap/WeakSet.cpp
    heap/WeakHandleOwner.cpp
    heap/WeakBlock.cpp

    debugger/Debugger.cpp
    debugger/DebuggerActivation.cpp
    debugger/DebuggerCallFrame.cpp

    interpreter/AbstractPC.cpp
    interpreter/CallFrame.cpp
    interpreter/Interpreter.cpp
    interpreter/JSStack.cpp
    interpreter/VMInspector.cpp

    jit/ClosureCallStubRoutine.cpp
    jit/ExecutableAllocator.cpp
    jit/HostCallReturnValue.cpp
    jit/GCAwareJITStubRoutine.cpp
    jit/JITArithmetic32_64.cpp
    jit/JITArithmetic.cpp
    jit/JITCall32_64.cpp
    jit/JITCall.cpp
    jit/JITCode.cpp
    jit/JIT.cpp
    jit/JITDisassembler.cpp
    jit/JITExceptions.cpp
    jit/JITOpcodes32_64.cpp
    jit/JITOpcodes.cpp
    jit/JITPropertyAccess32_64.cpp
    jit/JITPropertyAccess.cpp
    jit/JITStubRoutine.cpp
    jit/JITStubs.cpp
    jit/JumpReplacementWatchpoint.cpp
    jit/ThunkGenerators.cpp

    parser/Lexer.cpp
    parser/Nodes.cpp
    parser/Parser.cpp
    parser/ParserArena.cpp
    parser/SourceProviderCache.cpp

    profiler/ProfilerBytecode.cpp
    profiler/ProfilerBytecode.h
    profiler/ProfilerBytecodeSequence.cpp
    profiler/ProfilerBytecodes.cpp
    profiler/ProfilerBytecodes.h
    profiler/ProfilerCompilation.cpp
    profiler/ProfilerCompilation.h
    profiler/ProfilerCompilationKind.cpp
    profiler/ProfilerCompilationKind.h
    profiler/ProfilerCompiledBytecode.cpp
    profiler/ProfilerCompiledBytecode.h
    profiler/ProfilerDatabase.cpp
    profiler/ProfilerDatabase.h
    profiler/ProfilerExecutionCounter.h
    profiler/ProfilerOrigin.cpp
    profiler/ProfilerOrigin.h
    profiler/ProfilerOriginStack.cpp
    profiler/ProfilerOriginStack.h
    profiler/ProfilerOSRExit.cpp
    profiler/ProfilerOSRExitSite.cpp
    profiler/ProfilerProfiledBytecodes.cpp
    profiler/Profile.cpp
    profiler/ProfileGenerator.cpp
    profiler/ProfileNode.cpp
    profiler/LegacyProfiler.cpp

    runtime/ArgList.cpp
    runtime/Arguments.cpp
    runtime/ArrayConstructor.cpp
    runtime/ArrayPrototype.cpp
    runtime/BooleanConstructor.cpp
    runtime/BooleanObject.cpp
    runtime/BooleanPrototype.cpp
    runtime/CallData.cpp
    runtime/CodeCache.cpp
    runtime/CodeSpecializationKind.cpp
    runtime/CommonIdentifiers.cpp
    runtime/Completion.cpp
    runtime/ConstructData.cpp
    runtime/DateConstructor.cpp
    runtime/DateConversion.cpp
    runtime/DateInstance.cpp
    runtime/DatePrototype.cpp
    runtime/Error.cpp
    runtime/ErrorConstructor.cpp
    runtime/ErrorInstance.cpp
    runtime/ErrorPrototype.cpp
    runtime/ExceptionHelpers.cpp
    runtime/Executable.cpp
    runtime/FunctionConstructor.cpp
    runtime/FunctionPrototype.cpp
    runtime/GCActivityCallback.cpp
    runtime/GetterSetter.cpp
    runtime/Identifier.cpp
    runtime/IndexingType.cpp
    runtime/InitializeThreading.cpp
    runtime/InternalFunction.cpp
    runtime/JSActivation.cpp
    runtime/JSAPIValueWrapper.cpp
    runtime/JSArray.cpp
    runtime/JSCell.cpp
    runtime/JSDateMath.cpp
    runtime/JSFunction.cpp
    runtime/JSBoundFunction.cpp
    runtime/JSGlobalData.cpp
    runtime/JSGlobalObject.cpp
    runtime/JSGlobalObjectFunctions.cpp
    runtime/JSProxy.cpp
    runtime/JSLock.cpp
    runtime/JSNotAnObject.cpp
    runtime/JSObject.cpp
    runtime/JSONObject.cpp
    runtime/JSPropertyNameIterator.cpp
    runtime/JSSegmentedVariableObject.cpp
    runtime/JSNameScope.cpp
    runtime/JSWithScope.cpp
    runtime/JSScope.cpp
    runtime/JSString.cpp
    runtime/JSStringJoiner.cpp
    runtime/JSSymbolTableObject.cpp
    runtime/JSValue.cpp
    runtime/JSVariableObject.cpp
    runtime/JSWrapperObject.cpp
    runtime/LiteralParser.cpp
    runtime/Lookup.cpp
    runtime/MathObject.cpp
    runtime/MemoryStatistics.cpp
    runtime/NameConstructor.cpp
    runtime/NameConstructor.h
    runtime/NameInstance.cpp
    runtime/NameInstance.h
    runtime/NamePrototype.cpp
    runtime/NamePrototype.h
    runtime/NativeErrorConstructor.cpp
    runtime/NativeErrorPrototype.cpp
    runtime/NumberConstructor.cpp
    runtime/NumberObject.cpp
    runtime/NumberPrototype.cpp
    runtime/ObjectConstructor.cpp
    runtime/ObjectPrototype.cpp
    runtime/Operations.cpp
    runtime/Options.cpp
    runtime/PropertyDescriptor.cpp
    runtime/PropertyNameArray.cpp
    runtime/PropertySlot.cpp
    runtime/RegExp.cpp
    runtime/RegExpCache.cpp
    runtime/RegExpConstructor.cpp
    runtime/RegExpCachedResult.cpp
    runtime/RegExpMatchesArray.cpp
    runtime/RegExpObject.cpp
    runtime/RegExpPrototype.cpp
    runtime/SmallStrings.cpp
    runtime/SparseArrayValueMap.cpp
    runtime/StrictEvalActivation.cpp
    runtime/StringConstructor.cpp
    runtime/StringObject.cpp
    runtime/StringPrototype.cpp
    runtime/StringRecursionChecker.cpp
    runtime/Structure.cpp
    runtime/StructureChain.cpp
    runtime/SymbolTable.cpp
    runtime/TimeoutChecker.cpp

    tools/CodeProfile.cpp
    tools/CodeProfiling.cpp

    yarr/YarrCanonicalizeUCS2.cpp
    yarr/YarrPattern.cpp
    yarr/YarrInterpreter.cpp
    yarr/YarrJIT.cpp
    yarr/YarrSyntaxChecker.cpp
)

set(JavaScriptCore_LUT_FILES
    runtime/ArrayConstructor.cpp
    runtime/ArrayPrototype.cpp
    runtime/BooleanPrototype.cpp
    runtime/DateConstructor.cpp
    runtime/DatePrototype.cpp
    runtime/ErrorPrototype.cpp
    runtime/JSGlobalObject.cpp
    runtime/JSONObject.cpp
    runtime/MathObject.cpp
    runtime/NamePrototype.cpp
    runtime/NumberConstructor.cpp
    runtime/NumberPrototype.cpp
    runtime/ObjectConstructor.cpp
    runtime/ObjectPrototype.cpp
    runtime/RegExpConstructor.cpp
    runtime/RegExpObject.cpp
    runtime/RegExpPrototype.cpp
    runtime/StringConstructor.cpp
    runtime/StringPrototype.cpp
)

set(JavaScriptCore_LIBRARIES
    ${WTF_LIBRARY_NAME}
)

if (ENABLE_LLINT)
    # We cannot check for RUBY_FOUND because it is set only when the full package is installed and
    # the only thing we need is the interpreter. Unlike Python, cmake does not provide a macro
    # for finding the only Ruby interpreter.
    if (NOT RUBY_EXECUTABLE)
        message(FATAL_ERROR "The Ruby interpreter is needed to generate LLInt files.")
    endif ()

    set(LLINT_ASM
        llint/LowLevelInterpreter.asm
        llint/LowLevelInterpreter32_64.asm
        llint/LowLevelInterpreter64.asm
    )

    set(OFFLINE_ASM
        offlineasm/armv7.rb
        offlineasm/ast.rb
        offlineasm/backends.rb
        offlineasm/cloop.rb
        offlineasm/config.rb
        offlineasm/instructions.rb
        offlineasm/offsets.rb
        offlineasm/opt.rb
        offlineasm/parser.rb
        offlineasm/registers.rb
        offlineasm/risc.rb
        offlineasm/self_hash.rb
        offlineasm/settings.rb
        offlineasm/transform.rb
        offlineasm/x86.rb
    )

    add_custom_command(
        OUTPUT ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/LLIntDesiredOffsets.h
        MAIN_DEPENDENCY ${JAVASCRIPTCORE_DIR}/offlineasm/generate_offset_extractor.rb
        DEPENDS ${LLINT_ASM} ${OFFLINE_ASM}
        COMMAND ${RUBY_EXECUTABLE} ${JAVASCRIPTCORE_DIR}/offlineasm/generate_offset_extractor.rb ${JAVASCRIPTCORE_DIR}/llint/LowLevelInterpreter.asm ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/LLIntDesiredOffsets.h
        VERBATIM)

    # We add the header file directly to the ADD_EXECUTABLE call instead of setting the
    # OBJECT_DEPENDS property in LLIntOffsetsExtractor.cpp because generate_offset_extractor.rb may
    # not regenerate it in case the hash it calculates does not change.
    # In this case, if some of the dependencies specified in the ADD_CUSTOM_COMMAND above have
    # changed the command will always be called because the mtime of LLIntDesiredOffsets.h will
    # always be older than that of its dependencies.
    # Additionally, setting the OBJECT_DEPENDS property will make LLIntDesiredOffsets.h a Makefile
    # dependency of both LLIntOffsetsExtractor and LLIntOffsetsExtractor.cpp, so the command will
    # actually be run twice!
    add_executable(LLIntOffsetsExtractor
        ${JAVASCRIPTCORE_DIR}/llint/LLIntOffsetsExtractor.cpp
        ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/LLIntDesiredOffsets.h
    )
    target_link_libraries(LLIntOffsetsExtractor ${WTF_LIBRARY_NAME})

    # The build system will execute asm.rb every time LLIntOffsetsExtractor's mtime is newer than
    # LLIntAssembly.h's mtime. The problem we have here is: asm.rb has some built-in optimization
    # that generates a checksum of the LLIntOffsetsExtractor binary, if the checksum of the new
    # LLIntOffsetsExtractor matches, no output is generated. To make this target consistent and avoid
    # running this command for every build, we artificially update LLIntAssembly.h's mtime (using touch)
    # after every asm.rb run.
    add_custom_command(
        OUTPUT ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/LLIntAssembly.h
        MAIN_DEPENDENCY ${JAVASCRIPTCORE_DIR}/offlineasm/asm.rb
        DEPENDS LLIntOffsetsExtractor ${LLINT_ASM} ${OFFLINE_ASM}
        COMMAND ${RUBY_EXECUTABLE} ${JAVASCRIPTCORE_DIR}/offlineasm/asm.rb ${JAVASCRIPTCORE_DIR}/llint/LowLevelInterpreter.asm $<TARGET_FILE:LLIntOffsetsExtractor> ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/LLIntAssembly.h
        COMMAND ${CMAKE_COMMAND} -E touch_nocreate ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/LLIntAssembly.h
        VERBATIM)

    # The explanation for not making LLIntAssembly.h part of the OBJECT_DEPENDS property of some of
    # the .cpp files below is similar to the one in the previous comment. However, since these .cpp
    # files are used to build JavaScriptCore itself, we can just add LLIntAssembly.h to JSC_HEADERS
    # since it is used in the add_library() call at the end of this file.
    list(APPEND JavaScriptCore_HEADERS
        ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/LLIntAssembly.h
    )
    list(APPEND JavaScriptCore_SOURCES
        llint/LLIntCLoop.cpp
        llint/LLIntData.cpp
        llint/LLIntEntrypoints.cpp
        llint/LLIntExceptions.cpp
        llint/LLIntSlowPaths.cpp
        llint/LLIntThunks.cpp
        llint/LowLevelInterpreter.cpp
    )
endif ()

# GENERATOR 1-A: LUT creator
foreach (_file ${JavaScriptCore_LUT_FILES})
    get_filename_component(_name ${_file} NAME_WE)
    GENERATE_HASH_LUT(${JAVASCRIPTCORE_DIR}/${_file} ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/${_name}.lut.h)
    list(APPEND JavaScriptCore_HEADERS ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/${_name}.lut.h)
endforeach ()

set(JavaScriptCore_FORWARDING_HEADERS_DIRECTORIES
    assembler
    bytecode
    collector/handles
    debugger
    heap
    interpreter
    jit
    llint
    parser
    profiler
    runtime
    yarr
)

set(JavaScriptCore_FORWARDING_HEADERS_FILES
    API/APICast.h
    API/APIShims.h
    API/JavaScript.h
    API/JSBase.h
    API/JSContextRef.h
    API/JSContextRefPrivate.h
    API/JSObjectRef.h
    API/JSObjectRefPrivate.h
    API/JSStringRef.h
    API/JSStringRefCF.h
    API/JSStringRefBSTR.h
    API/JSValueRef.h
    API/JavaScriptCore.h
    API/JSRetainPtr.h
    API/JSWeakObjectMapRefInternal.h
    API/JSWeakObjectMapRefPrivate.h
    API/JSRetainPtr.h
    API/OpaqueJSString.h
    API/WebKitAvailability.h
)


# GENERATOR 1-B: particular LUT creator (for 1 file only)
GENERATE_HASH_LUT(${JAVASCRIPTCORE_DIR}/parser/Keywords.table ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/Lexer.lut.h MAIN_DEPENDENCY)
list(APPEND JavaScriptCore_HEADERS ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/Lexer.lut.h)

#GENERATOR: "RegExpJitTables.h": tables used by Yarr
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/RegExpJitTables.h
    MAIN_DEPENDENCY ${JAVASCRIPTCORE_DIR}/create_regex_tables
    COMMAND ${PYTHON_EXECUTABLE} ${JAVASCRIPTCORE_DIR}/create_regex_tables > ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/RegExpJitTables.h
    VERBATIM)
ADD_SOURCE_DEPENDENCIES(${JAVASCRIPTCORE_DIR}/yarr/YarrPattern.cpp ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/RegExpJitTables.h)


#GENERATOR: "KeywordLookup.h": keyword decision tree used by the lexer
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/KeywordLookup.h
    MAIN_DEPENDENCY ${JAVASCRIPTCORE_DIR}/KeywordLookupGenerator.py
    COMMAND ${PYTHON_EXECUTABLE} ${JAVASCRIPTCORE_DIR}/KeywordLookupGenerator.py ${JAVASCRIPTCORE_DIR}/parser/Keywords.table > ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/KeywordLookup.h
    VERBATIM)
ADD_SOURCE_DEPENDENCIES(${JAVASCRIPTCORE_DIR}/parser/Lexer.cpp ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/KeywordLookup.h)

if (WTF_CPU_ARM)
    list(APPEND JavaScriptCore_SOURCES
        assembler/ARMAssembler.cpp
        assembler/ARMv7Assembler.cpp
        assembler/MacroAssemblerARM.cpp
    )
elseif (WTF_CPU_MIPS)
elseif (WTF_CPU_X86)
elseif (WTF_CPU_X86_64)
else ()
    message(FATAL_ERROR "Unknown CPU")
endif ()


WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

WEBKIT_CREATE_FORWARDING_HEADERS(JavaScriptCore DIRECTORIES ${JavaScriptCore_FORWARDING_HEADERS_DIRECTORIES} FILES ${JavaScriptCore_FORWARDING_HEADERS_FILES})


add_subdirectory(shell)

WEBKIT_WRAP_SOURCELIST(${JavaScriptCore_SOURCES})
include_directories(${JavaScriptCore_INCLUDE_DIRECTORIES})
add_definitions(-DBUILDING_JavaScriptCore)
add_library(${JavaScriptCore_LIBRARY_NAME} ${JavaScriptCore_LIBRARY_TYPE} ${JavaScriptCore_HEADERS} ${JavaScriptCore_SOURCES})
target_link_libraries(${JavaScriptCore_LIBRARY_NAME} ${JavaScriptCore_LIBRARIES})
set_target_properties(${JavaScriptCore_LIBRARY_NAME} PROPERTIES FOLDER "JavaScriptCore")
set_target_properties(${JavaScriptCore_LIBRARY_NAME} PROPERTIES LINK_INTERFACE_LIBRARIES "")

if (SHARED_CORE)
    set_target_properties(${JavaScriptCore_LIBRARY_NAME} PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
    install(TARGETS ${JavaScriptCore_LIBRARY_NAME} DESTINATION "${LIB_INSTALL_DIR}")
endif ()
