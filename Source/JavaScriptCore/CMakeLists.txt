set(JavaScriptCore_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${JAVASCRIPTCORE_DIR}"
    "${JAVASCRIPTCORE_DIR}/API"
    "${JAVASCRIPTCORE_DIR}/ForwardingHeaders"
    "${JAVASCRIPTCORE_DIR}/assembler"
    "${JAVASCRIPTCORE_DIR}/bindings"
    "${JAVASCRIPTCORE_DIR}/builtins"
    "${JAVASCRIPTCORE_DIR}/bytecode"
    "${JAVASCRIPTCORE_DIR}/bytecompiler"
    "${JAVASCRIPTCORE_DIR}/dfg"
    "${JAVASCRIPTCORE_DIR}/disassembler"
    "${JAVASCRIPTCORE_DIR}/ftl"
    "${JAVASCRIPTCORE_DIR}/heap"
    "${JAVASCRIPTCORE_DIR}/debugger"
    "${JAVASCRIPTCORE_DIR}/inspector"
    "${JAVASCRIPTCORE_DIR}/inspector/agents"
    "${JAVASCRIPTCORE_DIR}/interpreter"
    "${JAVASCRIPTCORE_DIR}/jit"
    "${JAVASCRIPTCORE_DIR}/llint"
    "${JAVASCRIPTCORE_DIR}/llvm"
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
    API/JSCTestRunnerUtils.cpp
    API/JSCallbackConstructor.cpp
    API/JSCallbackFunction.cpp
    API/JSCallbackObject.cpp
    API/JSClassRef.cpp
    API/JSContextRef.cpp
    API/JSObjectRef.cpp
    API/JSProfilerPrivate.cpp
    API/JSScriptRef.cpp
    API/JSStringRef.cpp
    API/JSValueRef.cpp
    API/JSWeakObjectMapRefPrivate.cpp
    API/OpaqueJSString.cpp

    assembler/LinkBuffer.cpp
    assembler/MacroAssembler.cpp
    assembler/MacroAssemblerX86Common.cpp

    bindings/ScriptFunctionCall.cpp
    bindings/ScriptObject.cpp
    bindings/ScriptValue.cpp

    builtins/BuiltinExecutables.cpp

    bytecode/ArrayAllocationProfile.cpp
    bytecode/ArrayProfile.cpp
    bytecode/BytecodeBasicBlock.cpp
    bytecode/BytecodeLivenessAnalysis.cpp
    bytecode/CallLinkInfo.cpp
    bytecode/CallLinkStatus.cpp
    bytecode/CodeBlock.cpp
    bytecode/CodeBlockHash.cpp
    bytecode/CodeBlockJettisoningWatchpoint.cpp
    bytecode/CodeOrigin.cpp
    bytecode/CodeType.cpp
    bytecode/DFGExitProfile.cpp
    bytecode/DeferredCompilationCallback.cpp
    bytecode/ExecutionCounter.cpp
    bytecode/ExitKind.cpp
    bytecode/ExitingJITType.cpp
    bytecode/GetByIdStatus.cpp
    bytecode/GetByIdVariant.cpp
    bytecode/InlineCallFrameSet.cpp
    bytecode/JumpTable.cpp
    bytecode/LazyOperandValueProfile.cpp
    bytecode/MethodOfGettingAValueProfile.cpp
    bytecode/Opcode.cpp
    bytecode/PolymorphicPutByIdList.cpp
    bytecode/PreciseJumpTargets.cpp
    bytecode/ProfiledCodeBlockJettisoningWatchpoint.cpp
    bytecode/PutByIdStatus.cpp
    bytecode/PutByIdVariant.cpp
    bytecode/ReduceWhitespace.cpp
    bytecode/SamplingTool.cpp
    bytecode/SpecialPointer.cpp
    bytecode/SpeculatedType.cpp
    bytecode/StructureStubClearingWatchpoint.cpp
    bytecode/StructureStubInfo.cpp
    bytecode/UnlinkedCodeBlock.cpp
    bytecode/UnlinkedInstructionStream.cpp
    bytecode/ValueRecovery.cpp
    bytecode/Watchpoint.cpp

    bytecompiler/BytecodeGenerator.cpp
    bytecompiler/NodesCodegen.cpp

    debugger/Debugger.cpp
    debugger/DebuggerActivation.cpp
    debugger/DebuggerCallFrame.cpp

    dfg/DFGAbstractHeap.cpp
    dfg/DFGAbstractValue.cpp
    dfg/DFGArgumentsSimplificationPhase.cpp
    dfg/DFGArithMode.cpp
    dfg/DFGArrayMode.cpp
    dfg/DFGAtTailAbstractState.cpp
    dfg/DFGAvailability.cpp
    dfg/DFGBackwardsPropagationPhase.cpp
    dfg/DFGBasicBlock.cpp
    dfg/DFGBinarySwitch.cpp
    dfg/DFGBlockInsertionSet.cpp
    dfg/DFGByteCodeParser.cpp
    dfg/DFGCFAPhase.cpp
    dfg/DFGCFGSimplificationPhase.cpp
    dfg/DFGCPSRethreadingPhase.cpp
    dfg/DFGCSEPhase.cpp
    dfg/DFGCapabilities.cpp
    dfg/DFGClobberSet.cpp
    dfg/DFGClobberize.cpp
    dfg/DFGCommon.cpp
    dfg/DFGCommonData.cpp
    dfg/DFGCompilationKey.cpp
    dfg/DFGCompilationMode.cpp
    dfg/DFGConstantFoldingPhase.cpp
    dfg/DFGCriticalEdgeBreakingPhase.cpp
    dfg/DFGDCEPhase.cpp
    dfg/DFGDesiredIdentifiers.cpp
    dfg/DFGDesiredStructureChains.cpp
    dfg/DFGDesiredTransitions.cpp
    dfg/DFGDesiredWatchpoints.cpp
    dfg/DFGDesiredWeakReferences.cpp
    dfg/DFGDesiredWriteBarriers.cpp
    dfg/DFGDisassembler.cpp
    dfg/DFGDominators.cpp
    dfg/DFGDriver.cpp
    dfg/DFGEdge.cpp
    dfg/DFGFailedFinalizer.cpp
    dfg/DFGFinalizer.cpp
    dfg/DFGFixupPhase.cpp
    dfg/DFGFlushFormat.cpp
    dfg/DFGFlushLivenessAnalysisPhase.cpp
    dfg/DFGFlushedAt.cpp
    dfg/DFGGraph.cpp
    dfg/DFGGraphSafepoint.cpp
    dfg/DFGInPlaceAbstractState.cpp
    dfg/DFGIntegerCheckCombiningPhase.cpp
    dfg/DFGInvalidationPointInjectionPhase.cpp
    dfg/DFGJITCode.cpp
    dfg/DFGJITCompiler.cpp
    dfg/DFGJITFinalizer.cpp
    dfg/DFGJumpReplacement.cpp
    dfg/DFGLICMPhase.cpp
    dfg/DFGLazyJSValue.cpp
    dfg/DFGLivenessAnalysisPhase.cpp
    dfg/DFGLongLivedState.cpp
    dfg/DFGLoopPreHeaderCreationPhase.cpp
    dfg/DFGMinifiedNode.cpp
    dfg/DFGNaturalLoops.cpp
    dfg/DFGNode.cpp
    dfg/DFGNodeFlags.cpp
    dfg/DFGOSRAvailabilityAnalysisPhase.cpp
    dfg/DFGOSREntry.cpp
    dfg/DFGOSREntrypointCreationPhase.cpp
    dfg/DFGOSRExit.cpp
    dfg/DFGOSRExitBase.cpp
    dfg/DFGOSRExitCompiler.cpp
    dfg/DFGOSRExitCompiler32_64.cpp
    dfg/DFGOSRExitCompiler64.cpp
    dfg/DFGOSRExitCompilerCommon.cpp
    dfg/DFGOSRExitJumpPlaceholder.cpp
    dfg/DFGOSRExitPreparation.cpp
    dfg/DFGOperations.cpp
    dfg/DFGPhase.cpp
    dfg/DFGPlan.cpp
    dfg/DFGPredictionInjectionPhase.cpp
    dfg/DFGPredictionPropagationPhase.cpp
    dfg/DFGResurrectionForValidationPhase.cpp
    dfg/DFGSSAConversionPhase.cpp
    dfg/DFGSSALoweringPhase.cpp
    dfg/DFGSafepoint.cpp
    dfg/DFGSpeculativeJIT.cpp
    dfg/DFGSpeculativeJIT32_64.cpp
    dfg/DFGSpeculativeJIT64.cpp
    dfg/DFGStackLayoutPhase.cpp
    dfg/DFGStaticExecutionCountEstimationPhase.cpp
    dfg/DFGStoreBarrierElisionPhase.cpp
    dfg/DFGStrengthReductionPhase.cpp
    dfg/DFGThreadData.cpp
    dfg/DFGThunks.cpp
    dfg/DFGTierUpCheckInjectionPhase.cpp
    dfg/DFGTypeCheckHoistingPhase.cpp
    dfg/DFGUnificationPhase.cpp
    dfg/DFGUseKind.cpp
    dfg/DFGValidate.cpp
    dfg/DFGValueSource.cpp
    dfg/DFGVariableAccessDataDump.cpp
    dfg/DFGVariableEvent.cpp
    dfg/DFGVariableEventStream.cpp
    dfg/DFGVirtualRegisterAllocationPhase.cpp
    dfg/DFGWatchpointCollectionPhase.cpp
    dfg/DFGWorklist.cpp

    disassembler/ARMv7Disassembler.cpp
    disassembler/Disassembler.cpp
    disassembler/LLVMDisassembler.cpp
    disassembler/UDis86Disassembler.cpp
    disassembler/X86Disassembler.cpp

    heap/BlockAllocator.cpp
    heap/CodeBlockSet.cpp
    heap/ConservativeRoots.cpp
    heap/CopiedSpace.cpp
    heap/CopyVisitor.cpp
    heap/DeferGC.cpp
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
    heap/MarkStack.cpp
    heap/MarkedAllocator.cpp
    heap/MarkedBlock.cpp
    heap/MarkedSpace.cpp
    heap/SlotVisitor.cpp
    heap/SuperRegion.cpp
    heap/Weak.cpp
    heap/WeakBlock.cpp
    heap/WeakHandleOwner.cpp
    heap/WeakSet.cpp
    heap/WriteBarrierBuffer.cpp
    heap/WriteBarrierSupport.cpp

    inspector/ConsoleMessage.cpp
    inspector/ContentSearchUtilities.cpp
    inspector/IdentifiersFactory.cpp
    inspector/InjectedScript.cpp
    inspector/InjectedScriptBase.cpp
    inspector/InjectedScriptHost.cpp
    inspector/InjectedScriptManager.cpp
    inspector/InjectedScriptModule.cpp
    inspector/InspectorAgentRegistry.cpp
    inspector/InspectorBackendDispatcher.cpp
    inspector/InspectorValues.cpp
    inspector/JSInjectedScriptHost.cpp
    inspector/JSInjectedScriptHostPrototype.cpp
    inspector/JSJavaScriptCallFrame.cpp
    inspector/JSJavaScriptCallFramePrototype.cpp
    inspector/JavaScriptCallFrame.cpp
    inspector/ScriptArguments.cpp
    inspector/ScriptCallFrame.cpp
    inspector/ScriptCallStack.cpp
    inspector/ScriptCallStackFactory.cpp
    inspector/ScriptDebugServer.cpp
    inspector/agents/InspectorAgent.cpp
    inspector/agents/InspectorConsoleAgent.cpp
    inspector/agents/InspectorDebuggerAgent.cpp
    inspector/agents/InspectorRuntimeAgent.cpp

    interpreter/AbstractPC.cpp
    interpreter/CallFrame.cpp
    interpreter/Interpreter.cpp
    interpreter/JSStack.cpp
    interpreter/ProtoCallFrame.cpp
    interpreter/StackVisitor.cpp
    interpreter/VMInspector.cpp

    jit/AssemblyHelpers.cpp
    jit/ArityCheckFailReturnThunks.cpp
    jit/ClosureCallStubRoutine.cpp
    jit/ExecutableAllocator.cpp
    jit/ExecutableAllocatorFixedVMPool.cpp
    jit/GCAwareJITStubRoutine.cpp
    jit/HostCallReturnValue.cpp
    jit/JIT.cpp
    jit/JITArithmetic.cpp
    jit/JITArithmetic32_64.cpp
    jit/JITCall.cpp
    jit/JITCall32_64.cpp
    jit/JITCode.cpp
    jit/JITDisassembler.cpp
    jit/JITExceptions.cpp
    jit/JITInlineCacheGenerator.cpp
    jit/JITOpcodes.cpp
    jit/JITOpcodes32_64.cpp
    jit/JITOperations.cpp
    jit/JITPropertyAccess.cpp
    jit/JITPropertyAccess32_64.cpp
    jit/JITStubRoutine.cpp
    jit/JITStubs.cpp
    jit/JITThunks.cpp
    jit/JITToDFGDeferredCompilationCallback.cpp
    jit/Reg.cpp
    jit/RegisterPreservationWrapperGenerator.cpp
    jit/RegisterSet.cpp
    jit/Repatch.cpp
    jit/TempRegisterSet.cpp
    jit/ThunkGenerators.cpp

    parser/Lexer.cpp
    parser/Nodes.cpp
    parser/Parser.cpp
    parser/ParserArena.cpp
    parser/SourceCode.cpp
    parser/SourceProvider.cpp
    parser/SourceProviderCache.cpp

    profiler/LegacyProfiler.cpp
    profiler/Profile.cpp
    profiler/ProfileGenerator.cpp
    profiler/ProfileNode.cpp
    profiler/ProfilerBytecode.cpp
    profiler/ProfilerBytecodeSequence.cpp
    profiler/ProfilerBytecodes.cpp
    profiler/ProfilerCompilation.cpp
    profiler/ProfilerCompilationKind.cpp
    profiler/ProfilerCompiledBytecode.cpp
    profiler/ProfilerDatabase.cpp
    profiler/ProfilerJettisonReason.cpp
    profiler/ProfilerOSRExit.cpp
    profiler/ProfilerOSRExitSite.cpp
    profiler/ProfilerOrigin.cpp
    profiler/ProfilerOriginStack.cpp
    profiler/ProfilerProfiledBytecodes.cpp

    runtime/ArgList.cpp
    runtime/Arguments.cpp
    runtime/ArgumentsIteratorConstructor.cpp
    runtime/ArgumentsIteratorPrototype.cpp
    runtime/ArrayBuffer.cpp
    runtime/ArrayBufferNeuteringWatchpoint.cpp
    runtime/ArrayBufferView.cpp
    runtime/ArrayConstructor.cpp
    runtime/ArrayIteratorConstructor.cpp
    runtime/ArrayIteratorPrototype.cpp
    runtime/ArrayPrototype.cpp
    runtime/BooleanConstructor.cpp
    runtime/BooleanObject.cpp
    runtime/BooleanPrototype.cpp
    runtime/CallData.cpp
    runtime/CodeCache.cpp
    runtime/CodeSpecializationKind.cpp
    runtime/CommonIdentifiers.cpp
    runtime/CommonSlowPaths.cpp
    runtime/CommonSlowPathsExceptions.cpp
    runtime/CompilationResult.cpp
    runtime/Completion.cpp
    runtime/ConstructData.cpp
    runtime/DataView.cpp
    runtime/DataView.h
    runtime/DateConstructor.cpp
    runtime/DateConversion.cpp
    runtime/DateInstance.cpp
    runtime/DatePrototype.cpp
    runtime/DumpContext.cpp
    runtime/Error.cpp
    runtime/ErrorConstructor.cpp
    runtime/ErrorHandlingScope.cpp
    runtime/ErrorInstance.cpp
    runtime/ErrorPrototype.cpp
    runtime/ExceptionHelpers.cpp
    runtime/Executable.cpp
    runtime/FunctionConstructor.cpp
    runtime/FunctionExecutableDump.cpp
    runtime/FunctionPrototype.cpp
    runtime/GCActivityCallback.cpp
    runtime/GetterSetter.cpp
    runtime/Identifier.cpp
    runtime/IndexingType.cpp
    runtime/InitializeThreading.cpp
    runtime/IntendedStructureChain.cpp
    runtime/InternalFunction.cpp
    runtime/JSAPIValueWrapper.cpp
    runtime/JSActivation.cpp
    runtime/JSArgumentsIterator.cpp
    runtime/JSArray.cpp
    runtime/JSArrayBuffer.cpp
    runtime/JSArrayBufferConstructor.cpp
    runtime/JSArrayBufferPrototype.cpp
    runtime/JSArrayBufferView.cpp
    runtime/JSArrayIterator.cpp
    runtime/JSBoundFunction.cpp
    runtime/JSCJSValue.cpp
    runtime/JSCell.cpp
    runtime/JSDataView.cpp
    runtime/JSDataViewPrototype.cpp
    runtime/JSDateMath.cpp
    runtime/JSFunction.cpp
    runtime/JSGlobalObject.cpp
    runtime/JSGlobalObjectFunctions.cpp
    runtime/JSLock.cpp
    runtime/JSMap.cpp
    runtime/JSMapIterator.cpp
    runtime/JSNameScope.cpp
    runtime/JSNotAnObject.cpp
    runtime/JSONObject.cpp
    runtime/JSObject.cpp
    runtime/JSPromise.cpp
    runtime/JSPromiseConstructor.cpp
    runtime/JSPromiseDeferred.cpp
    runtime/JSPromiseFunctions.cpp
    runtime/JSPromiseReaction.cpp
    runtime/JSPromisePrototype.cpp
    runtime/JSPropertyNameIterator.cpp
    runtime/JSProxy.cpp
    runtime/JSScope.cpp
    runtime/JSSegmentedVariableObject.cpp
    runtime/JSSet.cpp
    runtime/JSSetIterator.cpp
    runtime/JSString.cpp
    runtime/JSStringJoiner.cpp
    runtime/JSSymbolTableObject.cpp
    runtime/JSTypedArrayConstructors.cpp
    runtime/JSTypedArrayPrototypes.cpp
    runtime/JSTypedArrays.cpp
    runtime/JSVariableObject.cpp
    runtime/JSWeakMap.cpp
    runtime/JSWithScope.cpp
    runtime/JSWrapperObject.cpp
    runtime/LiteralParser.cpp
    runtime/Lookup.cpp
    runtime/MapConstructor.cpp
    runtime/MapData.cpp
    runtime/MapIteratorConstructor.cpp
    runtime/MapIteratorPrototype.cpp
    runtime/MapPrototype.cpp
    runtime/MathObject.cpp
    runtime/MemoryStatistics.cpp
    runtime/NameConstructor.cpp
    runtime/NameInstance.cpp
    runtime/NamePrototype.cpp
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
    runtime/PropertyTable.cpp
    runtime/PrototypeMap.cpp
    runtime/RegExp.cpp
    runtime/RegExpCache.cpp
    runtime/RegExpCachedResult.cpp
    runtime/RegExpConstructor.cpp
    runtime/RegExpMatchesArray.cpp
    runtime/RegExpObject.cpp
    runtime/RegExpPrototype.cpp
    runtime/SamplingCounter.cpp
    runtime/SetConstructor.cpp
    runtime/SetIteratorConstructor.cpp
    runtime/SetIteratorPrototype.cpp
    runtime/SetPrototype.cpp
    runtime/SimpleTypedArrayController.cpp
    runtime/SmallStrings.cpp
    runtime/SparseArrayValueMap.cpp
    runtime/StrictEvalActivation.cpp
    runtime/StringConstructor.cpp
    runtime/StringObject.cpp
    runtime/StringPrototype.cpp
    runtime/StringRecursionChecker.cpp
    runtime/Structure.cpp
    runtime/StructureChain.cpp
    runtime/StructureRareData.cpp
    runtime/SymbolTable.cpp
    runtime/TestRunnerUtils.cpp
    runtime/TypedArrayController.cpp
    runtime/TypedArrayType.cpp
    runtime/VM.cpp
    runtime/VMEntryScope.cpp
    runtime/Watchdog.cpp
    runtime/WatchdogNone.cpp
    runtime/WeakMapConstructor.cpp
    runtime/WeakMapData.cpp
    runtime/WeakMapPrototype.cpp

    tools/CodeProfile.cpp
    tools/CodeProfiling.cpp

    yarr/RegularExpression.cpp
    yarr/YarrCanonicalizeUCS2.cpp
    yarr/YarrInterpreter.cpp
    yarr/YarrJIT.cpp
    yarr/YarrPattern.cpp
    yarr/YarrSyntaxChecker.cpp
)

