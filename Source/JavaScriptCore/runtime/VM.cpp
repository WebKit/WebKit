/*
 * Copyright (C) 2008-2023 Apple Inc. All rights reserved.
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

#include "AbortReason.h"
#include "AccessCase.h"
#include "AggregateError.h"
#include "ArgList.h"
#include "BuiltinExecutables.h"
#include "BytecodeIntrinsicRegistry.h"
#include "CheckpointOSRExitSideState.h"
#include "CodeBlock.h"
#include "CodeCache.h"
#include "CommonIdentifiers.h"
#include "ControlFlowProfiler.h"
#include "CustomGetterSetter.h"
#include "DOMAttributeGetterSetter.h"
#include "Debugger.h"
#include "DeferredWorkTimer.h"
#include "Disassembler.h"
#include "DoublePredictionFuzzerAgent.h"
#include "ErrorInstance.h"
#include "EvalCodeBlock.h"
#include "Exception.h"
#include "FTLThunks.h"
#include "FileBasedFuzzerAgent.h"
#include "FunctionCodeBlock.h"
#include "FunctionExecutable.h"
#include "GetterSetter.h"
#include "GigacageAlignedMemoryAllocator.h"
#include "HasOwnPropertyCache.h"
#include "Heap.h"
#include "HeapProfiler.h"
#include "IncrementalSweeper.h"
#include "Interpreter.h"
#include "IntlCache.h"
#include "JITCode.h"
#include "JITOperationList.h"
#include "JITSizeStatistics.h"
#include "JITThunks.h"
#include "JITWorklist.h"
#include "JSAPIValueWrapper.h"
#include "JSBigInt.h"
#include "JSGlobalObject.h"
#include "JSImmutableButterfly.h"
#include "JSLock.h"
#include "JSMap.h"
#include "JSMicrotask.h"
#include "JSPromise.h"
#include "JSPropertyNameEnumerator.h"
#include "JSScriptFetchParameters.h"
#include "JSScriptFetcher.h"
#include "JSSet.h"
#include "JSSourceCode.h"
#include "JSTemplateObjectDescriptor.h"
#include "LLIntData.h"
#include "LLIntExceptions.h"
#include "MarkedBlockInlines.h"
#include "MegamorphicCache.h"
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
#include "PropertyTable.h"
#include "RandomizingFuzzerAgent.h"
#include "RegExpCache.h"
#include "ResourceExhaustion.h"
#include "SamplingProfiler.h"
#include "ScopedArguments.h"
#include "ShadowChicken.h"
#include "SimpleTypedArrayController.h"
#include "SourceProviderCache.h"
#include "StrongInlines.h"
#include "StructureChain.h"
#include "StructureInlines.h"
#include "TestRunnerUtils.h"
#include "ThunkGenerators.h"
#include "TypeProfiler.h"
#include "TypeProfilerLog.h"
#include "VMEntryScopeInlines.h"
#include "VMInlines.h"
#include "VMInspector.h"
#include "VariableEnvironment.h"
#include "WaiterListManager.h"
#include "WasmInstance.h"
#include "WasmWorklist.h"
#include "Watchdog.h"
#include "WeakGCMapInlines.h"
#include "WideningNumberPredictionFuzzerAgent.h"
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/ProcessID.h>
#include <wtf/ReadWriteLock.h>
#include <wtf/SimpleStats.h>
#include <wtf/StackTrace.h>
#include <wtf/StringPrintStream.h>
#include <wtf/SystemTracing.h>
#include <wtf/Threading.h>
#include <wtf/text/AtomStringTable.h>

#if ENABLE(C_LOOP)
#include "CLoopStackInlines.h"
#endif

#if ENABLE(DFG_JIT)
#include "ConservativeRoots.h"
#endif

#if ENABLE(REGEXP_TRACING)
#include "RegExp.h"
#endif

namespace JSC {

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

static bool vmCreationShouldCrash = false;

VM::VM(VMType vmType, HeapType heapType, WTF::RunLoop* runLoop, bool* success)
    : m_identifier(VMIdentifier::generate())
    , m_apiLock(adoptRef(new JSLock(this)))
    , m_runLoop(runLoop ? *runLoop : WTF::RunLoop::current())
    , m_random(Options::seedOfVMRandomForFuzzer() ? Options::seedOfVMRandomForFuzzer() : cryptographicallyRandomNumber<uint32_t>())
    , m_heapRandom(Options::seedOfVMRandomForFuzzer() ? Options::seedOfVMRandomForFuzzer() : cryptographicallyRandomNumber<uint32_t>())
    , m_integrityRandom(*this)
    , heap(*this, heapType)
    , clientHeap(heap)
    , vmType(vmType)
    , topCallFrame(CallFrame::noCaller())
    , deferredWorkTimer(DeferredWorkTimer::create(*this))
    , m_atomStringTable(vmType == Default ? Thread::current().atomStringTable() : new AtomStringTable)
    , emptyList(new ArgList)
    , machineCodeBytesPerBytecodeWordForBaselineJIT(makeUnique<SimpleStats>())
    , symbolImplToSymbolMap(*this)
    , m_regExpCache(new RegExpCache(this))
    , m_compactVariableMap(adoptRef(*new CompactTDZEnvironmentMap))
    , m_codeCache(makeUnique<CodeCache>())
    , m_intlCache(makeUnique<IntlCache>())
    , m_builtinExecutables(makeUnique<BuiltinExecutables>(*this))
    , m_syncWaiter(adoptRef(*new Waiter(this)))
{
    if (UNLIKELY(vmCreationShouldCrash || g_jscConfig.vmCreationDisallowed))
        CRASH_WITH_EXTRA_SECURITY_IMPLICATION_AND_INFO(VMCreationDisallowed, "VM creation disallowed"_s, 0x4242424220202020, 0xbadbeef0badbeef, 0x1234123412341234, 0x1337133713371337);

    VMInspector::instance().add(this);

    updateSoftReservedZoneSize(Options::softReservedZoneSize());
    setLastStackTop(Thread::current());
    stringSplitIndice.reserveInitialCapacity(256);

    JSRunLoopTimer::Manager::shared().registerVM(*this);

    // Need to be careful to keep everything consistent here
    JSLockHolder lock(this);
    AtomStringTable* existingEntryAtomStringTable = Thread::current().setCurrentAtomStringTable(m_atomStringTable);
    structureStructure.setWithoutWriteBarrier(Structure::createStructure(*this));
    structureRareDataStructure.setWithoutWriteBarrier(StructureRareData::createStructure(*this, nullptr, jsNull()));
    stringStructure.setWithoutWriteBarrier(JSString::createStructure(*this, nullptr, jsNull()));

    smallStrings.initializeCommonStrings(*this);
    numericStrings.initializeSmallIntCache(*this);

    propertyNames = new CommonIdentifiers(*this);
    propertyNameEnumeratorStructure.setWithoutWriteBarrier(JSPropertyNameEnumerator::createStructure(*this, nullptr, jsNull()));
    getterSetterStructure.setWithoutWriteBarrier(GetterSetter::createStructure(*this, nullptr, jsNull()));
    customGetterSetterStructure.setWithoutWriteBarrier(CustomGetterSetter::createStructure(*this, nullptr, jsNull()));
    domAttributeGetterSetterStructure.setWithoutWriteBarrier(DOMAttributeGetterSetter::createStructure(*this, nullptr, jsNull()));
    scopedArgumentsTableStructure.setWithoutWriteBarrier(ScopedArgumentsTable::createStructure(*this, nullptr, jsNull()));
    apiWrapperStructure.setWithoutWriteBarrier(JSAPIValueWrapper::createStructure(*this, nullptr, jsNull()));
    nativeExecutableStructure.setWithoutWriteBarrier(NativeExecutable::createStructure(*this, nullptr, jsNull()));
    evalExecutableStructure.setWithoutWriteBarrier(EvalExecutable::createStructure(*this, nullptr, jsNull()));
    programExecutableStructure.setWithoutWriteBarrier(ProgramExecutable::createStructure(*this, nullptr, jsNull()));
    functionExecutableStructure.setWithoutWriteBarrier(FunctionExecutable::createStructure(*this, nullptr, jsNull()));
    moduleProgramExecutableStructure.setWithoutWriteBarrier(ModuleProgramExecutable::createStructure(*this, nullptr, jsNull()));
    regExpStructure.setWithoutWriteBarrier(RegExp::createStructure(*this, nullptr, jsNull()));
    symbolStructure.setWithoutWriteBarrier(Symbol::createStructure(*this, nullptr, jsNull()));
    symbolTableStructure.setWithoutWriteBarrier(SymbolTable::createStructure(*this, nullptr, jsNull()));

    immutableButterflyStructures[arrayIndexFromIndexingType(CopyOnWriteArrayWithInt32) - NumberOfIndexingShapes].setWithoutWriteBarrier(JSImmutableButterfly::createStructure(*this, nullptr, jsNull(), CopyOnWriteArrayWithInt32));
    Structure* copyOnWriteArrayWithContiguousStructure = JSImmutableButterfly::createStructure(*this, nullptr, jsNull(), CopyOnWriteArrayWithContiguous);
    immutableButterflyStructures[arrayIndexFromIndexingType(CopyOnWriteArrayWithDouble) - NumberOfIndexingShapes].setWithoutWriteBarrier(Options::allowDoubleShape() ? JSImmutableButterfly::createStructure(*this, nullptr, jsNull(), CopyOnWriteArrayWithDouble) : copyOnWriteArrayWithContiguousStructure);
    immutableButterflyStructures[arrayIndexFromIndexingType(CopyOnWriteArrayWithContiguous) - NumberOfIndexingShapes].setWithoutWriteBarrier(copyOnWriteArrayWithContiguousStructure);

    sourceCodeStructure.setWithoutWriteBarrier(JSSourceCode::createStructure(*this, nullptr, jsNull()));
    scriptFetcherStructure.setWithoutWriteBarrier(JSScriptFetcher::createStructure(*this, nullptr, jsNull()));
    scriptFetchParametersStructure.setWithoutWriteBarrier(JSScriptFetchParameters::createStructure(*this, nullptr, jsNull()));
    structureChainStructure.setWithoutWriteBarrier(StructureChain::createStructure(*this, nullptr, jsNull()));
    sparseArrayValueMapStructure.setWithoutWriteBarrier(SparseArrayValueMap::createStructure(*this, nullptr, jsNull()));
    templateObjectDescriptorStructure.setWithoutWriteBarrier(JSTemplateObjectDescriptor::createStructure(*this, nullptr, jsNull()));
    unlinkedFunctionExecutableStructure.setWithoutWriteBarrier(UnlinkedFunctionExecutable::createStructure(*this, nullptr, jsNull()));
    unlinkedProgramCodeBlockStructure.setWithoutWriteBarrier(UnlinkedProgramCodeBlock::createStructure(*this, nullptr, jsNull()));
    unlinkedEvalCodeBlockStructure.setWithoutWriteBarrier(UnlinkedEvalCodeBlock::createStructure(*this, nullptr, jsNull()));
    unlinkedFunctionCodeBlockStructure.setWithoutWriteBarrier(UnlinkedFunctionCodeBlock::createStructure(*this, nullptr, jsNull()));
    unlinkedModuleProgramCodeBlockStructure.setWithoutWriteBarrier(UnlinkedModuleProgramCodeBlock::createStructure(*this, nullptr, jsNull()));
    propertyTableStructure.setWithoutWriteBarrier(PropertyTable::createStructure(*this, nullptr, jsNull()));
    functionRareDataStructure.setWithoutWriteBarrier(FunctionRareData::createStructure(*this, nullptr, jsNull()));
    exceptionStructure.setWithoutWriteBarrier(Exception::createStructure(*this, nullptr, jsNull()));
    programCodeBlockStructure.setWithoutWriteBarrier(ProgramCodeBlock::createStructure(*this, nullptr, jsNull()));
    moduleProgramCodeBlockStructure.setWithoutWriteBarrier(ModuleProgramCodeBlock::createStructure(*this, nullptr, jsNull()));
    evalCodeBlockStructure.setWithoutWriteBarrier(EvalCodeBlock::createStructure(*this, nullptr, jsNull()));
    functionCodeBlockStructure.setWithoutWriteBarrier(FunctionCodeBlock::createStructure(*this, nullptr, jsNull()));
    hashMapBucketSetStructure.setWithoutWriteBarrier(HashMapBucket<HashMapBucketDataKey>::createStructure(*this, nullptr, jsNull()));
    hashMapBucketMapStructure.setWithoutWriteBarrier(HashMapBucket<HashMapBucketDataKeyValue>::createStructure(*this, nullptr, jsNull()));
    bigIntStructure.setWithoutWriteBarrier(JSBigInt::createStructure(*this, nullptr, jsNull()));

    // Eagerly initialize constant cells since the concurrent compiler can access them.
    if (Options::useJIT()) {
        sentinelMapBucket();
        sentinelSetBucket();
        emptyPropertyNameEnumerator();
        ensureMegamorphicCache();
    }
    {
        auto* bigInt = JSBigInt::tryCreateFrom(*this, 1);
        if (bigInt)
            heapBigIntConstantOne.setWithoutWriteBarrier(bigInt);
        else {
            if (success)
                *success = false;
            else
                RELEASE_ASSERT_RESOURCE_AVAILABLE(bigInt, MemoryExhaustion, "Crash intentionally because memory is exhausted.");
        }
    }

    Thread::current().setCurrentAtomStringTable(existingEntryAtomStringTable);
    
    Gigacage::addPrimitiveDisableCallback(primitiveGigacageDisabledCallback, this);

    heap.notifyIsSafeToCollect();
    
    LLInt::Data::performAssertions(*this);
    
    if (UNLIKELY(Options::useProfiler())) {
        m_perBytecodeProfiler = makeUnique<Profiler::Database>(*this);

        if (UNLIKELY(Options::dumpProfilerDataAtExit())) {
            StringPrintStream pathOut;
            const char* profilerPath = getenv("JSC_PROFILER_PATH");
            if (profilerPath)
                pathOut.print(profilerPath, "/");
            pathOut.print("JSCProfile-", getCurrentProcessID(), "-", m_perBytecodeProfiler->databaseID(), ".json");
            m_perBytecodeProfiler->registerToSaveAtExit(pathOut.toCString().data());
        }
    }

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
        ensureSamplingProfiler(WTFMove(stopwatch));
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

    if (Options::useTracePoints())
        requestEntryScopeService(EntryScopeService::TracePoints);

#if ENABLE(JIT)
    // Make sure that any stubs that the JIT is going to use are initialized in non-compilation threads.
    if (Options::useJIT()) {
        jitStubs = makeUnique<JITThunks>();
#if ENABLE(FTL_JIT)
        ftlThunks = makeUnique<FTL::Thunks>();
#endif // ENABLE(FTL_JIT)
        getCTIInternalFunctionTrampolineFor(CodeForCall);
        getCTIInternalFunctionTrampolineFor(CodeForConstruct);
        m_sharedJITStubs = makeUnique<SharedJITStubSet>();
        getBoundFunction(/* isJSFunction */ true);
    }
