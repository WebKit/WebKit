SET(JavaScriptCore_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${JAVASCRIPTCORE_DIR}"
    "${JAVASCRIPTCORE_DIR}/API"
    "${JAVASCRIPTCORE_DIR}/ForwardingHeaders"
    "${JAVASCRIPTCORE_DIR}/assembler"
    "${JAVASCRIPTCORE_DIR}/bytecode"
    "${JAVASCRIPTCORE_DIR}/bytecompiler"
    "${JAVASCRIPTCORE_DIR}/dfg"
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

SET(JavaScriptCore_SOURCES
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

    bytecode/CallLinkInfo.cpp
    bytecode/CallLinkStatus.cpp
    bytecode/CodeBlock.cpp
    bytecode/DFGExitProfile.cpp
    bytecode/ExecutionCounter.cpp
    bytecode/GetByIdStatus.cpp
    bytecode/JumpTable.cpp
    bytecode/LazyOperandValueProfile.cpp
    bytecode/MethodCallLinkInfo.cpp
    bytecode/MethodCallLinkStatus.cpp
    bytecode/MethodOfGettingAValueProfile.cpp
    bytecode/Opcode.cpp
    bytecode/PolymorphicPutByIdList.cpp
    bytecode/PredictedType.cpp
    bytecode/PutByIdStatus.cpp
    bytecode/SamplingTool.cpp
    bytecode/StructureStubInfo.cpp

    bytecompiler/BytecodeGenerator.cpp
    bytecompiler/NodesCodegen.cpp

    dfg/DFGAbstractState.cpp
    dfg/DFGAssemblyHelpers.cpp
    dfg/DFGByteCodeParser.cpp
    dfg/DFGCapabilities.cpp
    dfg/DFGCFAPhase.cpp
    dfg/DFGCorrectableJumpPoint.cpp
    dfg/DFGCSEPhase.cpp
    dfg/DFGDriver.cpp
    dfg/DFGFixupPhase.cpp
    dfg/DFGGraph.cpp
    dfg/DFGJITCompiler.cpp
    dfg/DFGNodeFlags.cpp
    dfg/DFGOSREntry.cpp
    dfg/DFGOSRExit.cpp
    dfg/DFGOSRExitCompiler.cpp
    dfg/DFGOSRExitCompiler32_64.cpp
    dfg/DFGOSRExitCompiler64.cpp
    dfg/DFGOperations.cpp
    dfg/DFGPhase.cpp
    dfg/DFGPredictionPropagationPhase.cpp
    dfg/DFGRedundantPhiEliminationPhase.cpp
    dfg/DFGRepatch.cpp
    dfg/DFGSpeculativeJIT.cpp
    dfg/DFGSpeculativeJIT32_64.cpp
    dfg/DFGSpeculativeJIT64.cpp
    dfg/DFGThunks.cpp
    dfg/DFGVirtualRegisterAllocationPhase.cpp

    heap/BlockAllocator.cpp
    heap/CopiedSpace.cpp
    heap/ConservativeRoots.cpp
    heap/DFGCodeBlocks.cpp
    heap/HandleSet.cpp
    heap/HandleStack.cpp
    heap/Heap.cpp
    heap/MachineStackMarker.cpp
    heap/MarkedAllocator.cpp
    heap/MarkedBlock.cpp
    heap/MarkedSpace.cpp
    heap/MarkStack.cpp
    heap/WeakSet.cpp
    heap/WeakHandleOwner.cpp
    heap/WeakBlock.cpp

    debugger/Debugger.cpp
    debugger/DebuggerActivation.cpp
    debugger/DebuggerCallFrame.cpp
    
    interpreter/AbstractPC.cpp
    interpreter/CallFrame.cpp
    interpreter/Interpreter.cpp
    interpreter/RegisterFile.cpp

    jit/ExecutableAllocator.cpp
    jit/HostCallReturnValue.cpp
    jit/JITArithmetic32_64.cpp
    jit/JITArithmetic.cpp
    jit/JITCall32_64.cpp
    jit/JITCall.cpp
    jit/JIT.cpp
    jit/JITExceptions.cpp
    jit/JITOpcodes32_64.cpp
    jit/JITOpcodes.cpp
    jit/JITPropertyAccess32_64.cpp
    jit/JITPropertyAccess.cpp
    jit/JITStubs.cpp
    jit/ThunkGenerators.cpp

    parser/Lexer.cpp
    parser/Nodes.cpp
    parser/Parser.cpp
    parser/ParserArena.cpp
    parser/SourceProviderCache.cpp

    profiler/Profile.cpp
    profiler/ProfileGenerator.cpp
    profiler/ProfileNode.cpp
    profiler/Profiler.cpp

    runtime/ArgList.cpp
    runtime/Arguments.cpp
    runtime/ArrayConstructor.cpp
    runtime/ArrayPrototype.cpp
    runtime/BooleanConstructor.cpp
    runtime/BooleanObject.cpp
    runtime/BooleanPrototype.cpp
    runtime/CallData.cpp
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
    runtime/JSGlobalThis.cpp
    runtime/JSLock.cpp
    runtime/JSNotAnObject.cpp
    runtime/JSObject.cpp
    runtime/JSONObject.cpp
    runtime/JSPropertyNameIterator.cpp
    runtime/JSStaticScopeObject.cpp
    runtime/JSString.cpp
    runtime/JSStringJoiner.cpp
    runtime/JSValue.cpp
    runtime/JSVariableObject.cpp
    runtime/JSWrapperObject.cpp
    runtime/LiteralParser.cpp
    runtime/Lookup.cpp
    runtime/MathObject.cpp
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
    runtime/ScopeChain.cpp
    runtime/SmallStrings.cpp
    runtime/StrictEvalActivation.cpp
    runtime/StringConstructor.cpp
    runtime/StringObject.cpp
    runtime/StringPrototype.cpp
    runtime/StringRecursionChecker.cpp
    runtime/Structure.cpp
    runtime/StructureChain.cpp
    runtime/TimeoutChecker.cpp
    runtime/UString.cpp

    tools/CodeProfile.cpp
    tools/CodeProfiling.cpp

    yarr/YarrCanonicalizeUCS2.cpp
    yarr/YarrPattern.cpp
    yarr/YarrInterpreter.cpp
    yarr/YarrJIT.cpp
    yarr/YarrSyntaxChecker.cpp
)

SET(JavaScriptCore_LUT_FILES
    runtime/ArrayConstructor.cpp
    runtime/ArrayPrototype.cpp
    runtime/BooleanPrototype.cpp
    runtime/DateConstructor.cpp
    runtime/DatePrototype.cpp
    runtime/ErrorPrototype.cpp
    runtime/JSGlobalObject.cpp
    runtime/JSONObject.cpp
    runtime/MathObject.cpp
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

SET(JavaScriptCore_LIBRARIES
    ${WTF_LIBRARY_NAME}
)


# GENERATOR 1-A: LUT creator
FOREACH (_file ${JavaScriptCore_LUT_FILES})
    GET_FILENAME_COMPONENT(_name ${_file} NAME_WE)
    GENERATE_HASH_LUT(${JAVASCRIPTCORE_DIR}/${_file} ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/${_name}.lut.h)
    LIST(APPEND JavaScriptCore_HEADERS ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/${_name}.lut.h)
ENDFOREACH ()


# GENERATOR 1-B: particular LUT creator (for 1 file only)
GENERATE_HASH_LUT(${JAVASCRIPTCORE_DIR}/parser/Keywords.table ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/Lexer.lut.h MAIN_DEPENDENCY)
LIST(APPEND JavaScriptCore_HEADERS ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/Lexer.lut.h)

#GENERATOR: "RegExpJitTables.h": tables used by Yarr
ADD_CUSTOM_COMMAND(
    OUTPUT ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/RegExpJitTables.h
    MAIN_DEPENDENCY ${JAVASCRIPTCORE_DIR}/create_regex_tables
    COMMAND ${PYTHON_EXECUTABLE} ${JAVASCRIPTCORE_DIR}/create_regex_tables > ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/RegExpJitTables.h
    VERBATIM)
ADD_SOURCE_DEPENDENCIES(${JAVASCRIPTCORE_DIR}/yarr/YarrPattern.cpp ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/RegExpJitTables.h)


#GENERATOR: "KeywordLookup.h": keyword decision tree used by the lexer
ADD_CUSTOM_COMMAND(
    OUTPUT ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/KeywordLookup.h
    MAIN_DEPENDENCY ${JAVASCRIPTCORE_DIR}/KeywordLookupGenerator.py
    COMMAND ${PYTHON_EXECUTABLE} ${JAVASCRIPTCORE_DIR}/KeywordLookupGenerator.py ${JAVASCRIPTCORE_DIR}/parser/Keywords.table > ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/KeywordLookup.h
    VERBATIM)
ADD_SOURCE_DEPENDENCIES(${JAVASCRIPTCORE_DIR}/parser/Lexer.cpp ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/KeywordLookup.h)

IF (WTF_CPU_ARM)
    LIST(APPEND JavaScriptCore_SOURCES
        assembler/ARMAssembler.cpp
        assembler/ARMv7Assembler.cpp
        assembler/MacroAssemblerARM.cpp
    )
ELSEIF (WTF_CPU_MIPS)
ELSEIF (WTF_CPU_X86)
ELSEIF (WTF_CPU_X86_64)
ELSE ()
    MESSAGE(FATAL_ERROR "Unknown CPU")
ENDIF ()


WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()


ADD_SUBDIRECTORY(shell)

WEBKIT_WRAP_SOURCELIST(${JavaScriptCore_SOURCES})
INCLUDE_DIRECTORIES(${JavaScriptCore_INCLUDE_DIRECTORIES})
ADD_DEFINITIONS(-DBUILDING_JavaScriptCore)
ADD_LIBRARY(${JavaScriptCore_LIBRARY_NAME} ${JavaScriptCore_LIBRARY_TYPE} ${JavaScriptCore_HEADERS} ${JavaScriptCore_SOURCES})
TARGET_LINK_LIBRARIES(${JavaScriptCore_LIBRARY_NAME} ${JavaScriptCore_LIBRARIES})
SET_TARGET_PROPERTIES(${JavaScriptCore_LIBRARY_NAME} PROPERTIES FOLDER "JavaScriptCore")
SET_TARGET_PROPERTIES(${JavaScriptCore_LIBRARY_NAME} PROPERTIES LINK_INTERFACE_LIBRARIES "")

IF (JavaScriptCore_LINK_FLAGS)
    ADD_TARGET_PROPERTIES(${JavaScriptCore_LIBRARY_NAME} LINK_FLAGS "${JavaScriptCore_LINK_FLAGS}")
ENDIF ()

IF (SHARED_CORE)
    SET_TARGET_PROPERTIES(${JavaScriptCore_LIBRARY_NAME} PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
    INSTALL(TARGETS ${JavaScriptCore_LIBRARY_NAME} DESTINATION "${LIB_INSTALL_DIR}")
ENDIF ()