set(JavaScriptCore_LUT_FILES
    runtime/ArrayConstructor.cpp
    runtime/ArrayPrototype.cpp
    runtime/BooleanPrototype.cpp
    runtime/DateConstructor.cpp
    runtime/DatePrototype.cpp
    runtime/ErrorPrototype.cpp
    runtime/JSDataViewPrototype.cpp
    runtime/JSGlobalObject.cpp
    runtime/JSONObject.cpp
    runtime/JSPromiseConstructor.cpp
    runtime/JSPromisePrototype.cpp
    runtime/NamePrototype.cpp
    runtime/NumberConstructor.cpp
    runtime/NumberPrototype.cpp
    runtime/ObjectConstructor.cpp
    runtime/RegExpConstructor.cpp
    runtime/RegExpObject.cpp
    runtime/RegExpPrototype.cpp
    runtime/StringConstructor.cpp
)

set(JavaScriptCore_LIBRARIES
    WTF
)

if (WTF_USE_ICU_UNICODE)
    list(APPEND JavaScriptCore_INCLUDE_DIRECTORIES
        ${ICU_INCLUDE_DIRS}
    )
    list(APPEND JavaScriptCore_LIBRARIES
        ${ICU_I18N_LIBRARIES}
    )
endif ()

if (WTF_USE_UDIS86)
    set(UDIS_GEN_DEP
        disassembler/udis86/ud_opcode.py
        disassembler/udis86/ud_optable.py
    )

    add_custom_command(
        OUTPUT ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/udis86_itab.c ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/udis86_itab.h
        DEPENDS ${UDIS_GEN_DEP}
        WORKING_DIRECTORY ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}
        COMMAND ${PYTHON_EXECUTABLE} ${JAVASCRIPTCORE_DIR}/disassembler/udis86/itab.py ${JAVASCRIPTCORE_DIR}/disassembler/udis86/optable.xml
        VERBATIM)

    list(APPEND JavaScriptCore_INCLUDE_DIRECTORIES
        ${JAVASCRIPTCORE_DIR}/disassembler/udis86
    )
    list(APPEND JavaScriptCore_HEADERS
        ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/udis86_itab.h
    )
    list(APPEND JavaScriptCore_SOURCES
        disassembler/udis86/udis86.c
        disassembler/udis86/udis86_decode.c
        disassembler/udis86/udis86_input.c
        disassembler/udis86/udis86_itab_holder.c
        disassembler/udis86/udis86_syn-att.c
        disassembler/udis86/udis86_syn-intel.c
        disassembler/udis86/udis86_syn.c
    )