#endif // ENABLE(JIT)

    if (Options::forceDebuggerBytecodeGeneration() || Options::alwaysUseShadowChicken())
        ensureShadowChicken();

#if ENABLE(JIT)
    if (Options::dumpBaselineJITSizeStatistics() || Options::dumpDFGJITSizeStatistics())
        jitSizeStatistics = makeUnique<JITSizeStatistics>();
#endif

    if (!g_jscConfig.disabledFreezingForTesting)
        Config::permanentlyFreeze();

    // We must set this at the end only after the VM is fully initialized.
    WTF::storeStoreFence();
    m_isInService = true;
}

static ReadWriteLock s_destructionLock;

void waitForVMDestruction()
{
    Locker locker { s_destructionLock.write() };
}

VM::~VM()
{
    Locker destructionLocker { s_destructionLock.read() };

    if (Options::useAtomicsWaitAsync() && vmType == Default)
        WaiterListManager::singleton().unregisterVM(this);

    Gigacage::removePrimitiveDisableCallback(primitiveGigacageDisabledCallback, this);
    deferredWorkTimer->stopRunningTasks();
#if ENABLE(WEBASSEMBLY)
    if (Wasm::Worklist* worklist = Wasm::existingWorklistOrNull())
        worklist->stopAllPlansForContext(*this);
#endif
    if (UNLIKELY(m_watchdog))
        m_watchdog->willDestroyVM(this);
    m_traps.willDestroyVM();
    m_isInService = false;
    WTF::storeStoreFence();

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
        worklist->cancelAllPlansForVM(*this);
#endif // ENABLE(JIT)
    
    waitForAsynchronousDisassembly();
    
    // Clear this first to ensure that nobody tries to remove themselves from it.
    m_perBytecodeProfiler = nullptr;

    ASSERT(currentThreadIsHoldingAPILock());
    m_apiLock->willDestroyVM(this);
    smallStrings.setIsInitialized(false);
    heap.lastChanceToFinalize();

    JSRunLoopTimer::Manager::shared().unregisterVM(*this);

    VMInspector::instance().remove(this);

    delete emptyList;

    delete propertyNames;
    if (vmType != Default)
        delete m_atomStringTable;

    delete clientData;
    delete m_regExpCache;

#if ENABLE(DFG_JIT)
    for (unsigned i = 0; i < m_scratchBuffers.size(); ++i)
        VMMalloc::free(m_scratchBuffers[i]);
#endif

#if ENABLE(JIT)
    m_sharedJITStubs = nullptr;
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
    requestEntryScopeService(EntryScopeService::FirePrimitiveGigacageEnabled);
}

