# -------------------------------------------------------------------
# Target file for the JavaScriptSource library
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

TEMPLATE = lib
TARGET = JavaScriptCore

include(JavaScriptCore.pri)

WEBKIT += wtf
QT += core
QT -= gui

CONFIG += staticlib

*-g++*:QMAKE_CXXFLAGS_RELEASE -= -O2
*-g++*:QMAKE_CXXFLAGS_RELEASE += -O3

# Rules when JIT enabled (not disabled)
!contains(DEFINES, ENABLE_JIT=0) {
    linux*-g++*:greaterThan(QT_GCC_MAJOR_VERSION,3):greaterThan(QT_GCC_MINOR_VERSION,0) {
        QMAKE_CXXFLAGS += -fno-stack-protector
        QMAKE_CFLAGS += -fno-stack-protector
    }
}

include(yarr/yarr.pri)

INSTALLDEPS += all

debug_and_release: INCLUDEPATH += $$JAVASCRIPTCORE_GENERATED_SOURCES_DIR/$$activeBuildConfig()

SOURCES += \
    API/JSBase.cpp \
    API/JSCallbackConstructor.cpp \
    API/JSCallbackFunction.cpp \
    API/JSCallbackObject.cpp \
    API/JSClassRef.cpp \
    API/JSContextRef.cpp \
    API/JSObjectRef.cpp \
    API/JSStringRef.cpp \
    API/JSValueRef.cpp \
    API/JSWeakObjectMapRefPrivate.cpp \
    API/OpaqueJSString.cpp \
    assembler/ARMAssembler.cpp \
    assembler/ARMv7Assembler.cpp \
    assembler/LinkBuffer.cpp \
    assembler/MacroAssembler.cpp \
    assembler/MacroAssemblerARM.cpp \
    assembler/MacroAssemblerSH4.cpp \
    bytecode/ArrayAllocationProfile.cpp \
    bytecode/ArrayProfile.cpp \
    bytecode/CallLinkInfo.cpp \
    bytecode/CallLinkStatus.cpp \
    bytecode/CodeBlock.cpp \
    bytecode/CodeBlockHash.cpp \
    bytecode/CodeOrigin.cpp \
    bytecode/CodeType.cpp \
    bytecode/DFGExitProfile.cpp \
    bytecode/ExecutionCounter.cpp \
    bytecode/GetByIdStatus.cpp \
    bytecode/JumpTable.cpp \
    bytecode/LazyOperandValueProfile.cpp \
    bytecode/MethodOfGettingAValueProfile.cpp \
    bytecode/Opcode.cpp \
    bytecode/PolymorphicPutByIdList.cpp \
    bytecode/PutByIdStatus.cpp \
    bytecode/ReduceWhitespace.cpp \
    bytecode/ResolveGlobalStatus.cpp \
    bytecode/SamplingTool.cpp \
    bytecode/SpecialPointer.cpp \
    bytecode/SpeculatedType.cpp \
    bytecode/StructureStubClearingWatchpoint.cpp \
    bytecode/StructureStubInfo.cpp \
    bytecode/UnlinkedCodeBlock.cpp \
    bytecode/Watchpoint.cpp \
    bytecompiler/BytecodeGenerator.cpp \
    bytecompiler/NodesCodegen.cpp \
    heap/CopiedSpace.cpp \
    heap/CopyVisitor.cpp \
    heap/ConservativeRoots.cpp \
    heap/DFGCodeBlocks.cpp \
    heap/WeakSet.cpp \
    heap/WeakHandleOwner.cpp \
    heap/WeakBlock.cpp \
    heap/HandleSet.cpp \
    heap/HandleStack.cpp \
    heap/BlockAllocator.cpp \
    heap/GCThreadSharedData.cpp \
    heap/GCThread.cpp \
    heap/Heap.cpp \
    heap/HeapStatistics.cpp \
    heap/HeapTimer.cpp \
    heap/IncrementalSweeper.cpp \
    heap/JITStubRoutineSet.cpp \
    heap/MachineStackMarker.cpp \
    heap/MarkStack.cpp \
    heap/MarkedAllocator.cpp \
    heap/MarkedBlock.cpp \
    heap/MarkedSpace.cpp \
    heap/SlotVisitor.cpp \
    heap/VTableSpectrum.cpp \
    heap/WriteBarrierSupport.cpp \
    debugger/DebuggerActivation.cpp \
    debugger/DebuggerCallFrame.cpp \
    debugger/Debugger.cpp \
    dfg/DFGAbstractState.cpp \
    dfg/DFGArgumentsSimplificationPhase.cpp \
    dfg/DFGArrayMode.cpp \
    dfg/DFGAssemblyHelpers.cpp \
    dfg/DFGByteCodeParser.cpp \
    dfg/DFGCapabilities.cpp \
    dfg/DFGCFAPhase.cpp \
    dfg/DFGCFGSimplificationPhase.cpp \
    dfg/DFGConstantFoldingPhase.cpp \
    dfg/DFGCorrectableJumpPoint.cpp \
    dfg/DFGCSEPhase.cpp \
    dfg/DFGDisassembler.cpp \
    dfg/DFGDominators.cpp \
    dfg/DFGDriver.cpp \
    dfg/DFGFixupPhase.cpp \
    dfg/DFGGraph.cpp \
    dfg/DFGJITCompiler.cpp \
    dfg/DFGMinifiedNode.cpp \
    dfg/DFGNodeFlags.cpp \
    dfg/DFGOperations.cpp \
    dfg/DFGOSREntry.cpp \
    dfg/DFGOSRExit.cpp \
    dfg/DFGOSRExitCompiler.cpp \
    dfg/DFGOSRExitCompiler64.cpp \
    dfg/DFGOSRExitCompiler32_64.cpp \
    dfg/DFGPhase.cpp \
    dfg/DFGPredictionPropagationPhase.cpp \
    dfg/DFGRepatch.cpp \
    dfg/DFGSpeculativeJIT.cpp \
    dfg/DFGSpeculativeJIT32_64.cpp \
    dfg/DFGSpeculativeJIT64.cpp \
    dfg/DFGStructureCheckHoistingPhase.cpp \
    dfg/DFGThunks.cpp \
    dfg/DFGValueSource.cpp \
    dfg/DFGVariableAccessDataDump.cpp \
    dfg/DFGVariableEvent.cpp \
    dfg/DFGVariableEventStream.cpp \
    dfg/DFGValidate.cpp \
    dfg/DFGVirtualRegisterAllocationPhase.cpp \
    disassembler/Disassembler.cpp \
    interpreter/AbstractPC.cpp \
    interpreter/CallFrame.cpp \
    interpreter/Interpreter.cpp \
    interpreter/JSStack.cpp \
    jit/ClosureCallStubRoutine.cpp \
    jit/ExecutableAllocatorFixedVMPool.cpp \
    jit/ExecutableAllocator.cpp \
    jit/HostCallReturnValue.cpp \
    jit/GCAwareJITStubRoutine.cpp \
    jit/JITArithmetic.cpp \
    jit/JITArithmetic32_64.cpp \
    jit/JITCall.cpp \
    jit/JITCall32_64.cpp \
    jit/JITCode.cpp \
    jit/JIT.cpp \
    jit/JITDisassembler.cpp \
    jit/JITExceptions.cpp \
    jit/JITOpcodes.cpp \
    jit/JITOpcodes32_64.cpp \
    jit/JITPropertyAccess.cpp \
    jit/JITPropertyAccess32_64.cpp \
    jit/JITStubRoutine.cpp \
    jit/JITStubs.cpp \
    jit/JumpReplacementWatchpoint.cpp \
    jit/ThunkGenerators.cpp \
    llint/LLIntCLoop.cpp \
    llint/LLIntData.cpp \
    llint/LLIntEntrypoints.cpp \
    llint/LLIntExceptions.cpp \
    llint/LLIntSlowPaths.cpp \
    llint/LLIntThunks.cpp \
    llint/LowLevelInterpreter.cpp \
    parser/Lexer.cpp \
    parser/Nodes.cpp \
    parser/ParserArena.cpp \
    parser/Parser.cpp \
    parser/SourceProviderCache.cpp \
    profiler/ProfilerBytecode.cpp \
    profiler/ProfilerBytecode.h \
    profiler/ProfilerBytecodes.cpp \
    profiler/ProfilerBytecodes.h \
    profiler/ProfilerCompilation.cpp \
    profiler/ProfilerCompilation.h \
    profiler/ProfilerCompilationKind.cpp \
    profiler/ProfilerCompilationKind.h \
    profiler/ProfilerCompiledBytecode.cpp \
    profiler/ProfilerCompiledBytecode.h \
    profiler/ProfilerDatabase.cpp \
    profiler/ProfilerDatabase.h \
    profiler/ProfilerExecutionCounter.h \
    profiler/ProfilerOrigin.cpp \
    profiler/ProfilerOrigin.h \
    profiler/ProfilerOriginStack.cpp \
    profiler/ProfilerOriginStack.h \
    profiler/Profile.cpp \
    profiler/ProfileGenerator.cpp \
    profiler/ProfileNode.cpp \
    profiler/LegacyProfiler.cpp \
    runtime/ArgList.cpp \
    runtime/Arguments.cpp \
    runtime/ArrayConstructor.cpp \
    runtime/ArrayPrototype.cpp \
    runtime/BooleanConstructor.cpp \
    runtime/BooleanObject.cpp \
    runtime/BooleanPrototype.cpp \
    runtime/CallData.cpp \
    runtime/CodeCache.cpp \
    runtime/CodeSpecializationKind.cpp \
    runtime/CommonIdentifiers.cpp \
    runtime/Completion.cpp \
    runtime/ConstructData.cpp \
    runtime/DateConstructor.cpp \
    runtime/DateConversion.cpp \
    runtime/DateInstance.cpp \
    runtime/DatePrototype.cpp \
    runtime/ErrorConstructor.cpp \
    runtime/Error.cpp \
    runtime/ErrorInstance.cpp \
    runtime/ErrorPrototype.cpp \
    runtime/ExceptionHelpers.cpp \
    runtime/Executable.cpp \
    runtime/FunctionConstructor.cpp \
    runtime/FunctionPrototype.cpp \
    runtime/GCActivityCallback.cpp \
    runtime/GetterSetter.cpp \
    runtime/Options.cpp \
    runtime/Identifier.cpp \
    runtime/IndexingType.cpp \
    runtime/InitializeThreading.cpp \
    runtime/InternalFunction.cpp \
    runtime/JSActivation.cpp \
    runtime/JSAPIValueWrapper.cpp \
    runtime/JSArray.cpp \
    runtime/JSCell.cpp \
    runtime/JSDateMath.cpp \
    runtime/JSFunction.cpp \
    runtime/JSBoundFunction.cpp \
    runtime/JSGlobalData.cpp \
    runtime/JSGlobalObject.cpp \
    runtime/JSGlobalObjectFunctions.cpp \
    runtime/JSProxy.cpp \
    runtime/JSLock.cpp \
    runtime/JSNotAnObject.cpp \
    runtime/JSObject.cpp \
    runtime/JSONObject.cpp \
    runtime/JSPropertyNameIterator.cpp \
    runtime/JSSegmentedVariableObject.cpp \
    runtime/JSWithScope.cpp \
    runtime/JSNameScope.cpp \
    runtime/JSScope.cpp \
    runtime/JSString.cpp \
    runtime/JSStringJoiner.cpp \
    runtime/JSSymbolTableObject.cpp \
    runtime/JSValue.cpp \
    runtime/JSVariableObject.cpp \
    runtime/JSWrapperObject.cpp \
    runtime/LiteralParser.cpp \
    runtime/Lookup.cpp \
    runtime/MathObject.cpp \
    runtime/MemoryStatistics.cpp \
    runtime/NameConstructor.cpp \
    runtime/NameInstance.cpp \
    runtime/NamePrototype.cpp \
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
    runtime/RegExpConstructor.cpp \
    runtime/RegExpCachedResult.cpp \
    runtime/RegExpMatchesArray.cpp \
    runtime/RegExp.cpp \
    runtime/RegExpObject.cpp \
    runtime/RegExpPrototype.cpp \
    runtime/RegExpCache.cpp \
    runtime/SamplingCounter.cpp \
    runtime/SmallStrings.cpp \
    runtime/SparseArrayValueMap.cpp \
    runtime/StrictEvalActivation.cpp \
    runtime/StringConstructor.cpp \
    runtime/StringObject.cpp \
    runtime/StringPrototype.cpp \
    runtime/StringRecursionChecker.cpp \
    runtime/StructureChain.cpp \
    runtime/Structure.cpp \
    runtime/SymbolTable.cpp \
    runtime/TimeoutChecker.cpp \
    tools/CodeProfile.cpp \
    tools/CodeProfiling.cpp \
    yarr/YarrJIT.cpp \

linux-*:if(isEqual(QT_ARCH, "i386")|isEqual(QT_ARCH, "x86_64")) {
    SOURCES += \
        disassembler/UDis86Disassembler.cpp \
        disassembler/udis86/udis86.c \
        disassembler/udis86/udis86_decode.c \
        disassembler/udis86/udis86_input.c \
        disassembler/udis86/udis86_itab_holder.c \
        disassembler/udis86/udis86_syn-att.c \
        disassembler/udis86/udis86_syn-intel.c \
        disassembler/udis86/udis86_syn.c \
}

HEADERS += $$files(*.h, true)

*sh4* {
    QMAKE_CXXFLAGS += -mieee -w
    QMAKE_CFLAGS   += -mieee -w
}

lessThan(QT_GCC_MAJOR_VERSION, 5) {
    # GCC 4.5 and before
    lessThan(QT_GCC_MINOR_VERSION, 6) {
        # Disable C++0x mode in JSC for those who enabled it in their Qt's mkspec.
        *-g++*:QMAKE_CXXFLAGS -= -std=c++0x -std=gnu++0x
    }
}