endif ()

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
        offlineasm/arm.rb
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
    target_link_libraries(LLIntOffsetsExtractor WTF)

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
        llint/LLIntEntrypoint.cpp
        llint/LLIntExceptions.cpp
        llint/LLIntSlowPaths.cpp
        llint/LLIntThunks.cpp
        llint/LowLevelInterpreter.cpp
    )
endif ()

if (ENABLE_FTL_JIT)
    if (NOT LLVM_STATIC_LIBRARIES)
        message(FATAL_ERROR "No LLVM libs found, but FTL is enabled. Stopping build.")
    endif ()

    # Configure llvmForJSC.so
    set(llvmForJSC_SOURCES
        llvm/library/LLVMAnchor.cpp
        llvm/library/LLVMExports.cpp
        llvm/library/LLVMOverrides.cpp
    )
    set(llvmForJSC_INCLUDE_DIRECTORIES
        ${LLVM_INCLUDE_DIRS}
        "${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}"
    )

    add_custom_command(
        OUTPUT ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/WebKitLLVMLibraryToken.h
        DEPENDS ${LLVM_STATIC_LIBRARIES}
        COMMAND ${CMAKE_COMMAND} -E touch ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/WebKitLLVMLibraryToken.h
        VERBATIM)

    WEBKIT_WRAP_SOURCELIST(${llvmForJSC_SOURCES})
    add_library(llvmForJSC SHARED ${llvmForJSC_SOURCES} ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/WebKitLLVMLibraryToken.h)
    target_link_libraries(llvmForJSC ${LLVM_STATIC_LIBRARIES} "pthread" "dl")

    # Added extra items for JavaScriptCore
    list(APPEND JavaScriptCore_INCLUDE_DIRECTORIES
        ${LLVM_INCLUDE_DIRS}
    )

    list(APPEND JavaScriptCore_SOURCES
        llvm/InitializeLLVM.cpp
        llvm/InitializeLLVMPOSIX.cpp
        llvm/InitializeLLVMLinux.cpp
        llvm/LLVMAPI.cpp

        dfg/DFGToFTLDeferredCompilationCallback.cpp
        dfg/DFGToFTLForOSREntryDeferredCompilationCallback.cpp

        disassembler/LLVMDisassembler.cpp
        disassembler/X86Disassembler.cpp

        ftl/FTLAbstractHeap.cpp
        ftl/FTLAbstractHeapRepository.cpp
        ftl/FTLCapabilities.cpp
        ftl/FTLCommonValues.cpp
        ftl/FTLCompile.cpp
        ftl/FTLDWARFDebugLineInfo.cpp
        ftl/FTLExitArgument.cpp
        ftl/FTLExitArgumentForOperand.cpp
        ftl/FTLExitThunkGenerator.cpp
        ftl/FTLExitValue.cpp
        ftl/FTLFail.cpp
        ftl/FTLForOSREntryJITCode.cpp
        ftl/FTLInlineCacheSize.cpp
        ftl/FTLIntrinsicRepository.cpp
        ftl/FTLJITCode.cpp
        ftl/FTLJITFinalizer.cpp
        ftl/FTLLink.cpp
        ftl/FTLLocation.cpp
        ftl/FTLLowerDFGToLLVM.cpp
        ftl/FTLOSREntry.cpp
        ftl/FTLOSRExit.cpp
        ftl/FTLOSRExitCompiler.cpp
        ftl/FTLOutput.cpp
        ftl/FTLSaveRestore.cpp
        ftl/FTLSlowPathCall.cpp
        ftl/FTLSlowPathCallKey.cpp
        ftl/FTLStackMaps.cpp
        ftl/FTLState.cpp
        ftl/FTLThunks.cpp
        ftl/FTLValueFormat.cpp
    )