void VM::setLastStackTop(const Thread& thread)
{
    m_lastStackTop = thread.savedLastStackTop();
    auto& stack = thread.stack();
    RELEASE_ASSERT(stack.contains(m_lastStackTop), 0x5510, m_lastStackTop, stack.origin(), stack.end());
}

Ref<VM> VM::createContextGroup(HeapType heapType)
{
    return adoptRef(*new VM(APIContextGroup, heapType));
}

Ref<VM> VM::create(HeapType heapType, WTF::RunLoop* runLoop)
{
    return adoptRef(*new VM(Default, heapType, runLoop));
}

RefPtr<VM> VM::tryCreate(HeapType heapType, WTF::RunLoop* runLoop)
{
    bool success = true;
    RefPtr<VM> vm = adoptRef(new VM(Default, heapType, runLoop, &success));
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
        instance = adoptRef(new VM(APIShared, HeapType::Small)).leakRef();
    return *instance;
}

VM*& VM::sharedInstanceInternal()
{
    static VM* sharedInstance;
    return sharedInstance;
}

Watchdog& VM::ensureWatchdog()
{
    if (!m_watchdog) {
        m_watchdog = adoptRef(new Watchdog(this));
        ensureTerminationException();
        requestEntryScopeService(EntryScopeService::Watchdog);
    }
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
    if (!m_samplingProfiler) {
        m_samplingProfiler = adoptRef(new SamplingProfiler(*this, WTFMove(stopwatch)));
        requestEntryScopeService(EntryScopeService::SamplingProfiler);
    }
    return *m_samplingProfiler;
}

