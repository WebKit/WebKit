/*
 * Copyright (C) 2008-2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "VM.h"

#include "AggregateError.h"
#include "ArgList.h"
#include "BigIntObject.h"
#include "BooleanObject.h"
#include "BuiltinExecutables.h"
#include "BytecodeIntrinsicRegistry.h"
#include "CheckpointOSRExitSideState.h"
#include "ClonedArguments.h"
#include "CodeBlock.h"
#include "CodeCache.h"
#include "CommonIdentifiers.h"
#include "CustomGetterSetter.h"
#include "DFGWorklist.h"
#include "DOMAttributeGetterSetter.h"
#include "DateInstance.h"
#include "DebuggerScope.h"
#include "DeferredWorkTimer.h"
#include "Disassembler.h"
#include "DoublePredictionFuzzerAgent.h"
#include "ErrorInstance.h"
#include "EvalCodeBlock.h"
#include "Exception.h"
#include "ExecutableToCodeBlockEdge.h"
#include "FTLThunks.h"
#include "FastMallocAlignedMemoryAllocator.h"
#include "FileBasedFuzzerAgent.h"
#include "FunctionCodeBlock.h"
#include "FunctionExecutable.h"
#include "GetterSetter.h"
#include "GigacageAlignedMemoryAllocator.h"
#include "HasOwnPropertyCache.h"
#include "Heap.h"
#include "HeapProfiler.h"
#include "HostCallReturnValue.h"
#include "Interpreter.h"
#include "IntlCollator.h"
#include "IntlDateTimeFormat.h"
#include "IntlLocale.h"
#include "IntlNumberFormat.h"
#include "IntlPluralRules.h"
#include "IntlRelativeTimeFormat.h"
#include "IsoHeapCellType.h"
#include "IsoInlinedHeapCellType.h"
#include "JITCode.h"
#include "JITWorklist.h"
#include "JSAPIGlobalObject.h"
#include "JSAPIValueWrapper.h"
#include "JSAPIWrapperObject.h"
#include "JSArray.h"
#include "JSArrayBuffer.h"
#include "JSArrayIterator.h"
#include "JSAsyncGenerator.h"
#include "JSBigInt.h"
#include "JSBoundFunction.h"
#include "JSCallbackConstructor.h"
#include "JSCallbackFunction.h"
#include "JSCallbackObject.h"
#include "JSCallee.h"
#include "JSCustomGetterSetterFunction.h"
#include "JSDestructibleObjectHeapCellType.h"
#include "JSFinalizationRegistry.h"
#include "JSFunction.h"
#include "JSGlobalLexicalEnvironment.h"
#include "JSGlobalObject.h"
#include "JSImmutableButterfly.h"
#include "JSInjectedScriptHost.h"
#include "JSJavaScriptCallFrame.h"
#include "JSLock.h"
#include "JSMap.h"
#include "JSMapIterator.h"
#include "JSModuleNamespaceObject.h"
#include "JSModuleRecord.h"
#include "JSNativeStdFunction.h"
#include "JSPromise.h"
#include "JSPropertyNameEnumerator.h"
#include "JSProxy.h"
#include "JSScriptFetchParameters.h"
#include "JSScriptFetcher.h"
#include "JSSet.h"
#include "JSSetIterator.h"
#include "JSSourceCode.h"
#include "JSStringIterator.h"
#include "JSTemplateObjectDescriptor.h"
#include "JSToWasmICCallee.h"
#include "JSTypedArrays.h"
#include "JSWeakMap.h"
#include "JSWeakObjectRef.h"
#include "JSWeakSet.h"
#include "JSWebAssemblyCodeBlock.h"
#include "JSWebAssemblyGlobal.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyMemory.h"
#include "JSWebAssemblyModule.h"
#include "JSWebAssemblyTable.h"
#include "JSWithScope.h"
#include "LLIntData.h"
#include "MinimumReservedZoneSize.h"
#include "ModuleProgramCodeBlock.h"
#include "ModuleProgramExecutable.h"
#include "NarrowingNumberPredictionFuzzerAgent.h"
#include "NativeExecutable.h"
#include "NumberObject.h"
#include "PredictionFileCreatingFuzzerAgent.h"
#include "ProfilerDatabase.h"
#include "ProgramCodeBlock.h"
#include "ProgramExecutable.h"
#include "PropertyMapHashTable.h"
#include "ProxyRevoke.h"
#include "RandomizingFuzzerAgent.h"
#include "RegExpCache.h"
#include "RegExpObject.h"
#include "SamplingProfiler.h"
#include "ScopedArguments.h"
#include "ShadowChicken.h"
#include "SimpleTypedArrayController.h"
#include "SourceProviderCache.h"
#include "StrictEvalActivation.h"
#include "StringObject.h"
#include "StrongInlines.h"
#include "StructureChain.h"
#include "StructureInlines.h"
#include "SymbolObject.h"
#include "TestRunnerUtils.h"
#include "ThunkGenerators.h"
#include "TypeProfiler.h"
#include "TypeProfilerLog.h"
#include "VMEntryScope.h"
#include "VMInlines.h"
#include "VMInspector.h"
#include "VariableEnvironment.h"
#include "WasmWorklist.h"
#include "Watchdog.h"
#include "WeakGCMapInlines.h"
#include "WebAssemblyFunction.h"
#include "WebAssemblyModuleRecord.h"
#include "WebAssemblyWrapperFunction.h"
#include "WideningNumberPredictionFuzzerAgent.h"
#include <wtf/ProcessID.h>
#include <wtf/ReadWriteLock.h>
#include <wtf/SimpleStats.h>
#include <wtf/StringPrintStream.h>
#include <wtf/Threading.h>
#include <wtf/text/AtomStringTable.h>

#if ENABLE(C_LOOP)
#include "CLoopStack.h"
#include "CLoopStackInlines.h"
#endif

#if ENABLE(DFG_JIT)
#include "ConservativeRoots.h"
#endif

#if ENABLE(REGEXP_TRACING)
#include "RegExp.h"
#endif

#if JSC_OBJC_API_ENABLED
#include "ObjCCallbackFunction.h"
#endif

#ifdef JSC_GLIB_API_ENABLED
#include "JSAPIWrapperGlobalObject.h"
#include "JSCCallbackFunction.h"
#endif

namespace JSC {

Atomic<unsigned> VM::s_numberOfIDs;

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(VM);

// Note: Platform.h will enforce that ENABLE(ASSEMBLER) is true if either
// ENABLE(JIT) or ENABLE(YARR_JIT) or both are enabled. The code below
// just checks for ENABLE(JIT) or ENABLE(YARR_JIT) with this premise in mind.

#if ENABLE(ASSEMBLER)
static bool enableAssembler()
{
    if (!Options::useJIT())
        return false;

    char* canUseJITString = getenv("JavaScriptCoreUseJIT");
    if (canUseJITString && !atoi(canUseJITString))
        return false;

    ExecutableAllocator::initializeUnderlyingAllocator();
    if (!ExecutableAllocator::singleton().isValid()) {
        if (Options::crashIfCantAllocateJITMemory())
            CRASH();
        return false;
    }

    return true;
}
#endif // ENABLE(!ASSEMBLER)

bool VM::canUseAssembler()
{
#if ENABLE(ASSEMBLER)
    static std::once_flag onceKey;
    static bool enabled = false;
    std::call_once(onceKey, [] {
        enabled = enableAssembler();
    });
    return enabled;
#else
    return false; // interpreter only
#endif
}

void VM::computeCanUseJIT()
{
#if ENABLE(JIT)
#if ASSERT_ENABLED
    RELEASE_ASSERT(!g_jscConfig.vm.canUseJITIsSet);
    g_jscConfig.vm.canUseJITIsSet = true;
#endif
    g_jscConfig.vm.canUseJIT = VM::canUseAssembler() && Options::useJIT();
#endif
}

inline unsigned VM::nextID()
{
    for (;;) {
        unsigned currentNumberOfIDs = s_numberOfIDs.load();
        unsigned newID = currentNumberOfIDs + 1;
        if (s_numberOfIDs.compareExchangeWeak(currentNumberOfIDs, newID))
            return newID;
    }
}

static bool vmCreationShouldCrash = false;

VM::VM(VMType vmType, HeapType heapType, bool* success)
    : m_id(nextID())
    , m_apiLock(adoptRef(new JSLock(this)))
#if USE(CF)
    , m_runLoop(CFRunLoopGetCurrent())
#endif // USE(CF)
    , m_random(Options::seedOfVMRandomForFuzzer() ? Options::seedOfVMRandomForFuzzer() : cryptographicallyRandomNumber())
    , m_integrityRandom(*this)
    , heap(*this, heapType)
    , fastMallocAllocator(makeUnique<FastMallocAlignedMemoryAllocator>())
    , primitiveGigacageAllocator(makeUnique<GigacageAlignedMemoryAllocator>(Gigacage::Primitive))
    , jsValueGigacageAllocator(makeUnique<GigacageAlignedMemoryAllocator>(Gigacage::JSValue))
    , auxiliaryHeapCellType(makeUnique<HeapCellType>(CellAttributes(DoesNotNeedDestruction, HeapCell::Auxiliary)))
    , immutableButterflyHeapCellType(makeUnique<HeapCellType>(CellAttributes(DoesNotNeedDestruction, HeapCell::JSCellWithIndexingHeader)))
    , cellHeapCellType(makeUnique<HeapCellType>(CellAttributes(DoesNotNeedDestruction, HeapCell::JSCell)))
    , destructibleCellHeapCellType(makeUnique<HeapCellType>(CellAttributes(NeedsDestruction, HeapCell::JSCell)))
    , apiGlobalObjectHeapCellType(IsoHeapCellType::create<JSAPIGlobalObject>())
    , callbackConstructorHeapCellType(IsoHeapCellType::create<JSCallbackConstructor>())
    , callbackGlobalObjectHeapCellType(IsoHeapCellType::create<JSCallbackObject<JSGlobalObject>>())
    , callbackObjectHeapCellType(IsoHeapCellType::create<JSCallbackObject<JSNonFinalObject>>())
    , dateInstanceHeapCellType(IsoHeapCellType::create<DateInstance>())
    , errorInstanceHeapCellType(IsoHeapCellType::create<ErrorInstance>())
    , finalizationRegistryCellType(IsoHeapCellType::create<JSFinalizationRegistry>())
    , globalLexicalEnvironmentHeapCellType(IsoHeapCellType::create<JSGlobalLexicalEnvironment>())
    , globalObjectHeapCellType(IsoHeapCellType::create<JSGlobalObject>())
    , injectedScriptHostSpaceHeapCellType(IsoHeapCellType::create<Inspector::JSInjectedScriptHost>())
    , javaScriptCallFrameHeapCellType(IsoHeapCellType::create<Inspector::JSJavaScriptCallFrame>())
    , jsModuleRecordHeapCellType(IsoHeapCellType::create<JSModuleRecord>())
    , moduleNamespaceObjectHeapCellType(IsoHeapCellType::create<JSModuleNamespaceObject>())
    , nativeStdFunctionHeapCellType(IsoHeapCellType::create<JSNativeStdFunction>())
    , stringHeapCellType(makeUnique<IsoInlinedHeapCellType<JSString>>())
    , weakMapHeapCellType(IsoHeapCellType::create<JSWeakMap>())
    , weakSetHeapCellType(IsoHeapCellType::create<JSWeakSet>())
    , destructibleObjectHeapCellType(makeUnique<JSDestructibleObjectHeapCellType>())
#if JSC_OBJC_API_ENABLED
    , apiWrapperObjectHeapCellType(IsoHeapCellType::create<JSCallbackObject<JSAPIWrapperObject>>())
    , objCCallbackFunctionHeapCellType(IsoHeapCellType::create<ObjCCallbackFunction>())
#endif
#ifdef JSC_GLIB_API_ENABLED
    , apiWrapperObjectHeapCellType(IsoHeapCellType::create<JSCallbackObject<JSAPIWrapperObject>>())
    , callbackAPIWrapperGlobalObjectHeapCellType(IsoHeapCellType::create<JSCallbackObject<JSAPIWrapperGlobalObject>>())
    , jscCallbackFunctionHeapCellType(IsoHeapCellType::create<JSCCallbackFunction>())
#endif
    , intlCollatorHeapCellType(IsoHeapCellType::create<IntlCollator>())
    , intlDateTimeFormatHeapCellType(IsoHeapCellType::create<IntlDateTimeFormat>())
    , intlLocaleHeapCellType(IsoHeapCellType::create<IntlLocale>())
    , intlNumberFormatHeapCellType(IsoHeapCellType::create<IntlNumberFormat>())
    , intlPluralRulesHeapCellType(IsoHeapCellType::create<IntlPluralRules>())
    , intlRelativeTimeFormatHeapCellType(IsoHeapCellType::create<IntlRelativeTimeFormat>())
#if ENABLE(WEBASSEMBLY)
    , webAssemblyCodeBlockHeapCellType(IsoHeapCellType::create<JSWebAssemblyCodeBlock>())
    , webAssemblyFunctionHeapCellType(IsoHeapCellType::create<WebAssemblyFunction>())
    , webAssemblyGlobalHeapCellType(IsoHeapCellType::create<JSWebAssemblyGlobal>())
    , webAssemblyInstanceHeapCellType(IsoHeapCellType::create<JSWebAssemblyInstance>())
    , webAssemblyMemoryHeapCellType(IsoHeapCellType::create<JSWebAssemblyMemory>())
    , webAssemblyModuleHeapCellType(IsoHeapCellType::create<JSWebAssemblyModule>())
    , webAssemblyModuleRecordHeapCellType(IsoHeapCellType::create<WebAssemblyModuleRecord>())
    , webAssemblyTableHeapCellType(IsoHeapCellType::create<JSWebAssemblyTable>())
#endif
    , primitiveGigacageAuxiliarySpace("Primitive Gigacage Auxiliary", heap, auxiliaryHeapCellType.get(), primitiveGigacageAllocator.get()) // Hash:0x3e7cd762
    , jsValueGigacageAuxiliarySpace("JSValue Gigacage Auxiliary", heap, auxiliaryHeapCellType.get(), jsValueGigacageAllocator.get()) // Hash:0x241e946
    , immutableButterflyJSValueGigacageAuxiliarySpace("ImmutableButterfly Gigacage JSCellWithIndexingHeader", heap, immutableButterflyHeapCellType.get(), jsValueGigacageAllocator.get()) // Hash:0x7a945300
    , cellSpace("JSCell", heap, cellHeapCellType.get(), fastMallocAllocator.get()) // Hash:0xadfb5a79
    , variableSizedCellSpace("Variable Sized JSCell", heap, cellHeapCellType.get(), fastMallocAllocator.get()) // Hash:0xbcd769cc
    , destructibleObjectSpace("JSDestructibleObject", heap, destructibleObjectHeapCellType.get(), fastMallocAllocator.get()) // Hash:0x4f5ed7a9
    , arraySpace ISO_SUBSPACE_INIT(heap, cellHeapCellType.get(), JSArray)
    , bigIntSpace ISO_SUBSPACE_INIT(heap, destructibleCellHeapCellType.get(), JSBigInt)
    , calleeSpace ISO_SUBSPACE_INIT(heap, cellHeapCellType.get(), JSCallee)
    , clonedArgumentsSpace ISO_SUBSPACE_INIT(heap, cellHeapCellType.get(), ClonedArguments)
    , customGetterSetterSpace ISO_SUBSPACE_INIT(heap, cellHeapCellType.get(), CustomGetterSetter)
    , dateInstanceSpace ISO_SUBSPACE_INIT(heap, dateInstanceHeapCellType.get(), DateInstance)
    , domAttributeGetterSetterSpace ISO_SUBSPACE_INIT(heap, cellHeapCellType.get(), DOMAttributeGetterSetter)
    , exceptionSpace ISO_SUBSPACE_INIT(heap, destructibleCellHeapCellType.get(), Exception)
    , executableToCodeBlockEdgeSpace ISO_SUBSPACE_INIT(heap, cellHeapCellType.get(), ExecutableToCodeBlockEdge) // Hash:0x7b730b20
    , functionSpace ISO_SUBSPACE_INIT(heap, cellHeapCellType.get(), JSFunction) // Hash:0x800fca72
    , getterSetterSpace ISO_SUBSPACE_INIT(heap, cellHeapCellType.get(), GetterSetter)
    , globalLexicalEnvironmentSpace ISO_SUBSPACE_INIT(heap, globalLexicalEnvironmentHeapCellType.get(), JSGlobalLexicalEnvironment)
    , internalFunctionSpace ISO_SUBSPACE_INIT(heap, cellHeapCellType.get(), InternalFunction) // Hash:0xf845c464
    , jsProxySpace ISO_SUBSPACE_INIT(heap, cellHeapCellType.get(), JSProxy)
    , nativeExecutableSpace ISO_SUBSPACE_INIT(heap, destructibleCellHeapCellType.get(), NativeExecutable) // Hash:0x67567f95
    , numberObjectSpace ISO_SUBSPACE_INIT(heap, cellHeapCellType.get(), NumberObject)
    , plainObjectSpace ISO_SUBSPACE_INIT(heap, cellHeapCellType.get(), JSNonFinalObject) // Mainly used for prototypes.
    , promiseSpace ISO_SUBSPACE_INIT(heap, cellHeapCellType.get(), JSPromise)
    , propertyNameEnumeratorSpace ISO_SUBSPACE_INIT(heap, cellHeapCellType.get(), JSPropertyNameEnumerator)
    , propertyTableSpace ISO_SUBSPACE_INIT(heap, destructibleCellHeapCellType.get(), PropertyTable) // Hash:0xc6bc9f12
    , regExpSpace ISO_SUBSPACE_INIT(heap, destructibleCellHeapCellType.get(), RegExp)
    , regExpObjectSpace ISO_SUBSPACE_INIT(heap, cellHeapCellType.get(), RegExpObject)
    , ropeStringSpace ISO_SUBSPACE_INIT(heap, stringHeapCellType.get(), JSRopeString)
    , scopedArgumentsSpace ISO_SUBSPACE_INIT(heap, cellHeapCellType.get(), ScopedArguments)
    , sparseArrayValueMapSpace ISO_SUBSPACE_INIT(heap, destructibleCellHeapCellType.get(), SparseArrayValueMap)
    , stringSpace ISO_SUBSPACE_INIT(heap, stringHeapCellType.get(), JSString) // Hash:0x90cf758f
    , stringObjectSpace ISO_SUBSPACE_INIT(heap, cellHeapCellType.get(), StringObject)
    , structureChainSpace ISO_SUBSPACE_INIT(heap, cellHeapCellType.get(), StructureChain)
    , structureRareDataSpace ISO_SUBSPACE_INIT(heap, destructibleCellHeapCellType.get(), StructureRareData) // Hash:0xaca4e62d
    , structureSpace ISO_SUBSPACE_INIT(heap, destructibleCellHeapCellType.get(), Structure) // Hash:0x1f1bcdca
    , symbolTableSpace ISO_SUBSPACE_INIT(heap, destructibleCellHeapCellType.get(), SymbolTable) // Hash:0xc5215afd
    , executableToCodeBlockEdgesWithConstraints(executableToCodeBlockEdgeSpace)
    , executableToCodeBlockEdgesWithFinalizers(executableToCodeBlockEdgeSpace)
    , codeBlockSpace ISO_SUBSPACE_INIT(heap, destructibleCellHeapCellType.get(), CodeBlock) // Hash:0x77e66ec9
    , functionExecutableSpace ISO_SUBSPACE_INIT(heap, destructibleCellHeapCellType.get(), FunctionExecutable) // Hash:0x5d158f3
    , programExecutableSpace ISO_SUBSPACE_INIT(heap, destructibleCellHeapCellType.get(), ProgramExecutable) // Hash:0x527c77e7
    , unlinkedFunctionExecutableSpace ISO_SUBSPACE_INIT(heap, destructibleCellHeapCellType.get(), UnlinkedFunctionExecutable) // Hash:0xf6b828d9
    , vmType(vmType)
    , clientData(nullptr)
    , topEntryFrame(nullptr)
    , topCallFrame(CallFrame::noCaller())
    , deferredWorkTimer(DeferredWorkTimer::create(*this))
    , m_atomStringTable(vmType == Default ? Thread::current().atomStringTable() : new AtomStringTable)
    , propertyNames(nullptr)
    , emptyList(new ArgList)
    , machineCodeBytesPerBytecodeWordForBaselineJIT(makeUnique<SimpleStats>())
    , customGetterSetterFunctionMap(*this)
    , stringCache(*this)
    , symbolImplToSymbolMap(*this)
    , structureCache(*this)
    , interpreter(nullptr)
    , entryScope(nullptr)
    , m_regExpCache(new RegExpCache(this))
    , m_compactVariableMap(adoptRef(*(new CompactVariableMap)))
#if ENABLE(REGEXP_TRACING)
    , m_rtTraceList(new RTTraceList())
#endif
#if ENABLE(GC_VALIDATION)
    , m_initializingObjectClass(0)
#endif
    , m_stackPointerAtVMEntry(nullptr)
    , m_codeCache(makeUnique<CodeCache>())
    , m_builtinExecutables(makeUnique<BuiltinExecutables>(*this))
    , m_typeProfilerEnabledCount(0)
    , m_primitiveGigacageEnabled(IsWatched)
    , m_controlFlowProfilerEnabledCount(0)
{
    if (UNLIKELY(vmCreationShouldCrash))
        CRASH_WITH_INFO(0x4242424220202020, 0xbadbeef0badbeef, 0x1234123412341234, 0x1337133713371337);

    interpreter = new Interpreter(*this);
    StackBounds stack = Thread::current().stack();
    updateSoftReservedZoneSize(Options::softReservedZoneSize());
    setLastStackTop(stack.origin());

    JSRunLoopTimer::Manager::shared().registerVM(*this);

    // Need to be careful to keep everything consistent here
    JSLockHolder lock(this);
    AtomStringTable* existingEntryAtomStringTable = Thread::current().setCurrentAtomStringTable(m_atomStringTable);
    structureStructure.set(*this, Structure::createStructure(*this));
    structureRareDataStructure.set(*this, StructureRareData::createStructure(*this, nullptr, jsNull()));
    stringStructure.set(*this, JSString::createStructure(*this, nullptr, jsNull()));

    smallStrings.initializeCommonStrings(*this);

    propertyNames = new CommonIdentifiers(*this);
    terminatedExecutionErrorStructure.set(*this, TerminatedExecutionError::createStructure(*this, nullptr, jsNull()));
    propertyNameEnumeratorStructure.set(*this, JSPropertyNameEnumerator::createStructure(*this, nullptr, jsNull()));
    getterSetterStructure.set(*this, GetterSetter::createStructure(*this, nullptr, jsNull()));
    customGetterSetterStructure.set(*this, CustomGetterSetter::createStructure(*this, nullptr, jsNull()));
    domAttributeGetterSetterStructure.set(*this, DOMAttributeGetterSetter::createStructure(*this, nullptr, jsNull()));
    scopedArgumentsTableStructure.set(*this, ScopedArgumentsTable::createStructure(*this, nullptr, jsNull()));
    apiWrapperStructure.set(*this, JSAPIValueWrapper::createStructure(*this, nullptr, jsNull()));
    nativeExecutableStructure.set(*this, NativeExecutable::createStructure(*this, nullptr, jsNull()));
    evalExecutableStructure.set(*this, EvalExecutable::createStructure(*this, nullptr, jsNull()));
    programExecutableStructure.set(*this, ProgramExecutable::createStructure(*this, nullptr, jsNull()));
    functionExecutableStructure.set(*this, FunctionExecutable::createStructure(*this, nullptr, jsNull()));
#if ENABLE(WEBASSEMBLY)
    webAssemblyCodeBlockStructure.set(*this, JSWebAssemblyCodeBlock::createStructure(*this, nullptr, jsNull()));
#endif
    moduleProgramExecutableStructure.set(*this, ModuleProgramExecutable::createStructure(*this, nullptr, jsNull()));
    regExpStructure.set(*this, RegExp::createStructure(*this, nullptr, jsNull()));
    symbolStructure.set(*this, Symbol::createStructure(*this, nullptr, jsNull()));
    symbolTableStructure.set(*this, SymbolTable::createStructure(*this, nullptr, jsNull()));

    immutableButterflyStructures[arrayIndexFromIndexingType(CopyOnWriteArrayWithInt32) - NumberOfIndexingShapes].set(*this, JSImmutableButterfly::createStructure(*this, nullptr, jsNull(), CopyOnWriteArrayWithInt32));
    immutableButterflyStructures[arrayIndexFromIndexingType(CopyOnWriteArrayWithDouble) - NumberOfIndexingShapes].set(*this, JSImmutableButterfly::createStructure(*this, nullptr, jsNull(), CopyOnWriteArrayWithDouble));
    immutableButterflyStructures[arrayIndexFromIndexingType(CopyOnWriteArrayWithContiguous) - NumberOfIndexingShapes].set(*this, JSImmutableButterfly::createStructure(*this, nullptr, jsNull(), CopyOnWriteArrayWithContiguous));

    sourceCodeStructure.set(*this, JSSourceCode::createStructure(*this, nullptr, jsNull()));
    scriptFetcherStructure.set(*this, JSScriptFetcher::createStructure(*this, nullptr, jsNull()));
    scriptFetchParametersStructure.set(*this, JSScriptFetchParameters::createStructure(*this, nullptr, jsNull()));
    structureChainStructure.set(*this, StructureChain::createStructure(*this, nullptr, jsNull()));
    sparseArrayValueMapStructure.set(*this, SparseArrayValueMap::createStructure(*this, nullptr, jsNull()));
    templateObjectDescriptorStructure.set(*this, JSTemplateObjectDescriptor::createStructure(*this, nullptr, jsNull()));
    unlinkedFunctionExecutableStructure.set(*this, UnlinkedFunctionExecutable::createStructure(*this, nullptr, jsNull()));
    unlinkedProgramCodeBlockStructure.set(*this, UnlinkedProgramCodeBlock::createStructure(*this, nullptr, jsNull()));
    unlinkedEvalCodeBlockStructure.set(*this, UnlinkedEvalCodeBlock::createStructure(*this, nullptr, jsNull()));
    unlinkedFunctionCodeBlockStructure.set(*this, UnlinkedFunctionCodeBlock::createStructure(*this, nullptr, jsNull()));
    unlinkedModuleProgramCodeBlockStructure.set(*this, UnlinkedModuleProgramCodeBlock::createStructure(*this, nullptr, jsNull()));
    propertyTableStructure.set(*this, PropertyTable::createStructure(*this, nullptr, jsNull()));
    functionRareDataStructure.set(*this, FunctionRareData::createStructure(*this, nullptr, jsNull()));
    exceptionStructure.set(*this, Exception::createStructure(*this, nullptr, jsNull()));
    programCodeBlockStructure.set(*this, ProgramCodeBlock::createStructure(*this, nullptr, jsNull()));
    moduleProgramCodeBlockStructure.set(*this, ModuleProgramCodeBlock::createStructure(*this, nullptr, jsNull()));
    evalCodeBlockStructure.set(*this, EvalCodeBlock::createStructure(*this, nullptr, jsNull()));
    functionCodeBlockStructure.set(*this, FunctionCodeBlock::createStructure(*this, nullptr, jsNull()));
    hashMapBucketSetStructure.set(*this, HashMapBucket<HashMapBucketDataKey>::createStructure(*this, nullptr, jsNull()));
    hashMapBucketMapStructure.set(*this, HashMapBucket<HashMapBucketDataKeyValue>::createStructure(*this, nullptr, jsNull()));
    bigIntStructure.set(*this, JSBigInt::createStructure(*this, nullptr, jsNull()));
    executableToCodeBlockEdgeStructure.set(*this, ExecutableToCodeBlockEdge::createStructure(*this, nullptr, jsNull()));

    // Eagerly initialize constant cells since the concurrent compiler can access them.
    if (Options::useJIT()) {
        sentinelMapBucket();
        sentinelSetBucket();
    }
    {
        auto* bigInt = JSBigInt::tryCreateFrom(*this, 1);
        if (bigInt)
            heapBigIntConstantOne.set(*this, bigInt);
        else {
            if (success)
                *success = false;
            else
                RELEASE_ASSERT(bigInt);
        }
    }

    Thread::current().setCurrentAtomStringTable(existingEntryAtomStringTable);
    
#if !ENABLE(C_LOOP)
    initializeHostCallReturnValue(); // This is needed to convince the linker not to drop host call return support.
#endif
    
    Gigacage::addPrimitiveDisableCallback(primitiveGigacageDisabledCallback, this);

    heap.notifyIsSafeToCollect();
    
    LLInt::Data::performAssertions(*this);
    
    if (UNLIKELY(Options::useProfiler())) {
        m_perBytecodeProfiler = makeUnique<Profiler::Database>(*this);

        StringPrintStream pathOut;
        const char* profilerPath = getenv("JSC_PROFILER_PATH");
        if (profilerPath)
            pathOut.print(profilerPath, "/");
        pathOut.print("JSCProfile-", getCurrentProcessID(), "-", m_perBytecodeProfiler->databaseID(), ".json");
        m_perBytecodeProfiler->registerToSaveAtExit(pathOut.toCString().data());
    }

    callFrameForCatch = nullptr;

    // Initialize this last, as a free way of asserting that VM initialization itself
    // won't use this.
    m_typedArrayController = adoptRef(new SimpleTypedArrayController());

    m_bytecodeIntrinsicRegistry = makeUnique<BytecodeIntrinsicRegistry>(*this);

    if (Options::useTypeProfiler())
        enableTypeProfiler();
    if (Options::useControlFlowProfiler())
        enableControlFlowProfiler();
#if ENABLE(SAMPLING_PROFILER)
    if (Options::useSamplingProfiler()) {
        setShouldBuildPCToCodeOriginMapping();
        Ref<Stopwatch> stopwatch = Stopwatch::create();
        stopwatch->start();
        m_samplingProfiler = adoptRef(new SamplingProfiler(*this, WTFMove(stopwatch)));
        if (Options::samplingProfilerPath())
            m_samplingProfiler->registerForReportAtExit();
        m_samplingProfiler->start();
    }
#endif // ENABLE(SAMPLING_PROFILER)

    if (Options::useRandomizingFuzzerAgent())
        setFuzzerAgent(makeUnique<RandomizingFuzzerAgent>(*this));
    if (Options::useDoublePredictionFuzzerAgent())
        setFuzzerAgent(makeUnique<DoublePredictionFuzzerAgent>(*this));
    if (Options::useFileBasedFuzzerAgent())
        setFuzzerAgent(makeUnique<FileBasedFuzzerAgent>(*this));
    if (Options::usePredictionFileCreatingFuzzerAgent())
        setFuzzerAgent(makeUnique<PredictionFileCreatingFuzzerAgent>(*this));
    if (Options::useNarrowingNumberPredictionFuzzerAgent())
        setFuzzerAgent(makeUnique<NarrowingNumberPredictionFuzzerAgent>(*this));
    if (Options::useWideningNumberPredictionFuzzerAgent())
        setFuzzerAgent(makeUnique<WideningNumberPredictionFuzzerAgent>(*this));

    if (Options::alwaysGeneratePCToCodeOriginMap())
        setShouldBuildPCToCodeOriginMapping();

    if (Options::watchdog()) {
        Watchdog& watchdog = ensureWatchdog();
        watchdog.setTimeLimit(Seconds::fromMilliseconds(Options::watchdog()));
    }

#if ENABLE(JIT)
    // Make sure that any stubs that the JIT is going to use are initialized in non-compilation threads.
    if (Options::useJIT()) {
        jitStubs = makeUnique<JITThunks>();
#if ENABLE(FTL_JIT)
        ftlThunks = makeUnique<FTL::Thunks>();
#endif // ENABLE(FTL_JIT)
        getCTIInternalFunctionTrampolineFor(CodeForCall);
        getCTIInternalFunctionTrampolineFor(CodeForConstruct);
    }
#endif

    if (Options::forceDebuggerBytecodeGeneration() || Options::alwaysUseShadowChicken())
        ensureShadowChicken();

    VMInspector::instance().add(this);

    if (!g_jscConfig.disabledFreezingForTesting)
        Config::permanentlyFreeze();
}

static ReadWriteLock s_destructionLock;

void waitForVMDestruction()
{
    auto locker = holdLock(s_destructionLock.write());
}

VM::~VM()
{
    auto destructionLocker = holdLock(s_destructionLock.read());
    
    Gigacage::removePrimitiveDisableCallback(primitiveGigacageDisabledCallback, this);
    deferredWorkTimer->stopRunningTasks();
#if ENABLE(WEBASSEMBLY)
    if (Wasm::Worklist* worklist = Wasm::existingWorklistOrNull())
        worklist->stopAllPlansForContext(wasmContext);
#endif
    if (UNLIKELY(m_watchdog))
        m_watchdog->willDestroyVM(this);
    m_traps.willDestroyVM();
    VMInspector::instance().remove(this);

    // Never GC, ever again.
    heap.incrementDeferralDepth();

#if ENABLE(SAMPLING_PROFILER)
    if (m_samplingProfiler) {
        m_samplingProfiler->reportDataToOptionFile();
        m_samplingProfiler->shutdown();
    }
#endif // ENABLE(SAMPLING_PROFILER)
    
#if ENABLE(JIT)
    if (JITWorklist* worklist = JITWorklist::existingGlobalWorklistOrNull())
        worklist->completeAllForVM(*this);
#endif // ENABLE(JIT)

#if ENABLE(DFG_JIT)
    // Make sure concurrent compilations are done, but don't install them, since there is
    // no point to doing so.
    for (unsigned i = DFG::numberOfWorklists(); i--;) {
        if (DFG::Worklist* worklist = DFG::existingWorklistForIndexOrNull(i)) {
            worklist->removeNonCompilingPlansForVM(*this);
            worklist->waitUntilAllPlansForVMAreReady(*this);
            worklist->removeAllReadyPlansForVM(*this);
        }
    }
#endif // ENABLE(DFG_JIT)
    
    waitForAsynchronousDisassembly();
    
    // Clear this first to ensure that nobody tries to remove themselves from it.
    m_perBytecodeProfiler = nullptr;

    ASSERT(currentThreadIsHoldingAPILock());
    m_apiLock->willDestroyVM(this);
    smallStrings.setIsInitialized(false);
    heap.lastChanceToFinalize();

    JSRunLoopTimer::Manager::shared().unregisterVM(*this);
    
    delete interpreter;
#ifndef NDEBUG
    interpreter = reinterpret_cast<Interpreter*>(0xbbadbeef);
#endif

    delete emptyList;

    delete propertyNames;
    if (vmType != Default)
        delete m_atomStringTable;

    delete clientData;
    delete m_regExpCache;

#if ENABLE(REGEXP_TRACING)
    delete m_rtTraceList;
#endif

#if ENABLE(DFG_JIT)
    for (unsigned i = 0; i < m_scratchBuffers.size(); ++i)
        VMMalloc::free(m_scratchBuffers[i]);
#endif
}

void VM::primitiveGigacageDisabledCallback(void* argument)
{
    static_cast<VM*>(argument)->primitiveGigacageDisabled();
}

void VM::primitiveGigacageDisabled()
{
    if (m_apiLock->currentThreadIsHoldingLock()) {
        m_primitiveGigacageEnabled.fireAll(*this, "Primitive gigacage disabled");
        return;
    }
 
    // This is totally racy, and that's OK. The point is, it's up to the user to ensure that they pass the
    // uncaged buffer in a nicely synchronized manner.
    m_needToFirePrimitiveGigacageEnabled = true;
}

void VM::setLastStackTop(void* lastStackTop)
{ 
    m_lastStackTop = lastStackTop;
}

Ref<VM> VM::createContextGroup(HeapType heapType)
{
    return adoptRef(*new VM(APIContextGroup, heapType));
}

Ref<VM> VM::create(HeapType heapType)
{
    return adoptRef(*new VM(Default, heapType));
}

RefPtr<VM> VM::tryCreate(HeapType heapType)
{
    bool success = true;
    RefPtr<VM> vm = adoptRef(new VM(Default, heapType, &success));
    if (!success) {
        // Here, we're destructing a partially constructed VM and we know that
        // no one else can be using it at the same time. So, acquiring the lock
        // is superflous. However, we don't want to change how VMs are destructed.
        // Just going through the motion of acquiring the lock here allows us to
        // use the standard destruction process.

        // VM expects us to be holding the VM lock when destructing it. Acquiring
        // the lock also puts the VM in a state (e.g. acquiring heap access) that
        // is needed for destruction. The lock will hold the last reference to
        // the VM after we nullify the refPtr below. The VM will actually be
        // destructed in JSLockHolder's destructor.
        JSLockHolder lock(vm.get());
        vm = nullptr;
    }
    return vm;
}

bool VM::sharedInstanceExists()
{
    return sharedInstanceInternal();
}

VM& VM::sharedInstance()
{
    GlobalJSLock globalLock;
    VM*& instance = sharedInstanceInternal();
    if (!instance)
        instance = adoptRef(new VM(APIShared, SmallHeap)).leakRef();
    return *instance;
}

VM*& VM::sharedInstanceInternal()
{
    static VM* sharedInstance;
    return sharedInstance;
}

Watchdog& VM::ensureWatchdog()
{
    if (!m_watchdog)
        m_watchdog = adoptRef(new Watchdog(this));
    return *m_watchdog;
}

HeapProfiler& VM::ensureHeapProfiler()
{
    if (!m_heapProfiler)
        m_heapProfiler = makeUnique<HeapProfiler>(*this);
    return *m_heapProfiler;
}

#if ENABLE(SAMPLING_PROFILER)
SamplingProfiler& VM::ensureSamplingProfiler(Ref<Stopwatch>&& stopwatch)
{
    if (!m_samplingProfiler)
        m_samplingProfiler = adoptRef(new SamplingProfiler(*this, WTFMove(stopwatch)));
    return *m_samplingProfiler;
}
#endif // ENABLE(SAMPLING_PROFILER)

#if ENABLE(JIT)
static ThunkGenerator thunkGeneratorForIntrinsic(Intrinsic intrinsic)
{
    switch (intrinsic) {
    case CharCodeAtIntrinsic:
        return charCodeAtThunkGenerator;
    case CharAtIntrinsic:
        return charAtThunkGenerator;
    case StringPrototypeCodePointAtIntrinsic:
        return stringPrototypeCodePointAtThunkGenerator;
    case Clz32Intrinsic:
        return clz32ThunkGenerator;
    case FromCharCodeIntrinsic:
        return fromCharCodeThunkGenerator;
    case SqrtIntrinsic:
        return sqrtThunkGenerator;
    case AbsIntrinsic:
        return absThunkGenerator;
    case FloorIntrinsic:
        return floorThunkGenerator;
    case CeilIntrinsic:
        return ceilThunkGenerator;
    case TruncIntrinsic:
        return truncThunkGenerator;
    case RoundIntrinsic:
        return roundThunkGenerator;
    case ExpIntrinsic:
        return expThunkGenerator;
    case LogIntrinsic:
        return logThunkGenerator;
    case IMulIntrinsic:
        return imulThunkGenerator;
    case RandomIntrinsic:
        return randomThunkGenerator;
    case BoundFunctionCallIntrinsic:
        return boundFunctionCallGenerator;
    default:
        return nullptr;
    }
}

#endif // ENABLE(JIT)

NativeExecutable* VM::getHostFunction(NativeFunction function, NativeFunction constructor, const String& name)
{
    return getHostFunction(function, NoIntrinsic, constructor, nullptr, name);
}

static Ref<NativeJITCode> jitCodeForCallTrampoline()
{
    static NativeJITCode* result;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        result = new NativeJITCode(LLInt::getCodeRef<JSEntryPtrTag>(llint_native_call_trampoline), JITType::HostCallThunk, NoIntrinsic);
    });
    return makeRef(*result);
}

static Ref<NativeJITCode> jitCodeForConstructTrampoline()
{
    static NativeJITCode* result;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        result = new NativeJITCode(LLInt::getCodeRef<JSEntryPtrTag>(llint_native_construct_trampoline), JITType::HostCallThunk, NoIntrinsic);
    });
    return makeRef(*result);
}

NativeExecutable* VM::getHostFunction(NativeFunction function, Intrinsic intrinsic, NativeFunction constructor, const DOMJIT::Signature* signature, const String& name)
{
#if ENABLE(JIT)
    if (Options::useJIT()) {
        return jitStubs->hostFunctionStub(
            *this, function, constructor,
            intrinsic != NoIntrinsic ? thunkGeneratorForIntrinsic(intrinsic) : nullptr,
            intrinsic, signature, name);
    }
#endif // ENABLE(JIT)
    UNUSED_PARAM(intrinsic);
    UNUSED_PARAM(signature);
    return NativeExecutable::create(*this, jitCodeForCallTrampoline(), function, jitCodeForConstructTrampoline(), constructor, name);
}

NativeExecutable* VM::getBoundFunction(bool isJSFunction, bool canConstruct)
{
    bool slowCase = !isJSFunction;

    auto getOrCreate = [&] (Weak<NativeExecutable>& slot) -> NativeExecutable* {
        if (auto* cached = slot.get())
            return cached;
        NativeExecutable* result = getHostFunction(
            slowCase ? boundFunctionCall : boundThisNoArgsFunctionCall,
            slowCase ? NoIntrinsic : BoundFunctionCallIntrinsic,
            canConstruct ? (slowCase ? boundFunctionConstruct : boundThisNoArgsFunctionConstruct) : callHostFunctionAsConstructor, nullptr, String());
        slot = Weak<NativeExecutable>(result);
        return result;
    };

    if (slowCase) {
        if (canConstruct)
            return getOrCreate(m_slowCanConstructBoundExecutable);
        return getOrCreate(m_slowBoundExecutable);
    }
    if (canConstruct)
        return getOrCreate(m_fastCanConstructBoundExecutable);
    return getOrCreate(m_fastBoundExecutable);
}

MacroAssemblerCodePtr<JSEntryPtrTag> VM::getCTIInternalFunctionTrampolineFor(CodeSpecializationKind kind)
{
#if ENABLE(JIT)
    if (Options::useJIT()) {
        if (kind == CodeForCall)
            return jitStubs->ctiInternalFunctionCall(*this).retagged<JSEntryPtrTag>();
        return jitStubs->ctiInternalFunctionConstruct(*this).retagged<JSEntryPtrTag>();
    }
#endif
    if (kind == CodeForCall)
        return LLInt::getCodePtr<JSEntryPtrTag>(llint_internal_function_call_trampoline);
    return LLInt::getCodePtr<JSEntryPtrTag>(llint_internal_function_construct_trampoline);
}

VM::ClientData::~ClientData()
{
}

void VM::resetDateCache()
{
    dateCache.utcTimeOffsetCache.reset();
    dateCache.localTimeOffsetCache.reset();
    dateCache.cachedDateString = String();
    dateCache.cachedDateStringValue = std::numeric_limits<double>::quiet_NaN();
    dateCache.dateInstanceCache.reset();
}

void VM::whenIdle(Function<void()>&& callback)
{
    if (!entryScope) {
        callback();
        return;
    }

    entryScope->addDidPopListener(WTFMove(callback));
}

void VM::deleteAllLinkedCode(DeleteAllCodeEffort effort)
{
    whenIdle([=] () {
        heap.deleteAllCodeBlocks(effort);
    });
}

void VM::deleteAllCode(DeleteAllCodeEffort effort)
{
    whenIdle([=] () {
        m_codeCache->clear();
        m_regExpCache->deleteAllCode();
        heap.deleteAllCodeBlocks(effort);
        heap.deleteAllUnlinkedCodeBlocks(effort);
        heap.reportAbandonedObjectGraph();
    });
}

void VM::shrinkFootprintWhenIdle()
{
    whenIdle([=] () {
        sanitizeStackForVM(*this);
        deleteAllCode(DeleteAllCodeIfNotCollecting);
        heap.collectNow(Synchronousness::Sync, CollectionScope::Full);
        // FIXME: Consider stopping various automatic threads here.
        // https://bugs.webkit.org/show_bug.cgi?id=185447
        WTF::releaseFastMallocFreeMemory();
    });
}

SourceProviderCache* VM::addSourceProviderCache(SourceProvider* sourceProvider)
{
    auto addResult = sourceProviderCacheMap.add(sourceProvider, nullptr);
    if (addResult.isNewEntry)
        addResult.iterator->value = adoptRef(new SourceProviderCache);
    return addResult.iterator->value.get();
}

void VM::clearSourceProviderCaches()
{
    sourceProviderCacheMap.clear();
}

Exception* VM::throwException(JSGlobalObject* globalObject, Exception* exception)
{
    CallFrame* throwOriginFrame = topJSCallFrame();
    if (!throwOriginFrame)
        throwOriginFrame = globalObject->deprecatedCallFrameForDebugger();

    if (UNLIKELY(Options::breakOnThrow())) {
        CodeBlock* codeBlock = throwOriginFrame ? throwOriginFrame->codeBlock() : nullptr;
        dataLog("Throwing exception in call frame ", RawPointer(throwOriginFrame), " for code block ", codeBlock, "\n");
        CRASH();
    }

    interpreter->notifyDebuggerOfExceptionToBeThrown(*this, globalObject, throwOriginFrame, exception);

    setException(exception);

#if ENABLE(EXCEPTION_SCOPE_VERIFICATION)
    m_nativeStackTraceOfLastThrow = StackTrace::captureStackTrace(Options::unexpectedExceptionStackTraceLimit());
    m_throwingThread = &Thread::current();
#endif
    return exception;
}

Exception* VM::throwException(JSGlobalObject* globalObject, JSValue thrownValue)
{
    VM& vm = *this;
    Exception* exception = jsDynamicCast<Exception*>(vm, thrownValue);
    if (!exception)
        exception = Exception::create(*this, thrownValue);

    return throwException(globalObject, exception);
}

Exception* VM::throwException(JSGlobalObject* globalObject, JSObject* error)
{
    return throwException(globalObject, JSValue(error));
}

void VM::setStackPointerAtVMEntry(void* sp)
{
    m_stackPointerAtVMEntry = sp;
    updateStackLimits();
}

size_t VM::updateSoftReservedZoneSize(size_t softReservedZoneSize)
{
    size_t oldSoftReservedZoneSize = m_currentSoftReservedZoneSize;
    m_currentSoftReservedZoneSize = softReservedZoneSize;
#if ENABLE(C_LOOP)
    interpreter->cloopStack().setSoftReservedZoneSize(softReservedZoneSize);
#endif

    updateStackLimits();

    return oldSoftReservedZoneSize;
}

#if OS(WINDOWS)
// On Windows the reserved stack space consists of committed memory, a guard page, and uncommitted memory,
// where the guard page is a barrier between committed and uncommitted memory.
// When data from the guard page is read or written, the guard page is moved, and memory is committed.
// This is how the system grows the stack.
// When using the C stack on Windows we need to precommit the needed stack space.
// Otherwise we might crash later if we access uncommitted stack memory.
// This can happen if we allocate stack space larger than the page guard size (4K).
// The system does not get the chance to move the guard page, and commit more memory,
// and we crash if uncommitted memory is accessed.
// The MSVC compiler fixes this by inserting a call to the _chkstk() function,
// when needed, see http://support.microsoft.com/kb/100775.
// By touching every page up to the stack limit with a dummy operation,
// we force the system to move the guard page, and commit memory.

static void preCommitStackMemory(void* stackLimit)
{
    const int pageSize = 4096;
    for (volatile char* p = reinterpret_cast<char*>(&stackLimit); p > stackLimit; p -= pageSize) {
        char ch = *p;
        *p = ch;
    }
}
#endif

void VM::updateStackLimits()
{
#if OS(WINDOWS)
    void* lastSoftStackLimit = m_softStackLimit;
#endif

    const StackBounds& stack = Thread::current().stack();
    size_t reservedZoneSize = Options::reservedZoneSize();
    // We should have already ensured that Options::reservedZoneSize() >= minimumReserveZoneSize at
    // options initialization time, and the option value should not have been changed thereafter.
    // We don't have the ability to assert here that it hasn't changed, but we can at least assert
    // that the value is sane.
    RELEASE_ASSERT(reservedZoneSize >= minimumReservedZoneSize);

    if (m_stackPointerAtVMEntry) {
        char* startOfStack = reinterpret_cast<char*>(m_stackPointerAtVMEntry);
        m_softStackLimit = stack.recursionLimit(startOfStack, Options::maxPerThreadStackUsage(), m_currentSoftReservedZoneSize);
        m_stackLimit = stack.recursionLimit(startOfStack, Options::maxPerThreadStackUsage(), reservedZoneSize);
    } else {
        m_softStackLimit = stack.recursionLimit(m_currentSoftReservedZoneSize);
        m_stackLimit = stack.recursionLimit(reservedZoneSize);
    }

#if OS(WINDOWS)
    // We only need to precommit stack memory dictated by the VM::m_softStackLimit limit.
    // This is because VM::m_softStackLimit applies to stack usage by LLINT asm or JIT
    // generated code which can allocate stack space that the C++ compiler does not know
    // about. As such, we have to precommit that stack memory manually.
    //
    // In contrast, we do not need to worry about VM::m_stackLimit because that limit is
    // used exclusively by C++ code, and the C++ compiler will automatically commit the
    // needed stack pages.
    if (lastSoftStackLimit != m_softStackLimit)
        preCommitStackMemory(m_softStackLimit);
#endif
}

#if ENABLE(DFG_JIT)
void VM::gatherScratchBufferRoots(ConservativeRoots& conservativeRoots)
{
    auto lock = holdLock(m_scratchBufferLock);
    for (auto* scratchBuffer : m_scratchBuffers) {
        if (scratchBuffer->activeLength()) {
            void* bufferStart = scratchBuffer->dataBuffer();
            conservativeRoots.add(bufferStart, static_cast<void*>(static_cast<char*>(bufferStart) + scratchBuffer->activeLength()));
        }
    }
}

void VM::scanSideState(ConservativeRoots& roots) const
{
    ASSERT(heap.worldIsStopped());
    for (const auto& iter : m_checkpointSideState) {
        static_assert(sizeof(iter.value->tmps) / sizeof(JSValue) == maxNumCheckpointTmps);
        roots.add(iter.value->tmps, iter.value->tmps + maxNumCheckpointTmps);
    }
}
#endif

void VM::addCheckpointOSRSideState(CallFrame* callFrame, std::unique_ptr<CheckpointOSRExitSideState>&& payload)
{
    ASSERT(currentThreadIsHoldingAPILock());
    auto addResult = m_checkpointSideState.add(callFrame, WTFMove(payload));
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
}

std::unique_ptr<CheckpointOSRExitSideState> VM::findCheckpointOSRSideState(CallFrame* callFrame)
{
    ASSERT(currentThreadIsHoldingAPILock());
    auto sideState = m_checkpointSideState.take(callFrame);
    return sideState;
}

void logSanitizeStack(VM& vm)
{
    if (Options::verboseSanitizeStack() && vm.topCallFrame) {
        int dummy;
        auto& stackBounds = Thread::current().stack();
        dataLog(
            "Sanitizing stack for VM = ", RawPointer(&vm), " with top call frame at ", RawPointer(vm.topCallFrame),
            ", current stack pointer at ", RawPointer(&dummy), ", in ",
            pointerDump(vm.topCallFrame->codeBlock()), ", last code origin = ",
            vm.topCallFrame->codeOrigin(), ", last stack top = ", RawPointer(vm.lastStackTop()), ", in stack range [", RawPointer(stackBounds.origin()), ", ", RawPointer(stackBounds.end()), "]\n");
    }
}

#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
char* VM::acquireRegExpPatternContexBuffer()
{
    m_regExpPatternContextLock.lock();
    ASSERT(m_regExpPatternContextLock.isLocked());
    if (!m_regExpPatternContexBuffer)
        m_regExpPatternContexBuffer = makeUniqueArray<char>(VM::patternContextBufferSize);
    return m_regExpPatternContexBuffer.get();
}

void VM::releaseRegExpPatternContexBuffer()
{
    ASSERT(m_regExpPatternContextLock.isLocked());

    m_regExpPatternContextLock.unlock();
}
#endif

#if ENABLE(REGEXP_TRACING)
void VM::addRegExpToTrace(RegExp* regExp)
{
    gcProtect(regExp);
    m_rtTraceList->add(regExp);
}

void VM::dumpRegExpTrace()
{
    // The first RegExp object is ignored.  It is create by the RegExpPrototype ctor and not used.
    RTTraceList::iterator iter = ++m_rtTraceList->begin();
    
    if (iter != m_rtTraceList->end()) {
        dataLogF("\nRegExp Tracing\n");
        dataLogF("Regular Expression                              8 Bit          16 Bit        match()    Matches    Average\n");
        dataLogF(" <Match only / Match>                         JIT Addr      JIT Address       calls      found   String len\n");
        dataLogF("----------------------------------------+----------------+----------------+----------+----------+-----------\n");
    
        unsigned reCount = 0;
    
        for (; iter != m_rtTraceList->end(); ++iter, ++reCount) {
            (*iter)->printTraceData();
            gcUnprotect(*iter);
        }

        dataLogF("%d Regular Expressions\n", reCount);
    }
    
    m_rtTraceList->clear();
}
#else
void VM::dumpRegExpTrace()
{
}
#endif

WatchpointSet* VM::ensureWatchpointSetForImpureProperty(UniquedStringImpl* propertyName)
{
    auto result = m_impurePropertyWatchpointSets.add(propertyName, nullptr);
    if (result.isNewEntry)
        result.iterator->value = WatchpointSet::create(IsWatched);
    return result.iterator->value.get();
}

void VM::addImpureProperty(UniquedStringImpl* propertyName)
{
    if (RefPtr<WatchpointSet> watchpointSet = m_impurePropertyWatchpointSets.take(propertyName))
        watchpointSet->fireAll(*this, "Impure property added");
}

template<typename Func>
static bool enableProfilerWithRespectToCount(unsigned& counter, const Func& doEnableWork)
{
    bool needsToRecompile = false;
    if (!counter) {
        doEnableWork();
        needsToRecompile = true;
    }
    counter++;

    return needsToRecompile;
}

template<typename Func>
static bool disableProfilerWithRespectToCount(unsigned& counter, const Func& doDisableWork)
{
    RELEASE_ASSERT(counter > 0);
    bool needsToRecompile = false;
    counter--;
    if (!counter) {
        doDisableWork();
        needsToRecompile = true;
    }

    return needsToRecompile;
}

bool VM::enableTypeProfiler()
{
    auto enableTypeProfiler = [this] () {
        this->m_typeProfiler = makeUnique<TypeProfiler>();
        this->m_typeProfilerLog = makeUnique<TypeProfilerLog>(*this);
    };

    return enableProfilerWithRespectToCount(m_typeProfilerEnabledCount, enableTypeProfiler);
}

bool VM::disableTypeProfiler()
{
    auto disableTypeProfiler = [this] () {
        this->m_typeProfiler.reset(nullptr);
        this->m_typeProfilerLog.reset(nullptr);
    };

    return disableProfilerWithRespectToCount(m_typeProfilerEnabledCount, disableTypeProfiler);
}

bool VM::enableControlFlowProfiler()
{
    auto enableControlFlowProfiler = [this] () {
        this->m_controlFlowProfiler = makeUnique<ControlFlowProfiler>();
    };

    return enableProfilerWithRespectToCount(m_controlFlowProfilerEnabledCount, enableControlFlowProfiler);
}

bool VM::disableControlFlowProfiler()
{
    auto disableControlFlowProfiler = [this] () {
        this->m_controlFlowProfiler.reset(nullptr);
    };

    return disableProfilerWithRespectToCount(m_controlFlowProfilerEnabledCount, disableControlFlowProfiler);
}

void VM::dumpTypeProfilerData()
{
    if (!typeProfiler())
        return;

    typeProfilerLog()->processLogEntries(*this, "VM Dump Types"_s);
    typeProfiler()->dumpTypeProfilerData(*this);
}

void VM::queueMicrotask(JSGlobalObject& globalObject, Ref<Microtask>&& task)
{
    m_microtaskQueue.append(makeUnique<QueuedTask>(*this, &globalObject, WTFMove(task)));
}

void VM::callPromiseRejectionCallback(Strong<JSPromise>& promise)
{
    JSObject* callback = promise->globalObject()->unhandledRejectionCallback();
    if (!callback)
        return;

    auto scope = DECLARE_CATCH_SCOPE(*this);

    auto callData = getCallData(*this, callback);
    ASSERT(callData.type != CallData::Type::None);

    MarkedArgumentBuffer args;
    args.append(promise.get());
    args.append(promise->result(*this));
    call(promise->globalObject(), callback, callData, jsNull(), args);
    scope.clearException();
}

void VM::didExhaustMicrotaskQueue()
{
    auto unhandledRejections = WTFMove(m_aboutToBeNotifiedRejectedPromises);
    for (auto& promise : unhandledRejections) {
        if (promise->isHandled(*this))
            continue;

        callPromiseRejectionCallback(promise);
    }
}

void VM::promiseRejected(JSPromise* promise)
{
    m_aboutToBeNotifiedRejectedPromises.constructAndAppend(*this, promise);
}

void VM::drainMicrotasks()
{
    do {
        while (!m_microtaskQueue.isEmpty()) {
            m_microtaskQueue.takeFirst()->run();
            if (m_onEachMicrotaskTick)
                m_onEachMicrotaskTick(*this);
        }
        didExhaustMicrotaskQueue();
    } while (!m_microtaskQueue.isEmpty());
    finalizeSynchronousJSExecution();
}

void QueuedTask::run()
{
    m_microtask->run(m_globalObject.get());
}

void sanitizeStackForVM(VM& vm)
{
    logSanitizeStack(vm);
    if (vm.topCallFrame) {
        auto& stackBounds = Thread::current().stack();
        ASSERT(vm.currentThreadIsHoldingAPILock());
        ASSERT_UNUSED(stackBounds, stackBounds.contains(vm.lastStackTop()));
    }
#if ENABLE(C_LOOP)
    vm.interpreter->cloopStack().sanitizeStack();
#else
    sanitizeStackForVMImpl(&vm);
#endif
}

size_t VM::committedStackByteCount()
{
#if !ENABLE(C_LOOP)
    // When using the C stack, we don't know how many stack pages are actually
    // committed. So, we use the current stack usage as an estimate.
    uint8_t* current = bitwise_cast<uint8_t*>(currentStackPointer());
    uint8_t* high = bitwise_cast<uint8_t*>(Thread::current().stack().origin());
    return high - current;
#else
    return CLoopStack::committedByteCount();
#endif
}

#if ENABLE(C_LOOP)
bool VM::ensureStackCapacityForCLoop(Register* newTopOfStack)
{
    return interpreter->cloopStack().ensureCapacityFor(newTopOfStack);
}

bool VM::isSafeToRecurseSoftCLoop() const
{
    return interpreter->cloopStack().isSafeToRecurse();
}

void* VM::currentCLoopStackPointer() const
{
    return interpreter->cloopStack().currentStackPointer();
}
#endif // ENABLE(C_LOOP)

#if ENABLE(EXCEPTION_SCOPE_VERIFICATION)
void VM::verifyExceptionCheckNeedIsSatisfied(unsigned recursionDepth, ExceptionEventLocation& location)
{
    if (!Options::validateExceptionChecks())
        return;

    if (UNLIKELY(m_needExceptionCheck)) {
        auto throwDepth = m_simulatedThrowPointRecursionDepth;
        auto& throwLocation = m_simulatedThrowPointLocation;

        dataLog(
            "ERROR: Unchecked JS exception:\n"
            "    This scope can throw a JS exception: ", throwLocation, "\n"
            "        (ExceptionScope::m_recursionDepth was ", throwDepth, ")\n"
            "    But the exception was unchecked as of this scope: ", location, "\n"
            "        (ExceptionScope::m_recursionDepth was ", recursionDepth, ")\n"
            "\n");

        StringPrintStream out;
        std::unique_ptr<StackTrace> currentTrace = StackTrace::captureStackTrace(Options::unexpectedExceptionStackTraceLimit());

        if (Options::dumpSimulatedThrows()) {
            out.println("The simulated exception was thrown at:");
            m_nativeStackTraceOfLastSimulatedThrow->dump(out, "    ");
            out.println();
        }
        out.println("Unchecked exception detected at:");
        currentTrace->dump(out, "    ");
        out.println();

        dataLog(out.toCString());
        RELEASE_ASSERT(!m_needExceptionCheck);
    }
}
#endif

#if USE(CF)
void VM::setRunLoop(CFRunLoopRef runLoop)
{
    ASSERT(runLoop);
    m_runLoop = runLoop;
    JSRunLoopTimer::Manager::shared().didChangeRunLoop(*this, runLoop);
}
#endif // USE(CF)

ScratchBuffer* VM::scratchBufferForSize(size_t size)
{
    if (!size)
        return nullptr;

    auto locker = holdLock(m_scratchBufferLock);

    if (size > m_sizeOfLastScratchBuffer) {
        // Protect against a N^2 memory usage pathology by ensuring
        // that at worst, we get a geometric series, meaning that the
        // total memory usage is somewhere around
        // max(scratch buffer size) * 4.
        m_sizeOfLastScratchBuffer = size * 2;

        ScratchBuffer* newBuffer = ScratchBuffer::create(m_sizeOfLastScratchBuffer);
        RELEASE_ASSERT(newBuffer);
        m_scratchBuffers.append(newBuffer);
    }

    ScratchBuffer* result = m_scratchBuffers.last();
    return result;
}

void VM::clearScratchBuffers()
{
    auto lock = holdLock(m_scratchBufferLock);
    for (auto* scratchBuffer : m_scratchBuffers)
        scratchBuffer->setActiveLength(0);
}

void VM::ensureShadowChicken()
{
    if (m_shadowChicken)
        return;
    m_shadowChicken = makeUnique<ShadowChicken>();
}

#define DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(name, heapCellType, type) \
    IsoSubspace* VM::name##Slow() \
    { \
        ASSERT(!m_##name); \
        auto space = makeUnique<IsoSubspace> ISO_SUBSPACE_INIT(heap, heapCellType, type); \
        WTF::storeStoreFence(); \
        m_##name = WTFMove(space); \
        return m_##name.get(); \
    }


DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(apiGlobalObjectSpace, apiGlobalObjectHeapCellType.get(), JSAPIGlobalObject)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(apiValueWrapperSpace, cellHeapCellType.get(), JSAPIValueWrapper)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(arrayBufferSpace, cellHeapCellType.get(), JSArrayBuffer)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(arrayIteratorSpace, cellHeapCellType.get(), JSArrayIterator)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(asyncGeneratorSpace, cellHeapCellType.get(), JSAsyncGenerator)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(bigIntObjectSpace, cellHeapCellType.get(), BigIntObject)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(booleanObjectSpace, cellHeapCellType.get(), BooleanObject)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(boundFunctionSpace, cellHeapCellType.get(), JSBoundFunction) // Hash:0xd7916d41
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(callbackConstructorSpace, callbackConstructorHeapCellType.get(), JSCallbackConstructor)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(callbackGlobalObjectSpace, callbackGlobalObjectHeapCellType.get(), JSCallbackObject<JSGlobalObject>)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(callbackFunctionSpace, cellHeapCellType.get(), JSCallbackFunction) // Hash:0xe7648ebc
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(callbackObjectSpace, callbackObjectHeapCellType.get(), JSCallbackObject<JSNonFinalObject>)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(customGetterSetterFunctionSpace, cellHeapCellType.get(), JSCustomGetterSetterFunction) // Hash:0x18091000
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(dataViewSpace, cellHeapCellType.get(), JSDataView)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(debuggerScopeSpace, cellHeapCellType.get(), DebuggerScope)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(errorInstanceSpace, errorInstanceHeapCellType.get(), ErrorInstance) // Hash:0x3f40d4a
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(float32ArraySpace, cellHeapCellType.get(), JSFloat32Array)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(float64ArraySpace, cellHeapCellType.get(), JSFloat64Array)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(functionRareDataSpace, destructibleCellHeapCellType.get(), FunctionRareData)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(generatorSpace, cellHeapCellType.get(), JSGenerator)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(globalObjectSpace, globalObjectHeapCellType.get(), JSGlobalObject)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(injectedScriptHostSpace, injectedScriptHostSpaceHeapCellType.get(), Inspector::JSInjectedScriptHost)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(int8ArraySpace, cellHeapCellType.get(), JSInt8Array)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(int16ArraySpace, cellHeapCellType.get(), JSInt16Array)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(int32ArraySpace, cellHeapCellType.get(), JSInt32Array)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(javaScriptCallFrameSpace, javaScriptCallFrameHeapCellType.get(), Inspector::JSJavaScriptCallFrame)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(jsModuleRecordSpace, jsModuleRecordHeapCellType.get(), JSModuleRecord)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(mapBucketSpace, cellHeapCellType.get(), JSMap::BucketType)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(mapIteratorSpace, cellHeapCellType.get(), JSMapIterator)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(mapSpace, cellHeapCellType.get(), JSMap)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(moduleNamespaceObjectSpace, moduleNamespaceObjectHeapCellType.get(), JSModuleNamespaceObject)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(nativeStdFunctionSpace, nativeStdFunctionHeapCellType.get(), JSNativeStdFunction) // Hash:0x70ed61e4
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(proxyObjectSpace, cellHeapCellType.get(), ProxyObject)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(proxyRevokeSpace, cellHeapCellType.get(), ProxyRevoke) // Hash:0xb506a939
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(scopedArgumentsTableSpace, destructibleCellHeapCellType.get(), ScopedArgumentsTable)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(scriptFetchParametersSpace, destructibleCellHeapCellType.get(), JSScriptFetchParameters)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(scriptFetcherSpace, destructibleCellHeapCellType.get(), JSScriptFetcher)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(setBucketSpace, cellHeapCellType.get(), JSSet::BucketType)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(setIteratorSpace, cellHeapCellType.get(), JSSetIterator)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(setSpace, cellHeapCellType.get(), JSSet)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(strictEvalActivationSpace, cellHeapCellType.get(), StrictEvalActivation)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(stringIteratorSpace, cellHeapCellType.get(), JSStringIterator)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(sourceCodeSpace, destructibleCellHeapCellType.get(), JSSourceCode)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(symbolSpace, destructibleCellHeapCellType.get(), Symbol)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(symbolObjectSpace, cellHeapCellType.get(), SymbolObject)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(templateObjectDescriptorSpace, destructibleCellHeapCellType.get(), JSTemplateObjectDescriptor)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(uint8ArraySpace, cellHeapCellType.get(), JSUint8Array)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(uint8ClampedArraySpace, cellHeapCellType.get(), JSUint8ClampedArray)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(uint16ArraySpace, cellHeapCellType.get(), JSUint16Array)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(uint32ArraySpace, cellHeapCellType.get(), JSUint32Array)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(unlinkedEvalCodeBlockSpace, destructibleCellHeapCellType.get(), UnlinkedEvalCodeBlock)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(unlinkedFunctionCodeBlockSpace, destructibleCellHeapCellType.get(), UnlinkedFunctionCodeBlock)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(unlinkedModuleProgramCodeBlockSpace, destructibleCellHeapCellType.get(), UnlinkedModuleProgramCodeBlock)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(unlinkedProgramCodeBlockSpace, destructibleCellHeapCellType.get(), UnlinkedProgramCodeBlock)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(weakMapSpace, weakMapHeapCellType.get(), JSWeakMap) // Hash:0x662b12a3
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(weakSetSpace, weakSetHeapCellType.get(), JSWeakSet) // Hash:0x4c781b30
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(weakObjectRefSpace, cellHeapCellType.get(), JSWeakObjectRef) // Hash:0x8ec68f1f
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(withScopeSpace, cellHeapCellType.get(), JSWithScope)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(finalizationRegistrySpace, finalizationRegistryCellType.get(), JSFinalizationRegistry)
#if JSC_OBJC_API_ENABLED
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(apiWrapperObjectSpace, apiWrapperObjectHeapCellType.get(), JSCallbackObject<JSAPIWrapperObject>)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(objCCallbackFunctionSpace, objCCallbackFunctionHeapCellType.get(), ObjCCallbackFunction) // Hash:0x10f610b8
#endif
#ifdef JSC_GLIB_API_ENABLED
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(apiWrapperObjectSpace, apiWrapperObjectHeapCellType.get(), JSCallbackObject<JSAPIWrapperObject>)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(jscCallbackFunctionSpace, jscCallbackFunctionHeapCellType.get(), JSCCallbackFunction)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(callbackAPIWrapperGlobalObjectSpace, callbackAPIWrapperGlobalObjectHeapCellType.get(), JSCallbackObject<JSAPIWrapperGlobalObject>)
#endif
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(intlCollatorSpace, intlCollatorHeapCellType.get(), IntlCollator)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(intlDateTimeFormatSpace, intlDateTimeFormatHeapCellType.get(), IntlDateTimeFormat)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(intlLocaleSpace, intlLocaleHeapCellType.get(), IntlLocale)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(intlNumberFormatSpace, intlNumberFormatHeapCellType.get(), IntlNumberFormat)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(intlPluralRulesSpace, intlPluralRulesHeapCellType.get(), IntlPluralRules)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(intlRelativeTimeFormatSpace, intlRelativeTimeFormatHeapCellType.get(), IntlRelativeTimeFormat)
#if ENABLE(WEBASSEMBLY)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(jsToWasmICCalleeSpace, cellHeapCellType.get(), JSToWasmICCallee)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(webAssemblyCodeBlockSpace, webAssemblyCodeBlockHeapCellType.get(), JSWebAssemblyCodeBlock) // Hash:0x9ad995cd
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(webAssemblyFunctionSpace, webAssemblyFunctionHeapCellType.get(), WebAssemblyFunction) // Hash:0x8b7c32db
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(webAssemblyGlobalSpace, webAssemblyGlobalHeapCellType.get(), JSWebAssemblyGlobal)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(webAssemblyInstanceSpace, webAssemblyInstanceHeapCellType.get(), JSWebAssemblyInstance)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(webAssemblyMemorySpace, webAssemblyMemoryHeapCellType.get(), JSWebAssemblyMemory)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(webAssemblyModuleSpace, webAssemblyModuleHeapCellType.get(), JSWebAssemblyModule)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(webAssemblyModuleRecordSpace, webAssemblyModuleRecordHeapCellType.get(), WebAssemblyModuleRecord)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(webAssemblyTableSpace, webAssemblyTableHeapCellType.get(), JSWebAssemblyTable)
DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW(webAssemblyWrapperFunctionSpace, cellHeapCellType.get(), WebAssemblyWrapperFunction) // Hash:0xd4a5ff01
#endif

#undef DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER_SLOW

#define DYNAMIC_SPACE_AND_SET_DEFINE_MEMBER_SLOW(name, heapCellType, type) \
    IsoSubspace* VM::name##Slow() \
    { \
        ASSERT(!m_##name); \
        auto space = makeUnique<SpaceAndSet> ISO_SUBSPACE_INIT(heap, heapCellType, type); \
        WTF::storeStoreFence(); \
        m_##name = WTFMove(space); \
        return &m_##name->space; \
    }

DYNAMIC_SPACE_AND_SET_DEFINE_MEMBER_SLOW(evalExecutableSpace, destructibleCellHeapCellType.get(), EvalExecutable) // Hash:0x958e3e9d
DYNAMIC_SPACE_AND_SET_DEFINE_MEMBER_SLOW(moduleProgramExecutableSpace, destructibleCellHeapCellType.get(), ModuleProgramExecutable) // Hash:0x6506fa3c

#undef DYNAMIC_SPACE_AND_SET_DEFINE_MEMBER_SLOW

JSCell* VM::sentinelSetBucketSlow()
{
    ASSERT(!m_sentinelSetBucket);
    auto* sentinel = JSSet::BucketType::createSentinel(*this);
    m_sentinelSetBucket.set(*this, sentinel);
    return sentinel;
}

JSCell* VM::sentinelMapBucketSlow()
{
    ASSERT(!m_sentinelMapBucket);
    auto* sentinel = JSMap::BucketType::createSentinel(*this);
    m_sentinelMapBucket.set(*this, sentinel);
    return sentinel;
}

JSPropertyNameEnumerator* VM::emptyPropertyNameEnumeratorSlow()
{
    ASSERT(!m_emptyPropertyNameEnumerator);
    PropertyNameArray propertyNames(*this, PropertyNameMode::Strings, PrivateSymbolMode::Exclude);
    auto* enumerator = JSPropertyNameEnumerator::create(*this, nullptr, 0, 0, WTFMove(propertyNames));
    m_emptyPropertyNameEnumerator.set(*this, enumerator);
    return enumerator;
}

JSGlobalObject* VM::deprecatedVMEntryGlobalObject(JSGlobalObject* globalObject) const
{
    if (entryScope)
        return entryScope->globalObject();
    return globalObject;
}

void VM::setCrashOnVMCreation(bool shouldCrash)
{
    vmCreationShouldCrash = shouldCrash;
}

void VM::addLoopHintExecutionCounter(const Instruction* instruction)
{
    auto locker = holdLock(m_loopHintExecutionCountLock);
    auto addResult = m_loopHintExecutionCounts.add(instruction, std::pair<unsigned, std::unique_ptr<uint64_t>>(0, nullptr));
    if (addResult.isNewEntry) {
        auto ptr = WTF::makeUniqueWithoutFastMallocCheck<uint64_t>();
        *ptr = 0;
        addResult.iterator->value.second = WTFMove(ptr);
    }
    ++addResult.iterator->value.first;
}

uint64_t* VM::getLoopHintExecutionCounter(const Instruction* instruction)
{
    auto locker = holdLock(m_loopHintExecutionCountLock);
    auto iter = m_loopHintExecutionCounts.find(instruction);
    return iter->value.second.get();
}

void VM::removeLoopHintExecutionCounter(const Instruction* instruction)
{
    auto locker = holdLock(m_loopHintExecutionCountLock);
    auto iter = m_loopHintExecutionCounts.find(instruction);
    RELEASE_ASSERT(!!iter->value.first);
    --iter->value.first;
    if (!iter->value.first)
        m_loopHintExecutionCounts.remove(iter);
}

} // namespace JSC