endif ()

set(HASH_LUT_GENERATOR ${CMAKE_CURRENT_SOURCE_DIR}/create_hash_table)
macro(GENERATE_HASH_LUT _input _output)
    add_custom_command(
        OUTPUT ${_output}
        DEPENDS ${HASH_LUT_GENERATOR} ${_input} ${CMAKE_CURRENT_SOURCE_DIR}/generate-js-builtins
        COMMAND ${PERL_EXECUTABLE} ${HASH_LUT_GENERATOR} ${_input} -i > ${_output}
        VERBATIM)
    list(APPEND JavaScriptCore_HEADERS ${_output})
endmacro()

# GENERATOR 1-A: LUT creator
foreach (_file ${JavaScriptCore_LUT_FILES})
    get_filename_component(_name ${_file} NAME_WE)
    GENERATE_HASH_LUT(${CMAKE_CURRENT_SOURCE_DIR}/${_file} ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/${_name}.lut.h)
endforeach ()

set(JavaScriptCore_FORWARDING_HEADERS_DIRECTORIES
    assembler
    bindings
    bytecode
    debugger
    heap
    inspector
    inspector/agents
    interpreter
    jit
    llint
    parser
    profiler
    runtime
    yarr

    collector/handles
    ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}
)

set(JavaScriptCore_FORWARDING_HEADERS_FILES
    API/APICast.h
    API/APIShims.h
    API/JSBase.h
    API/JSCTestRunnerUtils.h
    API/JSContextRef.h
    API/JSContextRefPrivate.h
    API/JSObjectRef.h
    API/JSObjectRefPrivate.h
    API/JSRetainPtr.h
    API/JSScriptRefPrivate.h
    API/JSStringRef.h
    API/JSStringRefBSTR.h
    API/JSStringRefCF.h
    API/JSValueRef.h
    API/JSWeakObjectMapRefInternal.h
    API/JSWeakObjectMapRefPrivate.h
    API/JavaScript.h
    API/JavaScriptCore.h
    API/OpaqueJSString.h
    API/WebKitAvailability.h

    assembler/LinkBuffer.h
    assembler/MacroAssembler.h
    assembler/MacroAssemblerCodeRef.h
    assembler/MacroAssemblerCodeRef.h
    jit/GPRInfo.h
    runtime/VM.h

    ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/JSCBuiltins.h
)