void VM::enableSamplingProfiler()
{
    SamplingProfiler* profiler = samplingProfiler();
    if (!profiler)
        profiler = &ensureSamplingProfiler(Stopwatch::create());
    profiler->start();
}

void VM::disableSamplingProfiler()
{
    SamplingProfiler* profiler = samplingProfiler();
    if (!profiler)
        profiler = &ensureSamplingProfiler(Stopwatch::create());
    {
        Locker locker { profiler->getLock() };
        profiler->pause();
    }
}

RefPtr<JSON::Value> VM::takeSamplingProfilerSamplesAsJSON()
{
    SamplingProfiler* profiler = samplingProfiler();
    if (!profiler)
        return nullptr;
    return profiler->stackTracesAsJSON();
}

#endif // ENABLE(SAMPLING_PROFILER)

static StringImpl::StaticStringImpl terminationErrorString { "JavaScript execution terminated." };
Exception* VM::ensureTerminationException()
{
    if (!m_terminationException) {
        JSString* terminationError = jsNontrivialString(*this, terminationErrorString);
        m_terminationException = Exception::create(*this, terminationError, Exception::DoNotCaptureStack);
    }
    return m_terminationException;
}

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
    case GlobalIsNaNIntrinsic:
        return globalIsNaNThunkGenerator;
    case NumberIsNaNIntrinsic:
        return numberIsNaNThunkGenerator;
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
    case RemoteFunctionCallIntrinsic:
        return remoteFunctionCallGenerator;
    case NumberConstructorIntrinsic:
        return numberConstructorCallThunkGenerator;
    case StringConstructorIntrinsic:
        return stringConstructorCallThunkGenerator;
    default:
        return nullptr;
    }
}

MacroAssemblerCodeRef<JITThunkPtrTag> VM::getCTIStub(ThunkGenerator generator)
{
    return jitStubs->ctiStub(*this, generator);
}

#endif // ENABLE(JIT)

NativeExecutable* VM::getHostFunction(NativeFunction function, ImplementationVisibility implementationVisibility, NativeFunction constructor, const String& name)
{
    return getHostFunction(function, implementationVisibility, NoIntrinsic, constructor, nullptr, name);
}

static Ref<NativeJITCode> jitCodeForCallTrampoline()
{
    static NativeJITCode* result;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        result = new NativeJITCode(LLInt::getCodeRef<JSEntryPtrTag>(llint_native_call_trampoline), JITType::HostCallThunk, NoIntrinsic);
    });
    return *result;
}

static Ref<NativeJITCode> jitCodeForConstructTrampoline()
{
    static NativeJITCode* result;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        result = new NativeJITCode(LLInt::getCodeRef<JSEntryPtrTag>(llint_native_construct_trampoline), JITType::HostCallThunk, NoIntrinsic);
    });
    return *result;
}

NativeExecutable* VM::getHostFunction(NativeFunction function, ImplementationVisibility implementationVisibility, Intrinsic intrinsic, NativeFunction constructor, const DOMJIT::Signature* signature, const String& name)
{
#if ENABLE(JIT)
    if (Options::useJIT()) {
        return jitStubs->hostFunctionStub(
            *this, toTagged(function), toTagged(constructor),
            intrinsic != NoIntrinsic ? thunkGeneratorForIntrinsic(intrinsic) : nullptr,
            implementationVisibility, intrinsic, signature, name);
    }
#endif // ENABLE(JIT)
    UNUSED_PARAM(intrinsic);
    UNUSED_PARAM(signature);
    return NativeExecutable::create(*this, jitCodeForCallTrampoline(), toTagged(function), jitCodeForConstructTrampoline(), toTagged(constructor), implementationVisibility, name);
}

NativeExecutable* VM::getBoundFunction(bool isJSFunction)
{
    bool slowCase = !isJSFunction;

    auto getOrCreate = [&](WriteBarrier<NativeExecutable>& slot) -> NativeExecutable* {
        if (auto* cached = slot.get())
            return cached;
        NativeExecutable* result = getHostFunction(
            slowCase ? boundFunctionCall : boundThisNoArgsFunctionCall,
            ImplementationVisibility::Private, // Bound function's visibility is private on the stack.
            slowCase ? NoIntrinsic : BoundFunctionCallIntrinsic,
            boundFunctionConstruct, nullptr, String());
        slot.setWithoutWriteBarrier(result);
        return result;
    };

    if (slowCase)
        return getOrCreate(m_slowCanConstructBoundExecutable);
    return getOrCreate(m_fastCanConstructBoundExecutable);
}

NativeExecutable* VM::getRemoteFunction(bool isJSFunction)
{
    bool slowCase = !isJSFunction;
    auto getOrCreate = [&] (Weak<NativeExecutable>& slot) -> NativeExecutable* {
        if (auto* cached = slot.get())
            return cached;

        Intrinsic intrinsic = NoIntrinsic;
        if (!slowCase) {
#if !(OS(WINDOWS) && CPU(X86_64))
            intrinsic = RemoteFunctionCallIntrinsic;
#endif
        }

        NativeExecutable* result = getHostFunction(
            slowCase ? remoteFunctionCallGeneric : remoteFunctionCallForJSFunction,
            ImplementationVisibility::Public, intrinsic,
            callHostFunctionAsConstructor, nullptr, String());
        slot = Weak<NativeExecutable>(result);
        return result;
    };

    if (slowCase)
        return getOrCreate(m_slowRemoteFunctionExecutable);
    return getOrCreate(m_fastRemoteFunctionExecutable);
}

CodePtr<JSEntryPtrTag> VM::getCTIInternalFunctionTrampolineFor(CodeSpecializationKind kind)
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

MacroAssemblerCodeRef<JSEntryPtrTag> VM::getCTILinkCall()
{
#if ENABLE(JIT)
    if (Options::useJIT())
        return getCTIStub(linkCallThunkGenerator).template retagged<JSEntryPtrTag>();
#endif
    return LLInt::getCodeRef<JSEntryPtrTag>(llint_link_call_trampoline);
}

MacroAssemblerCodeRef<JSEntryPtrTag> VM::getCTIThrowExceptionFromCallSlowPath()
{
#if ENABLE(JIT)
    if (Options::useJIT())
        return getCTIStub(throwExceptionFromCallSlowPathGenerator).template retagged<JSEntryPtrTag>();
#endif
    return LLInt::callToThrow(*this).template retagged<JSEntryPtrTag>();
}