# GENERATOR 1-B: particular LUT creator (for 1 file only)
GENERATE_HASH_LUT(${CMAKE_CURRENT_SOURCE_DIR}/parser/Keywords.table ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/Lexer.lut.h)

#GENERATOR: "RegExpJitTables.h": tables used by Yarr
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/RegExpJitTables.h
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/create_regex_tables
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/create_regex_tables > ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/RegExpJitTables.h
    VERBATIM)
ADD_SOURCE_DEPENDENCIES(${CMAKE_CURRENT_SOURCE_DIR}/yarr/YarrPattern.cpp ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/RegExpJitTables.h)


#GENERATOR: "KeywordLookup.h": keyword decision tree used by the lexer
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/KeywordLookup.h
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/KeywordLookupGenerator.py
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/KeywordLookupGenerator.py ${CMAKE_CURRENT_SOURCE_DIR}/parser/Keywords.table > ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/KeywordLookup.h
    VERBATIM)
ADD_SOURCE_DEPENDENCIES(${CMAKE_CURRENT_SOURCE_DIR}/parser/Lexer.cpp ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/KeywordLookup.h)


# Inspector Interfaces

set(JavaScriptCore_INSPECTOR_SCRIPTS_DIR "${JAVASCRIPTCORE_DIR}/inspector/scripts")