MacroAssemblerCodeRef<JITStubRoutinePtrTag> VM::getCTIVirtualCall(CallMode callMode)
{
#if ENABLE(JIT)
    if (Options::useJIT())
        return virtualThunkFor(*this, callMode);
#endif
    switch (callMode) {
    case CallMode::Regular:
        return LLInt::getCodeRef<JITStubRoutinePtrTag>(llint_virtual_call_trampoline);
    case CallMode::Tail:
        return LLInt::getCodeRef<JITStubRoutinePtrTag>(llint_virtual_tail_call_trampoline);
    case CallMode::Construct:
        return LLInt::getCodeRef<JITStubRoutinePtrTag>(llint_virtual_construct_trampoline);
    }
    return LLInt::getCodeRef<JITStubRoutinePtrTag>(llint_virtual_call_trampoline);
}

void VM::whenIdle(Function<void()>&& callback)
{
    if (!entryScope) {
        callback();
        return;
    }
    m_didPopListeners.append(WTFMove(callback));
    requestEntryScopeService(EntryScopeService::PopListeners);
}

void VM::deleteAllLinkedCode(DeleteAllCodeEffort effort)
{
    whenIdle([=, this] () {
        heap.deleteAllCodeBlocks(effort);
    });
}

void VM::deleteAllCode(DeleteAllCodeEffort effort)
{
    whenIdle([=, this] () {
        m_codeCache->clear();
        m_regExpCache->deleteAllCode();
        heap.deleteAllCodeBlocks(effort);
        heap.deleteAllUnlinkedCodeBlocks(effort);
        heap.reportAbandonedObjectGraph();
    });
}