set(JavaScriptCore_INSPECTOR_DOMAINS
    inspector/protocol/Console.json
    inspector/protocol/Debugger.json
    inspector/protocol/GenericTypes.json
    inspector/protocol/InspectorDomain.json
    inspector/protocol/Runtime.json
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InspectorJS.json
    MAIN_DEPENDENCY ${JavaScriptCore_INSPECTOR_SCRIPTS_DIR}/generate-combined-inspector-json.py
    DEPENDS ${JavaScriptCore_INSPECTOR_DOMAINS}
    COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_INSPECTOR_SCRIPTS_DIR}/generate-combined-inspector-json.py ${JAVASCRIPTCORE_DIR}/inspector/protocol > ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InspectorJS.json
    VERBATIM)

# Inspector Backend Dispatchers, Frontend Dispatchers, Type Builders
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InspectorJSBackendDispatchers.cpp ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InspectorJSBackendDispatchers.h ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InspectorJSFrontendDispatchers.cpp ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InspectorJSFrontendDispatchers.h ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InspectorJSTypeBuilders.cpp ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InspectorJSTypeBuilders.h
    MAIN_DEPENDENCY ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InspectorJS.json
    DEPENDS ${JavaScriptCore_INSPECTOR_SCRIPTS_DIR}/CodeGeneratorInspector.py ${JavaScriptCore_INSPECTOR_SCRIPTS_DIR}/CodeGeneratorInspectorStrings.py
    COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_INSPECTOR_SCRIPTS_DIR}/CodeGeneratorInspector.py ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InspectorJS.json --output_h_dir "${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}" --output_cpp_dir "${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}" --output_js_dir "${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}" --output_type JavaScript --write_always && mkdir -p ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/inspector && cp ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InspectorJSBackendDispatchers.h ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InspectorJSFrontendDispatchers.h ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InspectorJSTypeBuilders.h ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/inspector
    VERBATIM)

# JSCBuiltins
file(GLOB JSCBuiltins_js_files "${CMAKE_CURRENT_SOURCE_DIR}/builtins/*.js")
add_custom_command(
   OUTPUT ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/JSCBuiltins.cpp ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/JSCBuiltins.h
   MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/generate-js-builtins
   DEPENDS ${JSCBuiltins_js_files}
   COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/generate-js-builtins ${JSCBuiltins_js_files} ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/JSCBuiltins.h ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/JSCBuiltins.cpp
   VERBATIM)

list(APPEND JavaScriptCore_SOURCES
    ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InspectorJSBackendDispatchers.cpp
    ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InspectorJSFrontendDispatchers.cpp
    ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InspectorJSTypeBuilders.cpp
    ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/JSCBuiltins.cpp
)

list(APPEND JavaScriptCore_HEADERS
    ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InspectorJSBackendDispatchers.h
    ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InspectorJSFrontendDispatchers.h
    ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InspectorJSTypeBuilders.h
    ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/JSCBuiltins.h
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InjectedScriptSource.h ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InjectedScriptSource.min.js
    MAIN_DEPENDENCY ${JAVASCRIPTCORE_DIR}/inspector/InjectedScriptSource.js
    DEPENDS ${JavaScriptCore_INSPECTOR_SCRIPTS_DIR}/xxd.pl ${JavaScriptCore_INSPECTOR_SCRIPTS_DIR}/jsmin.py
    COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_INSPECTOR_SCRIPTS_DIR}/jsmin.py < ${JAVASCRIPTCORE_DIR}/inspector/InjectedScriptSource.js > ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InjectedScriptSource.min.js
    COMMAND ${PERL_EXECUTABLE} ${JavaScriptCore_INSPECTOR_SCRIPTS_DIR}/xxd.pl InjectedScriptSource_js ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InjectedScriptSource.min.js ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InjectedScriptSource.h
    VERBATIM)