void VM::shrinkFootprintWhenIdle()
{
    whenIdle([=, this] () {
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

bool VM::hasExceptionsAfterHandlingTraps()
{
    if (UNLIKELY(traps().needHandling(VMTraps::NonDebuggerAsyncEvents)))
        m_traps.handleTraps(VMTraps::NonDebuggerAsyncEvents);
    return exception();
}

void VM::clearException()
{
#if ENABLE(EXCEPTION_SCOPE_VERIFICATION)
    m_needExceptionCheck = false;
    m_nativeStackTraceOfLastThrow = nullptr;
    m_throwingThread = nullptr;
#endif
    m_exception = nullptr;
    traps().clearTrapBit(VMTraps::NeedExceptionHandling);
}

void VM::setException(Exception* exception)
{
    ASSERT(!exception || !isTerminationException(exception) || hasTerminationRequest());
    m_exception = exception;
    m_lastException = exception;
    if (exception)
        traps().setTrapBit(VMTraps::NeedExceptionHandling);
}

void VM::throwTerminationException()
{
    ASSERT(hasTerminationRequest());
    ASSERT(!m_traps.isDeferringTermination());
    setException(terminationException());
    if (m_executionForbiddenOnTermination)
        setExecutionForbidden();
}

Exception* VM::throwException(JSGlobalObject* globalObject, Exception* exceptionToThrow)
{
    // The TerminationException should never be overridden.
    if (hasPendingTerminationException())
        return m_exception;

    // The TerminationException is not like ordinary exceptions that should be
    // reported to the debugger. The fact that the TerminationException uses the
    // exception handling mechanism is just a VM internal implementation detail.
    // It is not meaningful to report it to the debugger as an exception.
    if (isTerminationException(exceptionToThrow)) {
        // Note: we can only get here is we're just re-throwing the TerminationException
        // from C++ functions to propagate it. If we're throwing it for the first
        // time, we would have gone through VM::throwTerminationException().
        setException(exceptionToThrow);
        return exceptionToThrow;
    }

    CallFrame* throwOriginFrame = topJSCallFrame();
    if (UNLIKELY(Options::breakOnThrow())) {
        CodeBlock* codeBlock = throwOriginFrame && !throwOriginFrame->isWasmFrame() ? throwOriginFrame->codeBlock() : nullptr;
        dataLog("Throwing exception in call frame ", RawPointer(throwOriginFrame), " for code block ", codeBlock, "\n");
        WTFBreakpointTrap();
    }

    interpreter.notifyDebuggerOfExceptionToBeThrown(*this, globalObject, throwOriginFrame, exceptionToThrow);

    setException(exceptionToThrow);

#if ENABLE(EXCEPTION_SCOPE_VERIFICATION)
    m_nativeStackTraceOfLastThrow = StackTrace::captureStackTrace(Options::unexpectedExceptionStackTraceLimit());
    m_throwingThread = &Thread::current();
#endif
    return exceptionToThrow;
}

Exception* VM::throwException(JSGlobalObject* globalObject, JSValue thrownValue)
{
    Exception* exception = jsDynamicCast<Exception*>(thrownValue);
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
    interpreter.cloopStack().setSoftReservedZoneSize(softReservedZoneSize);
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
    void* lastSoftStackLimit = m_softStackLimit;

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

    if (lastSoftStackLimit != m_softStackLimit) {
#if OS(WINDOWS)
        // We only need to precommit stack memory dictated by the VM::m_softStackLimit limit.
        // This is because VM::m_softStackLimit applies to stack usage by LLINT asm or JIT
        // generated code which can allocate stack space that the C++ compiler does not know
        // about. As such, we have to precommit that stack memory manually.
        //
        // In contrast, we do not need to worry about VM::m_stackLimit because that limit is
        // used exclusively by C++ code, and the C++ compiler will automatically commit the
        // needed stack pages.
        preCommitStackMemory(m_softStackLimit);
#endif
#if ENABLE(WEBASSEMBLY)
        for (auto& instance : m_wasmInstances.values())
            instance->updateSoftStackLimit(m_softStackLimit);
#endif
    }
}

#if ENABLE(DFG_JIT)
void VM::gatherScratchBufferRoots(ConservativeRoots& conservativeRoots)
{
    Locker locker { m_scratchBufferLock };
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
    for (const auto& sideState : m_checkpointSideState) {
        static_assert(sizeof(sideState->tmps) / sizeof(JSValue) == maxNumCheckpointTmps);
        roots.add(sideState->tmps, sideState->tmps + maxNumCheckpointTmps);
    }
}
#endif

void VM::pushCheckpointOSRSideState(std::unique_ptr<CheckpointOSRExitSideState>&& payload)
{
    ASSERT(currentThreadIsHoldingAPILock());
    ASSERT(payload->associatedCallFrame);
#if ASSERT_ENABLED
    for (const auto& sideState : m_checkpointSideState)
        ASSERT(sideState->associatedCallFrame != payload->associatedCallFrame);
#endif
    m_checkpointSideState.append(WTFMove(payload));

#if ASSERT_ENABLED
    auto bounds = StackBounds::currentThreadStackBounds();
    void* previousCallFrame = bounds.end();
    for (size_t i = m_checkpointSideState.size(); i--;) {
        auto* callFrame = m_checkpointSideState[i]->associatedCallFrame;
        if (!bounds.contains(callFrame))
            break;
        ASSERT(previousCallFrame < callFrame);
        previousCallFrame = callFrame;
    }
#endif
}

std::unique_ptr<CheckpointOSRExitSideState> VM::popCheckpointOSRSideState(CallFrame* expectedCallFrame)
{
    ASSERT(currentThreadIsHoldingAPILock());
    auto sideState = m_checkpointSideState.takeLast();
    RELEASE_ASSERT(sideState->associatedCallFrame == expectedCallFrame);
    return sideState;
}

void VM::popAllCheckpointOSRSideStateUntil(CallFrame* target)
{
    ASSERT(currentThreadIsHoldingAPILock());
    auto bounds = StackBounds::currentThreadStackBounds().withSoftOrigin(target);
    ASSERT(bounds.contains(target));

    // We have to worry about migrating from another thread since there may be no checkpoints in our thread but one in the other threads.
    while (m_checkpointSideState.size() && bounds.contains(m_checkpointSideState.last()->associatedCallFrame))
        m_checkpointSideState.takeLast();
    m_checkpointSideState.shrinkToFit();
}

static void logSanitizeStack(VM& vm)
{
    if (UNLIKELY(Options::verboseSanitizeStack())) {
        auto& stackBounds = Thread::current().stack();
        dataLogLn("Sanitizing stack for VM = ", RawPointer(&vm), ", current stack pointer at ", RawPointer(currentStackPointer()), ", last stack top = ", RawPointer(vm.lastStackTop()), ", in stack range (", RawPointer(stackBounds.end()), ", ", RawPointer(stackBounds.origin()), "]");
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
    m_rtTraceList.add(regExp);
}

void VM::dumpRegExpTrace()
{
    // The first RegExp object is ignored. It is created by the RegExpPrototype ctor and not used.
    RTTraceList::iterator iter = ++m_rtTraceList.begin();
    
    if (iter != m_rtTraceList.end()) {
        dataLogF("\nRegExp Tracing\n");
        dataLogF("Regular Expression                              8 Bit          16 Bit        match()    Matches    Average\n");
        dataLogF(" <Match only / Match>                         JIT Addr      JIT Address       calls      found   String len\n");
        dataLogF("----------------------------------------+----------------+----------------+----------+----------+-----------\n");
    
        unsigned reCount = 0;
    
        for (; iter != m_rtTraceList.end(); ++iter, ++reCount) {
            (*iter)->printTraceData();
            gcUnprotect(*iter);
        }

        dataLogF("%d Regular Expressions\n", reCount);
    }
    
    m_rtTraceList.clear();
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

void VM::queueMicrotask(QueuedTask&& task)
{
    m_microtaskQueue.enqueue(WTFMove(task));
}

void VM::callPromiseRejectionCallback(Strong<JSPromise>& promise)
{
    JSObject* callback = promise->globalObject()->unhandledRejectionCallback();
    if (!callback)
        return;

    auto scope = DECLARE_CATCH_SCOPE(*this);

    auto callData = JSC::getCallData(callback);
    ASSERT(callData.type != CallData::Type::None);

    MarkedArgumentBuffer args;
    args.append(promise.get());
    args.append(promise->result(*this));
    ASSERT(!args.hasOverflowed());
    call(promise->globalObject(), callback, callData, jsNull(), args);
    scope.clearException();
}

void VM::didExhaustMicrotaskQueue()
{
    do {
        auto unhandledRejections = WTFMove(m_aboutToBeNotifiedRejectedPromises);
        for (auto& promise : unhandledRejections) {
            if (promise->isHandled(*this))
                continue;

            callPromiseRejectionCallback(promise);
            if (UNLIKELY(hasPendingTerminationException()))
                return;
        }
    } while (!m_aboutToBeNotifiedRejectedPromises.isEmpty());
}

void VM::promiseRejected(JSPromise* promise)
{
    m_aboutToBeNotifiedRejectedPromises.constructAndAppend(*this, promise);
}

void VM::drainMicrotasks()
{
    if (UNLIKELY(m_drainMicrotaskDelayScopeCount))
        return;

    if (UNLIKELY(executionForbidden()))
        m_microtaskQueue.clear();
    else {
        do {
            while (!m_microtaskQueue.isEmpty()) {
                auto task = m_microtaskQueue.dequeue();
                task.run();
                if (UNLIKELY(hasPendingTerminationException()))
                    return;
                if (m_onEachMicrotaskTick)
                    m_onEachMicrotaskTick(*this);
            }
            didExhaustMicrotaskQueue();
            if (UNLIKELY(hasPendingTerminationException()))
                return;
        } while (!m_microtaskQueue.isEmpty());
    }
    finalizeSynchronousJSExecution();
}

void sanitizeStackForVM(VM& vm)
{
    auto& thread = Thread::current();
    auto& stack = thread.stack();
    if (!vm.currentThreadIsHoldingAPILock())
        return; // vm.lastStackTop() may not be set up correctly if JSLock is not held.

    logSanitizeStack(vm);

    RELEASE_ASSERT(stack.contains(vm.lastStackTop()), 0xaa10, vm.lastStackTop(), stack.origin(), stack.end());
#if ENABLE(C_LOOP)
    vm.interpreter.cloopStack().sanitizeStack();
#else
    sanitizeStackForVMImpl(&vm);
#endif
    RELEASE_ASSERT(stack.contains(vm.lastStackTop()), 0xaa20, vm.lastStackTop(), stack.origin(), stack.end());
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
    return interpreter.cloopStack().ensureCapacityFor(newTopOfStack);
}

bool VM::isSafeToRecurseSoftCLoop() const
{
    return interpreter.cloopStack().isSafeToRecurse();
}

void* VM::currentCLoopStackPointer() const
{
    return interpreter.cloopStack().currentStackPointer();
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
            out.println(StackTracePrinter { *m_nativeStackTraceOfLastSimulatedThrow, "    " });
        }
        out.println("Unchecked exception detected at:");
        out.println(StackTracePrinter { *currentTrace, "    " });

        dataLog(out.toCString());
        RELEASE_ASSERT(!m_needExceptionCheck);
    }
}
#endif

ScratchBuffer* VM::scratchBufferForSize(size_t size)
{
    if (!size)
        return nullptr;

    Locker locker { m_scratchBufferLock };

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
    Locker locker { m_scratchBufferLock };
    for (auto* scratchBuffer : m_scratchBuffers)
        scratchBuffer->setActiveLength(0);
    clearEntryScopeService(EntryScopeService::ClearScratchBuffers);
}

bool VM::isScratchBuffer(void* ptr)
{
    Locker locker { m_scratchBufferLock };
    for (auto* scratchBuffer : m_scratchBuffers) {
        if (scratchBuffer->dataBuffer() == ptr)
            return true;
    }
    return false;
}

Ref<Waiter> VM::syncWaiter()
{
    m_syncWaiter->setVM(this);
    return m_syncWaiter;
}

void VM::ensureShadowChicken()
{
    if (m_shadowChicken)
        return;
    m_shadowChicken = makeUnique<ShadowChicken>();
}

JSCell* VM::sentinelSetBucketSlow()
{
    ASSERT(!m_sentinelSetBucket);
    auto* sentinel = JSSet::BucketType::createSentinel(*this);
    m_sentinelSetBucket.setWithoutWriteBarrier(sentinel);
    return sentinel;
}

JSCell* VM::sentinelMapBucketSlow()
{
    ASSERT(!m_sentinelMapBucket);
    auto* sentinel = JSMap::BucketType::createSentinel(*this);
    m_sentinelMapBucket.setWithoutWriteBarrier(sentinel);
    return sentinel;
}

JSPropertyNameEnumerator* VM::emptyPropertyNameEnumeratorSlow()
{
    ASSERT(!m_emptyPropertyNameEnumerator);
    PropertyNameArray propertyNames(*this, PropertyNameMode::Strings, PrivateSymbolMode::Exclude);
    auto* enumerator = JSPropertyNameEnumerator::create(*this, nullptr, 0, 0, WTFMove(propertyNames));
    m_emptyPropertyNameEnumerator.setWithoutWriteBarrier(enumerator);
    return enumerator;
}

void VM::executeEntryScopeServicesOnEntry()
{
    if (UNLIKELY(hasEntryScopeServiceRequest(EntryScopeService::FirePrimitiveGigacageEnabled))) {
        m_primitiveGigacageEnabled.fireAll(*this, "Primitive gigacage disabled asynchronously");
        clearEntryScopeService(EntryScopeService::FirePrimitiveGigacageEnabled);
    }

    // Reset the date cache between JS invocations to force the VM to
    // observe time zone changes.
    dateCache.resetIfNecessary();

    auto* watchdog = this->watchdog();
    if (UNLIKELY(watchdog))
        watchdog->enteredVM();

#if ENABLE(SAMPLING_PROFILER)
    auto* samplingProfiler = this->samplingProfiler();
    if (UNLIKELY(samplingProfiler))
        samplingProfiler->noticeVMEntry();
#endif

    if (UNLIKELY(Options::useTracePoints()))
        tracePoint(VMEntryScopeStart);
}

void VM::executeEntryScopeServicesOnExit()
{
    if (UNLIKELY(Options::useTracePoints()))
        tracePoint(VMEntryScopeEnd);

    auto* watchdog = this->watchdog();
    if (UNLIKELY(watchdog))
        watchdog->exitedVM();

    if (hasEntryScopeServiceRequest(EntryScopeService::PopListeners)) {
        auto listeners = WTFMove(m_didPopListeners);
        for (auto& listener : listeners)
            listener();
        clearEntryScopeService(EntryScopeService::PopListeners);
    }

    // Normally, we want to clear the hasTerminationRequest flag here. However, if the
    // VMTraps::NeedTermination bit is still set at this point, then it means that
    // VMTraps::handleTraps() has not yet been called for this termination request. As a
    // result, the TerminationException has not been thrown yet. Some client code relies
    // on detecting the presence of the TerminationException in order to signal that a
    // termination was requested. Hence, don't clear the hasTerminationRequest flag until
    // VMTraps::handleTraps() has been called, and the TerminationException is thrown.
    //
    // Note: perhaps there's a better way for the client to know that a termination was
    // requested (after all, the request came from the client). However, this is how the
    // client code currently works. Changing that will take some significant effort to hunt
    // down all the places in client code that currently rely on this behavior.
    if (!traps().needHandling(VMTraps::NeedTermination))
        clearHasTerminationRequest();

    clearScratchBuffers();
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

void VM::addLoopHintExecutionCounter(const JSInstruction* instruction)
{
    Locker locker { m_loopHintExecutionCountLock };
    auto addResult = m_loopHintExecutionCounts.add(instruction, std::pair<unsigned, std::unique_ptr<uintptr_t>>(0, nullptr));
    if (addResult.isNewEntry) {
        auto ptr = WTF::makeUniqueWithoutFastMallocCheck<uintptr_t>();
        *ptr = 0;
        addResult.iterator->value.second = WTFMove(ptr);
    }
    ++addResult.iterator->value.first;
}

uintptr_t* VM::getLoopHintExecutionCounter(const JSInstruction* instruction)
{
    Locker locker { m_loopHintExecutionCountLock };
    auto iter = m_loopHintExecutionCounts.find(instruction);
    return iter->value.second.get();
}

void VM::removeLoopHintExecutionCounter(const JSInstruction* instruction)
{
    Locker locker { m_loopHintExecutionCountLock };
    auto iter = m_loopHintExecutionCounts.find(instruction);
    RELEASE_ASSERT(!!iter->value.first);
    --iter->value.first;
    if (!iter->value.first)
        m_loopHintExecutionCounts.remove(iter);
}

void VM::beginMarking()
{
    m_microtaskQueue.beginMarking();
}

template<typename Visitor>
void VM::visitAggregateImpl(Visitor& visitor)
{
    m_microtaskQueue.visitAggregate(visitor);
    numericStrings.visitAggregate(visitor);

    visitor.append(structureStructure);
    visitor.append(structureRareDataStructure);
    visitor.append(stringStructure);
    visitor.append(propertyNameEnumeratorStructure);
    visitor.append(getterSetterStructure);
    visitor.append(customGetterSetterStructure);
    visitor.append(domAttributeGetterSetterStructure);
    visitor.append(scopedArgumentsTableStructure);
    visitor.append(apiWrapperStructure);
    visitor.append(nativeExecutableStructure);
    visitor.append(evalExecutableStructure);
    visitor.append(programExecutableStructure);
    visitor.append(functionExecutableStructure);
#if ENABLE(WEBASSEMBLY)
    visitor.append(webAssemblyCalleeGroupStructure);
#endif
    visitor.append(moduleProgramExecutableStructure);
    visitor.append(regExpStructure);
    visitor.append(symbolStructure);
    visitor.append(symbolTableStructure);
    for (auto& structure : immutableButterflyStructures)
        visitor.append(structure);
    visitor.append(sourceCodeStructure);
    visitor.append(scriptFetcherStructure);
    visitor.append(scriptFetchParametersStructure);
    visitor.append(structureChainStructure);
    visitor.append(sparseArrayValueMapStructure);
    visitor.append(templateObjectDescriptorStructure);
    visitor.append(unlinkedFunctionExecutableStructure);
    visitor.append(unlinkedProgramCodeBlockStructure);
    visitor.append(unlinkedEvalCodeBlockStructure);
    visitor.append(unlinkedFunctionCodeBlockStructure);
    visitor.append(unlinkedModuleProgramCodeBlockStructure);
    visitor.append(propertyTableStructure);
    visitor.append(functionRareDataStructure);
    visitor.append(exceptionStructure);
    visitor.append(programCodeBlockStructure);
    visitor.append(moduleProgramCodeBlockStructure);
    visitor.append(evalCodeBlockStructure);
    visitor.append(functionCodeBlockStructure);
    visitor.append(hashMapBucketSetStructure);
    visitor.append(hashMapBucketMapStructure);
    visitor.append(bigIntStructure);

    visitor.append(m_emptyPropertyNameEnumerator);
    visitor.append(m_sentinelSetBucket);
    visitor.append(m_sentinelMapBucket);
    visitor.append(m_fastCanConstructBoundExecutable);
    visitor.append(m_slowCanConstructBoundExecutable);
    visitor.append(lastCachedString);
    visitor.append(heapBigIntConstantOne);
}
DEFINE_VISIT_AGGREGATE(VM);

void VM::addDebugger(Debugger& debugger)
{
    m_debuggers.append(&debugger);
}

void VM::removeDebugger(Debugger& debugger)
{
    m_debuggers.remove(&debugger);
}

void VM::performOpportunisticallyScheduledTasks(MonotonicTime deadline)
{
    JSLockHolder locker { *this };
    if (!deferredWorkTimer->hasAnyPendingWork())
        heap.sweeper().doWorkUntil(*this, deadline);
}

void QueuedTask::run()
{
    if (!m_job.isObject())
        return;
    JSObject* job = jsCast<JSObject*>(m_job);
    JSGlobalObject* globalObject = job->globalObject();
    runJSMicrotask(globalObject, m_identifier, job, m_arguments[0], m_arguments[1], m_arguments[2], m_arguments[3]);
}

template<typename Visitor>
void MicrotaskQueue::visitAggregateImpl(Visitor& visitor)
{
    // Because content in the queue will not be changed, we need to scan it only once per an entry during one GC cycle.
    // We record the previous scan's index, and restart scanning again in CollectorPhase::FixPoint from that.
    // When new GC phase begins, this cursor is reset to zero (beginMarking). This optimization is introduced because
    // some of application have massive size of MicrotaskQueue depth. For example, in parallel-promises-es2015-native.js
    // benchmark, it becomes 251670 at most.
    // This cursor is adjusted when an entry is dequeued. And we do not use any locking here, and that's fine: these
    // values are read by GC when CollectorPhase::FixPoint and CollectorPhase::Begin, and both suspend the mutator, thus,
    // there is no concurrency issue.
    for (auto iterator = m_queue.begin() + m_markedBefore, end = m_queue.end(); iterator != end; ++iterator) {
        auto& task = *iterator;
        visitor.appendUnbarriered(task.m_job);
        visitor.appendUnbarriered(task.m_arguments, QueuedTask::maxArguments);
    }
    m_markedBefore = m_queue.size();
}
DEFINE_VISIT_AGGREGATE(MicrotaskQueue);

void VM::ensureMegamorphicCacheSlow()
{
    ASSERT(!m_megamorphicCache);
    m_megamorphicCache = makeUnique<MegamorphicCache>();
}

void VM::invalidateStructureChainIntegrity(StructureChainIntegrityEvent)
{
    if (m_megamorphicCache)
        m_megamorphicCache->bumpEpoch();
}

#if ENABLE(WEBASSEMBLY)
void VM::registerWasmInstance(Wasm::Instance& instance)
{
    m_wasmInstances.add(instance);
}
#endif


VM::DrainMicrotaskDelayScope::DrainMicrotaskDelayScope(VM& vm)
    : m_vm(&vm)
{
    increment();
}

VM::DrainMicrotaskDelayScope::~DrainMicrotaskDelayScope()
{
    decrement();
}

VM::DrainMicrotaskDelayScope::DrainMicrotaskDelayScope(const VM::DrainMicrotaskDelayScope& other)
    : m_vm(other.m_vm)
{
    increment();
}

VM::DrainMicrotaskDelayScope& VM::DrainMicrotaskDelayScope::operator=(const VM::DrainMicrotaskDelayScope& other)
{
    if (this == &other)
        return *this;
    decrement();
    m_vm = other.m_vm;
    increment();
    return *this;
}

VM::DrainMicrotaskDelayScope& VM::DrainMicrotaskDelayScope::operator=(VM::DrainMicrotaskDelayScope&& other)
{
    decrement();
    m_vm = std::exchange(other.m_vm, nullptr);
    increment();
    return *this;
}

void VM::DrainMicrotaskDelayScope::increment()
{
    if (m_vm)
        ++m_vm->m_drainMicrotaskDelayScopeCount;
}

void VM::DrainMicrotaskDelayScope::decrement()
{
    if (!m_vm)
        return;
    ASSERT(m_vm->m_drainMicrotaskDelayScopeCount);
    if (!--m_vm->m_drainMicrotaskDelayScopeCount) {
        JSLockHolder locker(*m_vm);
        m_vm->drainMicrotasks();
    }
}

} // namespace JSC