list(APPEND JavaScriptCore_HEADERS ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InjectedScriptSource.h)


if (WTF_CPU_ARM)
    list(APPEND JavaScriptCore_SOURCES
        assembler/ARMAssembler.cpp
        assembler/ARMv7Assembler.cpp
        assembler/MacroAssemblerARM.cpp
    )
    if (MSVC AND ENABLE_JIT)
        add_custom_command(
            OUTPUT ${DERIVED_SOURCES_DIR}/GeneratedJITStubs.asm
            MAIN_DEPENDENCY ${JAVASCRIPTCORE_DIR}/create_jit_stubs
            DEPENDS ${JAVASCRIPTCORE_DIR}/jit/JITStubsARM.h
            DEPENDS ${JAVASCRIPTCORE_DIR}/jit/JITStubs.cpp
            COMMAND ${PERL_EXECUTABLE} ${JAVASCRIPTCORE_DIR}/create_jit_stubs --prefix=MSVC --header ${JAVASCRIPTCORE_DIR}/jit/JITStubsARM.h ${JAVASCRIPTCORE_DIR}/jit/JITStubs.cpp > ${DERIVED_SOURCES_DIR}/GeneratedJITStubs.asm
            VERBATIM)

        add_custom_command(
            OUTPUT ${DERIVED_SOURCES_DIR}/GeneratedJITStubs.obj
            MAIN_DEPENDENCY ${DERIVED_SOURCES_DIR}/GeneratedJITStubs.asm
            COMMAND armasm -nologo ${DERIVED_SOURCES_DIR}/GeneratedJITStubs.asm ${DERIVED_SOURCES_DIR}/GeneratedJITStubs.obj
            VERBATIM)

        list(APPEND JavaScriptCore_SOURCES ${DERIVED_SOURCES_DIR}/GeneratedJITStubs.obj)
    endif ()
elseif (WTF_CPU_MIPS)
elseif (WTF_CPU_SH4)
elseif (WTF_CPU_X86)
    list(APPEND JavaScriptCore_SOURCES
        assembler/MacroAssemblerX86Common.cpp
    )
elseif (WTF_CPU_X86_64)
    if (MSVC AND ENABLE_JIT)
        add_custom_command(
            OUTPUT ${DERIVED_SOURCES_DIR}/JITStubsMSVC64.obj
            MAIN_DEPENDENCY ${JAVASCRIPTCORE_DIR}/jit/JITStubsMSVC64.asm
            COMMAND ml64 -nologo -c -Fo ${DERIVED_SOURCES_DIR}/JITStubsMSVC64.obj ${JAVASCRIPTCORE_DIR}/jit/JITStubsMSVC64.asm
            VERBATIM)

        list(APPEND JavaScriptCore_SOURCES ${DERIVED_SOURCES_DIR}/JITStubsMSVC64.obj)
    endif ()
    list(APPEND JavaScriptCore_SOURCES
        assembler/MacroAssemblerX86Common.cpp
    )
else ()
    message(FATAL_ERROR "Unknown CPU")
endif ()


WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

WEBKIT_CREATE_FORWARDING_HEADERS(JavaScriptCore DIRECTORIES ${JavaScriptCore_FORWARDING_HEADERS_DIRECTORIES} FILES ${JavaScriptCore_FORWARDING_HEADERS_FILES})


add_subdirectory(shell)

WEBKIT_WRAP_SOURCELIST(${JavaScriptCore_SOURCES})
include_directories(${JavaScriptCore_INCLUDE_DIRECTORIES})
add_definitions(-DSTATICALLY_LINKED_WITH_WTF)
add_library(JavaScriptCore ${JavaScriptCore_LIBRARY_TYPE} ${JavaScriptCore_HEADERS} ${JavaScriptCore_SOURCES})
target_link_libraries(JavaScriptCore ${JavaScriptCore_LIBRARIES})
set_target_properties(JavaScriptCore PROPERTIES COMPILE_DEFINITIONS "BUILDING_JavaScriptCore")
set_target_properties(JavaScriptCore PROPERTIES FOLDER "JavaScriptCore")
set_target_properties(JavaScriptCore PROPERTIES LINK_INTERFACE_LIBRARIES "")

if (JavaScriptCore_OUTPUT_NAME)
    set_target_properties(JavaScriptCore PROPERTIES OUTPUT_NAME ${JavaScriptCore_OUTPUT_NAME})
endif ()

if (${JavaScriptCore_LIBRARY_TYPE} STREQUAL "SHARED")
    POPULATE_LIBRARY_VERSION(JAVASCRIPTCORE)
    set_target_properties(JavaScriptCore PROPERTIES VERSION ${JAVASCRIPTCORE_VERSION} SOVERSION ${JAVASCRIPTCORE_VERSION_MAJOR})
    install(TARGETS JavaScriptCore DESTINATION "${LIB_INSTALL_DIR}")
endif ()

if (ENABLE_FTL_JIT)
    add_dependencies(JavaScriptCore llvmForJSC)
endif ()
