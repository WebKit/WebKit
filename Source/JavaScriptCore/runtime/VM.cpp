/*
 * Copyright (C) 2008-2025 Apple Inc. All rights reserved.
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
#include "CustomGetterSetterInlines.h"
#include "DOMAttributeGetterSetterInlines.h"
#include "Debugger.h"
#include "DeferredWorkTimer.h"
#include "Disassembler.h"
#include "DoublePredictionFuzzerAgent.h"
#include "ErrorInstance.h"
#include "EvalCodeBlockInlines.h"
#include "EvalExecutableInlines.h"
#include "Exception.h"
#include "FTLThunks.h"
#include "FileBasedFuzzerAgent.h"
#include "FunctionCodeBlockInlines.h"
#include "FunctionExecutableInlines.h"
#include "GetterSetterInlines.h"
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
#include "JSCellButterflyInlines.h"
#include "JSGlobalObject.h"
#include "JSIterator.h"
#include "JSLock.h"
#include "JSMap.h"
#include "JSMicrotask.h"
#include "JSPromise.h"
#include "JSPromiseCombinatorsContextInlines.h"
#include "JSPromiseCombinatorsGlobalContext.h"
#include "JSPromiseConstructor.h"
#include "JSPromiseReaction.h"
#include "JSPropertyNameEnumeratorInlines.h"
#include "JSScriptFetchParametersInlines.h"
#include "JSScriptFetcherInlines.h"
#include "JSSet.h"
#include "JSSourceCodeInlines.h"
#include "JSTemplateObjectDescriptorInlines.h"
#include "JSToWasm.h"
#include "LLIntData.h"
#include "LLIntExceptions.h"
#include "MarkedBlockInlines.h"
#include "MegamorphicCache.h"
#include "MicrotaskQueueInlines.h"
#include "MinimumReservedZoneSize.h"
#include "ModuleProgramCodeBlockInlines.h"
#include "ModuleProgramExecutableInlines.h"
#include "NarrowingNumberPredictionFuzzerAgent.h"
#include "NativeExecutable.h"
#include "NumberObject.h"
#include "PredictionFileCreatingFuzzerAgent.h"
#include "ProfilerDatabase.h"
#include "ProgramCodeBlockInlines.h"
#include "ProgramExecutableInlines.h"
#include "PropertyTableInlines.h"
#include "RandomizingFuzzerAgent.h"
#include "RegExpCache.h"
#include "RegExpInlines.h"
#include "ResourceExhaustion.h"
#include "SamplingProfiler.h"
#include "ScopedArguments.h"
#include "ShadowChicken.h"
#include "SharedJITStubSet.h"
#include "SideDataRepository.h"
#include "SimpleTypedArrayController.h"
#include "SourceProviderCache.h"
#include "StrongInlines.h"
#include "StructureChainInlines.h"
#include "StructureInlines.h"
#include "StructureStubInfo.h"
#include "SubspaceInlines.h"
#include "SymbolInlines.h"
#include "SymbolTableInlines.h"
#include "TestRunnerUtils.h"
#include "ThunkGenerators.h"
#include "TypeProfiler.h"
#include "TypeProfilerLog.h"
#include "UnlinkedEvalCodeBlockInlines.h"
#include "UnlinkedFunctionCodeBlockInlines.h"
#include "UnlinkedFunctionExecutableInlines.h"
#include "UnlinkedModuleProgramCodeBlockInlines.h"
#include "UnlinkedProgramCodeBlockInlines.h"
#include "VMEntryScopeInlines.h"
#include "VMInlines.h"
#include "VMManager.h"
#include "VariableEnvironment.h"
#include "WaiterListManager.h"
#include "WasmExecutionHandler.h"
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
#include <wtf/text/StringToIntegerConversion.h>

#if ENABLE(DFG_JIT)
#include "ConservativeRoots.h"
#endif

#if ENABLE(REGEXP_TRACING)
#include "RegExp.h"
#endif

#if ENABLE(WEBASSEMBLY)
#include "JSWebAssemblyInstance.h"
#endif

#if PLATFORM(COCOA)
#include <notify.h>
#include <wtf/darwin/DispatchExtras.h>
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

    auto canUseJITString = unsafeSpan(getenv("JavaScriptCoreUseJIT"));
    if (canUseJITString.data() && !parseInteger<int>(canUseJITString).value_or(0))
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

// This function is not meant to be called by anyone. It just provides a convenient scope
// that can is permitted to access private members of VM in order to do some needed
// static_asserts.
inline void VM::checkStaticAsserts()
{
    // VM registration is done in the instantiation of its VMThreadContext.
    //
    // VM registration with the VMManager can only be done after m_apiLock is initialized
    // because VMManager may trigger traps to stop the VM, and VMTraps the which uses
    // m_apiLock for the VMTraps::SignalSender uses m_apiLock.
    //
    // VM registration needs to be done before the heap is initialized because we may Global GC
    // may want to block the VM from doing any heap activity. In a Global GC world, we would be
    // binding this VM to the global heap instead of instantiating the heap field. We want to
    // be able to block before that point.
    static_assert(OBJECT_OFFSETOF(VM, m_apiLock) < OBJECT_OFFSETOF(VM, m_threadContext));
    static_assert(OBJECT_OFFSETOF(VM, m_threadContext) < OBJECT_OFFSETOF(VM, heap));
}

static bool vmCreationShouldCrash = false;

VM::VM(VMType vmType, HeapType heapType, WTF::RunLoop* runLoop, bool* success)
    : topCallFrame(CallFrame::noCaller())
    , m_identifier(VMIdentifier::generate())
    , m_apiLock(adoptRef(*new JSLock(this)))
    , m_runLoop(runLoop ? *runLoop : WTF::RunLoop::currentSingleton())
    , m_random(Options::seedOfVMRandomForFuzzer() ? Options::seedOfVMRandomForFuzzer() : cryptographicallyRandomNumber<uint32_t>())
    , m_heapRandom(Options::seedOfVMRandomForFuzzer() ? Options::seedOfVMRandomForFuzzer() : cryptographicallyRandomNumber<uint32_t>())
    , m_integrityRandom(*this)
    , heap(*this, heapType)
    , clientHeap(heap)
    , vmType(vmType)
    , deferredWorkTimer(DeferredWorkTimer::create(*this))
    , m_atomStringTable(vmType == VMType::Default ? Thread::currentSingleton().atomStringTable() : new AtomStringTable)
    , m_symbolRegistry(makeUniqueRef<SymbolRegistry>())
    , m_privateSymbolRegistry(makeUniqueRef<SymbolRegistry>(SymbolRegistry::Type::PrivateSymbol))
    , emptyList(new ArgList)
    , machineCodeBytesPerBytecodeWordForBaselineJIT(makeUnique<SimpleStats>())
    , symbolImplToSymbolMap(*this)
    , atomStringToJSStringMap(*this)
    , m_regExpCache(makeUnique<RegExpCache>())
    , m_compactVariableMap(adoptRef(*new CompactTDZEnvironmentMap))
    , m_codeCache(makeUnique<CodeCache>())
    , m_intlCache(makeUnique<IntlCache>())
    , m_builtinExecutables(makeUnique<BuiltinExecutables>(*this))
    , m_defaultMicrotaskQueue(*this)
    , m_syncWaiter(adoptRef(*new Waiter(this)))
{
    if (vmCreationShouldCrash || g_jscConfig.vmCreationDisallowed) [[unlikely]]
        CRASH_WITH_EXTRA_SECURITY_IMPLICATION_AND_INFO(VMCreationDisallowed, "VM creation disallowed"_s, 0x4242424220202020, 0xbadbeef0badbeef, 0x1234123412341234, 0x1337133713371337);

    // Set up lazy initializers.
    {
        m_hasOwnPropertyCache.initLater([](VM&, auto& ref) {
            ref.set(HasOwnPropertyCache::create());
        });

        m_megamorphicCache.initLater([](VM&, auto& ref) {
            ref.set(makeUniqueRef<MegamorphicCache>());
        });

        m_shadowChicken.initLater([](VM&, auto& ref) {
            ref.set(makeUniqueRef<ShadowChicken>());
        });

        m_heapProfiler.initLater([](VM& vm, auto& ref) {
            ref.set(makeUniqueRef<HeapProfiler>(vm));
        });

        m_stringSearcherTables.initLater([](VM&, auto& ref) {
            ref.set(makeUniqueRef<AdaptiveStringSearcherTables>());
        });

        m_watchdog.initLater([](VM& vm, auto& ref) {
            ref.set(adoptRef(*new Watchdog(&vm)));
            vm.ensureTerminationException();
            vm.requestEntryScopeService(EntryScopeService::Watchdog);
        });
    }

    updateSoftReservedZoneSize(Options::softReservedZoneSize());
    setLastStackTop(Thread::currentSingleton());
    stringSplitIndice.reserveInitialCapacity(256);

    JSRunLoopTimer::Manager::singleton().registerVM(*this);

    // Need to be careful to keep everything consistent here
    JSLockHolder lock(this);
    AtomStringTable* existingEntryAtomStringTable = Thread::currentSingleton().setCurrentAtomStringTable(m_atomStringTable);
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
    promiseReactionStructure.setWithoutWriteBarrier(JSPromiseReaction::createStructure(*this, nullptr, jsNull()));
    promiseCombinatorsContextStructure.setWithoutWriteBarrier(JSPromiseCombinatorsContext::createStructure(*this, nullptr, jsNull()));
    promiseCombinatorsGlobalContextStructure.setWithoutWriteBarrier(JSPromiseCombinatorsGlobalContext::createStructure(*this, nullptr, jsNull()));
    regExpStructure.setWithoutWriteBarrier(RegExp::createStructure(*this, nullptr, jsNull()));
    symbolStructure.setWithoutWriteBarrier(Symbol::createStructure(*this, nullptr, jsNull()));
    symbolTableStructure.setWithoutWriteBarrier(SymbolTable::createStructure(*this, nullptr, jsNull()));

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    rawImmutableButterflyStructure(CopyOnWriteArrayWithInt32).setWithoutWriteBarrier(JSCellButterfly::createStructure(*this, nullptr, jsNull(), CopyOnWriteArrayWithInt32));
    Structure* copyOnWriteArrayWithContiguousStructure = JSCellButterfly::createStructure(*this, nullptr, jsNull(), CopyOnWriteArrayWithContiguous);
    rawImmutableButterflyStructure(CopyOnWriteArrayWithDouble).setWithoutWriteBarrier(Options::allowDoubleShape() ? JSCellButterfly::createStructure(*this, nullptr, jsNull(), CopyOnWriteArrayWithDouble) : copyOnWriteArrayWithContiguousStructure);
    rawImmutableButterflyStructure(CopyOnWriteArrayWithContiguous).setWithoutWriteBarrier(copyOnWriteArrayWithContiguousStructure);
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

    // This is only for JSCellButterfly filled with atom strings.
    cellButterflyOnlyAtomStringsStructure.setWithoutWriteBarrier(JSCellButterfly::createStructure(*this, nullptr, jsNull(), CopyOnWriteArrayWithContiguous));

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
    bigIntStructure.setWithoutWriteBarrier(JSBigInt::createStructure(*this, nullptr, jsNull()));
    m_orderedHashTableDeletedValue.setWithoutWriteBarrier(OrderedHashMap::createDeletedValue(*this));
    m_orderedHashTableSentinel.setWithoutWriteBarrier(OrderedHashMap::createSentinel(*this));

    // Eagerly initialize constant cells since the concurrent compiler can access them.
    if (Options::useJIT()) {
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

    Thread::currentSingleton().setCurrentAtomStringTable(existingEntryAtomStringTable);
    
    Gigacage::addPrimitiveDisableCallback(primitiveGigacageDisabledCallback, this);

    heap.notifyIsSafeToCollect();
    
    if (Options::useProfiler()) [[unlikely]] {
        m_perBytecodeProfiler = makeUnique<Profiler::Database>(*this);

        StringPrintStream pathOut;
        const char* profilerPath = getenv("JSC_PROFILER_PATH");
        if (profilerPath)
            pathOut.print(profilerPath, "/");
        pathOut.print("JSCProfile-", getCurrentProcessID(), "-", m_perBytecodeProfiler->databaseID(), ".json");
        static NeverDestroyed<CString> pathOutString = pathOut.toCString();

#if PLATFORM(COCOA)
        static std::once_flag registerFlag;
        std::call_once(registerFlag, [this]() {
            int pid = getpid();
            const char* key = "com.apple.WebKit.bytecode.profiler";
            dataLogF("<BYTECODE.STAT><%d> Registering callback for dumping profiles, dumping to %s.\n", pid, pathOutString->data());
            dataLogF("<BYTECODE.STAT><%d> Use `notifyutil -v -p %s` to dump statistics.\n", pid, key);

            int token;
            notify_register_dispatch(key, &token, mainDispatchQueueSingleton(), ^(int) {
                dataLogF("<BYTECODE.STAT><%d> Dumping\n", pid);
                if (!m_perBytecodeProfiler->save(pathOutString->data()))
                    dataLogF("<BYTECODE.STAT><%d> Failed to dump to %s. Do you need to add a sandbox extension? ((allow file-write* (subpath \"/private/tmp/\")) in WebProcess.sb.in\n", pid, pathOutString->data());
                else
                    dataLogF("<BYTECODE.STAT><%d> Dumped to %s\n", pid, pathOutString->data());
            });
        });
#endif

        if (Options::dumpProfilerDataAtExit()) [[unlikely]]
            m_perBytecodeProfiler->registerToSaveAtExit(pathOutString->data());
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
        ensureSamplingProfiler(WTF::move(stopwatch));
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
        Ref watchdog = ensureWatchdog();
        watchdog->setTimeLimit(Seconds::fromMilliseconds(Options::watchdog()));
    }

    if (Options::useTracePoints())
        requestEntryScopeService(EntryScopeService::TracePoints);

#if ENABLE(WEBASSEMBLY_DEBUGGER)
    if (Options::enableWasmDebugger()) [[unlikely]]
        m_debugState = makeUnique<Wasm::DebugState>();
#endif

#if ENABLE(JIT)
    // Make sure that any stubs that the JIT is going to use are initialized in non-compilation threads.
    if (Options::useJIT()) {
        jitStubs = makeUnique<JITThunks>();
        jitStubs->initialize(*this);
#if ENABLE(FTL_JIT)
        ftlThunks = makeUnique<FTL::Thunks>();
#endif // ENABLE(FTL_JIT)
        m_sharedJITStubs = makeUnique<SharedJITStubSet>();
        getBoundFunction(/* isJSFunction */ true, SourceTaintedOrigin::Untainted);
    }
#endif // ENABLE(JIT)

    if (Options::forceDebuggerBytecodeGeneration() || Options::alwaysUseShadowChicken())
        ensureShadowChicken();

#if ENABLE(JIT)
    if (Options::dumpBaselineJITSizeStatistics() || Options::dumpDFGJITSizeStatistics())
        jitSizeStatistics = makeUnique<JITSizeStatistics>();
#endif

    Config::finalize();

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

    if (vmType == VMType::Default)
        WaiterListManager::singleton().unregister(this);

    Gigacage::removePrimitiveDisableCallback(primitiveGigacageDisabledCallback, this);
    deferredWorkTimer->stopRunningTasks();
#if ENABLE(WEBASSEMBLY)
    if (Wasm::Worklist* worklist = Wasm::existingWorklistOrNull())
        worklist->stopAllPlansForContext(*this);
#endif
    if (RefPtr watchdog = this->watchdog(); watchdog) [[unlikely]]
        watchdog->willDestroyVM(this);
    traps().willDestroyVM();
    m_isInService = false;
    WTF::storeStoreFence();

    if (m_hasSideData)
        sideDataRepository().deleteAll(this);

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
    
    // Clear this first to ensure that nobody tries to remove themselves from it.
    m_perBytecodeProfiler = nullptr;

    ASSERT(currentThreadIsHoldingAPILock());
    m_apiLock->willDestroyVM(this);
    smallStrings.setIsInitialized(false);
    heap.lastChanceToFinalize();

    while (!m_microtaskQueues.isEmpty())
        m_microtaskQueues.begin()->remove();

    JSRunLoopTimer::Manager::singleton().unregisterVM(*this);

    delete emptyList;

    delete propertyNames;
    if (vmType != VMType::Default)
        delete m_atomStringTable;

    delete clientData;
    m_regExpCache.reset();

#if ENABLE(DFG_JIT)
    for (unsigned i = 0; i < m_scratchBuffers.size(); ++i)
        VMMalloc::free(m_scratchBuffers[i]);
#endif

#if ENABLE(JIT)
    m_sharedJITStubs = nullptr;
#endif

#if ENABLE(WEBASSEMBLY_DEBUGGER)
    if (Options::enableWasmDebugger()) [[unlikely]] {
        auto& debugServer = Wasm::DebugServer::singleton();
        if (debugServer.isConnected())
            debugServer.execution().notifyVMDestruction(this);
    }
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
    return adoptRef(*new VM(VMType::APIContextGroup, heapType));
}

Ref<VM> VM::create(HeapType heapType, WTF::RunLoop* runLoop)
{
    return adoptRef(*new VM(VMType::Default, heapType, runLoop));
}

RefPtr<VM> VM::tryCreate(HeapType heapType, WTF::RunLoop* runLoop)
{
    bool success = true;
    RefPtr<VM> vm = adoptRef(new VM(VMType::Default, heapType, runLoop, &success));
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

#if ENABLE(SAMPLING_PROFILER)
SamplingProfiler& VM::ensureSamplingProfiler(Ref<Stopwatch>&& stopwatch)
{
    if (!m_samplingProfiler) {
        lazyInitialize(m_samplingProfiler, adoptRef(*new SamplingProfiler(*this, WTF::move(stopwatch))));
        requestEntryScopeService(EntryScopeService::SamplingProfiler);
    }
    return *m_samplingProfiler;
}

void VM::enableSamplingProfiler()
{
    RefPtr profiler = samplingProfiler();
    if (!profiler)
        profiler = &ensureSamplingProfiler(Stopwatch::create());
    profiler->start();
}

void VM::disableSamplingProfiler()
{
    RefPtr profiler = samplingProfiler();
    if (!profiler)
        profiler = &ensureSamplingProfiler(Stopwatch::create());
    {
        Locker locker { profiler->getLock() };
        profiler->pause();
    }
}

RefPtr<JSON::Value> VM::takeSamplingProfilerSamplesAsJSON()
{
    RefPtr profiler = samplingProfiler();
    return profiler ? RefPtr { profiler->stackTracesAsJSON() } : nullptr;
}

#endif // ENABLE(SAMPLING_PROFILER)

static StringImpl::StaticStringImpl terminationErrorString { "JavaScript execution terminated." };
Exception* VM::ensureTerminationException()
{
    if (!m_terminationException) {
        JSString* terminationError = jsNontrivialString(*this, terminationErrorString);
        m_terminationException = Exception::create(*this, terminationError, Exception::StackCaptureAction::DoNotCaptureStack);
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
    case StringPrototypeAtIntrinsic:
        return stringAtThunkGenerator;
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
    case GlobalIsFiniteIntrinsic:
        return globalIsFiniteThunkGenerator;
    case NumberIsFiniteIntrinsic:
        return numberIsFiniteThunkGenerator;
    case NumberIsSafeIntegerIntrinsic:
        return numberIsSafeIntegerThunkGenerator;
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
#if USE(JSVALUE64)
    case ObjectIsIntrinsic:
        return objectIsThunkGenerator;
#endif
    case BoundFunctionCallIntrinsic:
        return boundFunctionCallGenerator;
    case RemoteFunctionCallIntrinsic:
        return remoteFunctionCallGenerator;
    case NumberConstructorIntrinsic:
        return numberConstructorCallThunkGenerator;
    case StringConstructorIntrinsic:
        return stringConstructorCallThunkGenerator;
    case ToIntegerOrInfinityIntrinsic:
        return toIntegerOrInfinityThunkGenerator;
    case ToLengthIntrinsic:
        return toLengthThunkGenerator;
    case WasmFunctionIntrinsic:
#if ENABLE(WEBASSEMBLY) && ENABLE(JIT)
        return Wasm::wasmFunctionThunkGenerator;
#else
        return nullptr;
#endif
    default:
        return nullptr;
    }
}

MacroAssemblerCodeRef<JITThunkPtrTag> VM::getCTIStub(ThunkGenerator generator)
{
    return jitStubs->ctiStub(*this, generator);
}

MacroAssemblerCodeRef<JITThunkPtrTag> VM::getCTIStub(CommonJITThunkID thunkID)
{
    return jitStubs->ctiStub(thunkID);
}

#endif // ENABLE(JIT)

NativeExecutable* VM::getHostFunction(NativeFunction function, ImplementationVisibility implementationVisibility, NativeFunction constructor, const String& name)
{
    return getHostFunction(function, implementationVisibility, NoIntrinsic, constructor, nullptr, name);
}

static Ref<NativeJITCode> jitCodeForCallTrampoline(Intrinsic intrinsic)
{
    switch (intrinsic) {
#if ENABLE(WEBASSEMBLY)
    case WasmFunctionIntrinsic: {
        static LazyNeverDestroyed<Ref<NativeJITCode>> result;
        static std::once_flag onceKey;
        std::call_once(onceKey, [&] {
            result.construct(adoptRef(*new NativeJITCode(LLInt::getCodeRef<JSEntryPtrTag>(js_to_wasm_wrapper_entry), JITType::HostCallThunk, intrinsic)));
        });
        return result.get();
    }
#endif
    default: {
        static LazyNeverDestroyed<Ref<NativeJITCode>> result;
        static std::once_flag onceKey;
        std::call_once(onceKey, [&] {
            result.construct(adoptRef(*new NativeJITCode(LLInt::getCodeRef<JSEntryPtrTag>(llint_native_call_trampoline), JITType::HostCallThunk, NoIntrinsic)));
        });
        return result.get();
    }
    }
}

static Ref<NativeJITCode> jitCodeForConstructTrampoline()
{
    static LazyNeverDestroyed<Ref<NativeJITCode>> result;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        result.construct(adoptRef(*new NativeJITCode(LLInt::getCodeRef<JSEntryPtrTag>(llint_native_construct_trampoline), JITType::HostCallThunk, NoIntrinsic)));
    });
    return result.get();
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
    return NativeExecutable::create(*this, jitCodeForCallTrampoline(intrinsic), toTagged(function), jitCodeForConstructTrampoline(), toTagged(constructor), implementationVisibility, name);
}

NativeExecutable* VM::getBoundFunction(bool isJSFunction, SourceTaintedOrigin taintedness)
{
    bool slowCase = !isJSFunction;

    auto getOrCreate = [&](WriteBarrier<NativeExecutable>& slot) -> NativeExecutable* {
        if (taintedness < SourceTaintedOrigin::IndirectlyTainted) {
            if (auto* cached = slot.get())
                return cached;
        }
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
        if (!slowCase)
            intrinsic = RemoteFunctionCallIntrinsic;

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
        if (kind == CodeSpecializationKind::CodeForCall)
            return jitStubs->ctiInternalFunctionCall(*this).retagged<JSEntryPtrTag>();
        return jitStubs->ctiInternalFunctionConstruct(*this).retagged<JSEntryPtrTag>();
    }
#endif
    if (kind == CodeSpecializationKind::CodeForCall)
        return LLInt::getCodePtr<JSEntryPtrTag>(llint_internal_function_call_trampoline);
    return LLInt::getCodePtr<JSEntryPtrTag>(llint_internal_function_construct_trampoline);
}

MacroAssemblerCodeRef<JSEntryPtrTag> VM::getCTIThrowExceptionFromCallSlowPath()
{
#if ENABLE(JIT)
    if (Options::useJIT())
        return getCTIStub(CommonJITThunkID::ThrowExceptionFromCallSlowPath).template retagged<JSEntryPtrTag>();
#endif
    return LLInt::callToThrow(*this).template retagged<JSEntryPtrTag>();
}

MacroAssemblerCodeRef<JITStubRoutinePtrTag> VM::getCTIVirtualCall(CallMode callMode)
{
#if ENABLE(JIT)
    if (Options::useJIT()) {
        switch (callMode) {
        case CallMode::Regular:
            return getCTIStub(CommonJITThunkID::VirtualThunkForRegularCall).template retagged<JITStubRoutinePtrTag>();
        case CallMode::Tail:
            return getCTIStub(CommonJITThunkID::VirtualThunkForTailCall).template retagged<JITStubRoutinePtrTag>();
        case CallMode::Construct:
            return getCTIStub(CommonJITThunkID::VirtualThunkForConstruct).template retagged<JITStubRoutinePtrTag>();
        }
        RELEASE_ASSERT_NOT_REACHED();
    }
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
    m_didPopListeners.append(WTF::move(callback));
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
        m_builtinExecutables->clear();
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
    if (traps().needHandling(VMTraps::NonDebuggerAsyncEvents)) [[unlikely]]
        traps().handleTraps(VMTraps::NonDebuggerAsyncEvents);
    return exception();
}

void VM::setHasTerminationRequest()
{
    m_hasTerminationRequest = true;
    requestEntryScopeService(ConcurrentEntryScopeService::ResetTerminationRequest);
}

void VM::setException(Exception* exception)
{
    ASSERT(!isTerminationException(exception) || hasTerminationRequest());
    m_exception = exception;
    m_lastException = exception;
    if (exception)
        traps().fireTrap(VMTraps::NeedExceptionHandling);
}

void VM::throwTerminationException()
{
    ASSERT(hasTerminationRequest());
    ASSERT(!traps().isDeferringTermination());
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
    if (Options::breakOnThrow()) [[unlikely]] {
        CodeBlock* codeBlock = throwOriginFrame && !throwOriginFrame->isNativeCalleeFrame() ? throwOriginFrame->codeBlock() : nullptr;
        dataLog("Throwing exception in call frame ", RawPointer(throwOriginFrame), " for code block ", codeBlock, "\n");
        WTFBreakpointTrap();
    }

    interpreter.notifyDebuggerOfExceptionToBeThrown(*this, globalObject, throwOriginFrame, exceptionToThrow);

    setException(exceptionToThrow);

#if ENABLE(EXCEPTION_SCOPE_VERIFICATION)
    m_nativeStackTraceOfLastThrow = StackTrace::captureStackTrace(Options::unexpectedExceptionStackTraceLimit());
    m_throwingThread = &Thread::currentSingleton();
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
    cloopStack().setSoftReservedZoneSize(softReservedZoneSize);
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
    void* lastSoftStackLimit = traps().softStackLimit();

    const StackBounds& stack = Thread::currentSingleton().stack();
    size_t reservedZoneSize = Options::reservedZoneSize();
    // We should have already ensured that Options::reservedZoneSize() >= minimumReserveZoneSize at
    // options initialization time, and the option value should not have been changed thereafter.
    // We don't have the ability to assert here that it hasn't changed, but we can at least assert
    // that the value is sane.
    RELEASE_ASSERT(reservedZoneSize >= minimumReservedZoneSize);

    void* newSoftStackLimit = 0;
    if (m_stackPointerAtVMEntry) {
        char* startOfStack = reinterpret_cast<char*>(m_stackPointerAtVMEntry);
        newSoftStackLimit = stack.recursionLimit(startOfStack, Options::maxPerThreadStackUsage(), m_currentSoftReservedZoneSize);
        m_stackLimit = stack.recursionLimit(startOfStack, Options::maxPerThreadStackUsage(), reservedZoneSize);
    } else {
        newSoftStackLimit = stack.recursionLimit(m_currentSoftReservedZoneSize);
        m_stackLimit = stack.recursionLimit(reservedZoneSize);
    }

    if (lastSoftStackLimit != newSoftStackLimit) {
        traps().setStackSoftLimit(newSoftStackLimit);
#if OS(WINDOWS)
        // We only need to precommit stack memory dictated by the VM::softStackLimit() limit.
        // This is because VM::softStackLimit() applies to stack usage by LLINT asm or JIT
        // generated code which can allocate stack space that the C++ compiler does not know
        // about. As such, we have to precommit that stack memory manually.
        //
        // In contrast, we do not need to worry about VM::m_stackLimit because that limit is
        // used exclusively by C++ code, and the C++ compiler will automatically commit the
        // needed stack pages.
        preCommitStackMemory(newSoftStackLimit);
#endif
    }
}

#if ENABLE(DFG_JIT)
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

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

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
#endif // ENABLE(DFG_JIT)

void VM::pushCheckpointOSRSideState(std::unique_ptr<CheckpointOSRExitSideState>&& payload)
{
    ASSERT(currentThreadIsHoldingAPILock());
    ASSERT(payload->associatedCallFrame);
#if ASSERT_ENABLED
    for (const auto& sideState : m_checkpointSideState)
        ASSERT(sideState->associatedCallFrame != payload->associatedCallFrame);
#endif
    m_checkpointSideState.append(WTF::move(payload));

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
    if (Options::verboseSanitizeStack()) [[unlikely]] {
        auto& stackBounds = Thread::currentSingleton().stack();
        dataLogLn("Sanitizing stack for VM = ", RawPointer(&vm), ", current stack pointer at ", RawPointer(currentStackPointer()), ", last stack top = ", RawPointer(vm.lastStackTop()), ", in stack range (", RawPointer(stackBounds.end()), ", ", RawPointer(stackBounds.origin()), "]");
    }
}

#if ENABLE(REGEXP_TRACING)

void VM::addRegExpToTrace(RegExp* regExp)
{
    gcProtect(regExp);
    m_rtTraceList.add(regExp);
}

void VM::dumpRegExpTrace()
{
    if (m_rtTraceList.size() <= 1)
        return;

    // The first RegExp object is ignored. It is created by the RegExpPrototype ctor and not used.
    RTTraceList::iterator iter = ++m_rtTraceList.begin();
    
    if (iter != m_rtTraceList.end()) {
        RegExp::printTraceHeader();

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
    m_defaultMicrotaskQueue.enqueue(WTF::move(task));
}

void VM::callPromiseRejectionCallback(Strong<JSPromise>& promise)
{
    JSObject* callback = promise->globalObject()->unhandledRejectionCallback();
    if (!callback)
        return;

    auto scope = DECLARE_TOP_EXCEPTION_SCOPE(*this);

    auto callData = JSC::getCallDataInline(callback);
    ASSERT(callData.type != CallData::Type::None);

    MarkedArgumentBuffer args;
    args.append(promise.get());
    args.append(promise->result());
    ASSERT(!args.hasOverflowed());
    call(promise->globalObject(), callback, callData, jsNull(), args);
    scope.clearException();
}

void VM::didExhaustMicrotaskQueue()
{
    do {
        auto unhandledRejections = WTF::move(m_aboutToBeNotifiedRejectedPromises);
        for (auto& promise : unhandledRejections) {
            if (promise->isHandled())
                continue;

            callPromiseRejectionCallback(promise);
            if (hasPendingTerminationException()) [[unlikely]]
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
    if (m_drainMicrotaskDelayScopeCount) [[unlikely]]
        return;

    if (executionForbidden()) [[unlikely]]
        m_defaultMicrotaskQueue.clear();
    else {
        std::optional<VMEntryScope> entryScope;
        if (!m_defaultMicrotaskQueue.isEmpty())
            entryScope.emplace(*this, nullptr);
        while (true) {
            m_defaultMicrotaskQueue.performMicrotaskCheckpoint</* useCallOnEachMicrotask */ true>(*this,
                [&](QueuedTask& task) ALWAYS_INLINE_LAMBDA {
                    auto* globalObject = task.globalObject();
                    entryScope->setGlobalObject(globalObject);
                    if (RefPtr dispatcher = task.dispatcher())
                        return dispatcher->run(task);

                    auto catchScope = DECLARE_TOP_EXCEPTION_SCOPE(*this);
                    runInternalMicrotask(globalObject, task.job(), task.payload(), task.arguments());
                    catchScope.clearExceptionExceptTermination();
                    return QueuedTask::Result::Executed;
                });
            if (hasPendingTerminationException()) [[unlikely]]
                return;
            didExhaustMicrotaskQueue();
            if (hasPendingTerminationException()) [[unlikely]]
                return;
            if (m_defaultMicrotaskQueue.isEmpty())
                break;
            if (!entryScope)
                entryScope.emplace(*this, nullptr);
        }
    }
    finalizeSynchronousJSExecution();
}

void sanitizeStackForVM(VM& vm)
{
    auto& thread = Thread::currentSingleton();
    auto& stack = thread.stack();
    if (!vm.currentThreadIsHoldingAPILock())
        return; // vm.lastStackTop() may not be set up correctly if JSLock is not held.

    logSanitizeStack(vm);

    RELEASE_ASSERT(stack.contains(vm.lastStackTop()), 0xaa10, vm.lastStackTop(), stack.origin(), stack.end());
#if ENABLE(C_LOOP)
    vm.cloopStack().sanitizeStack();
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
    uint8_t* current = std::bit_cast<uint8_t*>(currentStackPointer());
    uint8_t* high = std::bit_cast<uint8_t*>(Thread::currentSingleton().stack().origin());
    return high - current;
#else
    return CLoopStack::committedByteCount();
#endif
}

#if ENABLE(EXCEPTION_SCOPE_VERIFICATION)
void VM::verifyExceptionCheckNeedIsSatisfied(unsigned recursionDepth, ExceptionEventLocation& location)
{
    if (!Options::validateExceptionChecks())
        return;

    if (m_needExceptionCheck) [[unlikely]] {
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
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("exception check validation failed");
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
    return m_syncWaiter;
}

JSValue VM::checkVMEntryPermission()
{
    if (Options::crashOnDisallowedVMEntry() || g_jscConfig.vmEntryDisallowed)
        CRASH_WITH_EXTRA_SECURITY_IMPLICATION_AND_INFO(VMEntryDisallowed, "VM entry disallowed"_s);
    return jsUndefined();
}

JSPropertyNameEnumerator* VM::emptyPropertyNameEnumeratorSlow()
{
    ASSERT(!m_emptyPropertyNameEnumerator);
    PropertyNameArrayBuilder propertyNames(*this, PropertyNameMode::Strings, PrivateSymbolMode::Exclude);
    auto* enumerator = JSPropertyNameEnumerator::create(*this, nullptr, 0, 0, WTF::move(propertyNames));
    m_emptyPropertyNameEnumerator.setWithoutWriteBarrier(enumerator);
    return enumerator;
}

NativeExecutable* VM::promiseResolvingFunctionResolveExecutableSlow()
{
    ASSERT(!m_promiseResolvingFunctionResolveExecutable);
    auto* executable = getHostFunction(promiseResolvingFunctionResolve, ImplementationVisibility::Public, callHostFunctionAsConstructor, emptyString());
    m_promiseResolvingFunctionResolveExecutable.setWithoutWriteBarrier(executable);
    return executable;
}

NativeExecutable* VM::promiseResolvingFunctionRejectExecutableSlow()
{
    ASSERT(!m_promiseResolvingFunctionRejectExecutable);
    auto* executable = getHostFunction(promiseResolvingFunctionReject, ImplementationVisibility::Public, callHostFunctionAsConstructor, emptyString());
    m_promiseResolvingFunctionRejectExecutable.setWithoutWriteBarrier(executable);
    return executable;
}

NativeExecutable* VM::promiseFirstResolvingFunctionResolveExecutableSlow()
{
    ASSERT(!m_promiseFirstResolvingFunctionResolveExecutable);
    auto* executable = getHostFunction(promiseFirstResolvingFunctionResolve, ImplementationVisibility::Public, callHostFunctionAsConstructor, emptyString());
    m_promiseFirstResolvingFunctionResolveExecutable.setWithoutWriteBarrier(executable);
    return executable;
}

NativeExecutable* VM::promiseFirstResolvingFunctionRejectExecutableSlow()
{
    ASSERT(!m_promiseFirstResolvingFunctionRejectExecutable);
    auto* executable = getHostFunction(promiseFirstResolvingFunctionReject, ImplementationVisibility::Public, callHostFunctionAsConstructor, emptyString());
    m_promiseFirstResolvingFunctionRejectExecutable.setWithoutWriteBarrier(executable);
    return executable;
}

NativeExecutable* VM::promiseResolvingFunctionResolveWithInternalMicrotaskExecutableSlow()
{
    ASSERT(!m_promiseResolvingFunctionResolveWithInternalMicrotaskExecutable);
    auto* executable = getHostFunction(promiseResolvingFunctionResolveWithInternalMicrotask, ImplementationVisibility::Public, callHostFunctionAsConstructor, emptyString());
    m_promiseResolvingFunctionResolveWithInternalMicrotaskExecutable.setWithoutWriteBarrier(executable);
    return executable;
}

NativeExecutable* VM::promiseResolvingFunctionRejectWithInternalMicrotaskExecutableSlow()
{
    ASSERT(!m_promiseResolvingFunctionRejectWithInternalMicrotaskExecutable);
    auto* executable = getHostFunction(promiseResolvingFunctionRejectWithInternalMicrotask, ImplementationVisibility::Public, callHostFunctionAsConstructor, emptyString());
    m_promiseResolvingFunctionRejectWithInternalMicrotaskExecutable.setWithoutWriteBarrier(executable);
    return executable;
}

NativeExecutable* VM::promiseCapabilityExecutorExecutableSlow()
{
    ASSERT(!m_promiseCapabilityExecutorExecutable);
    auto* executable = getHostFunction(promiseCapabilityExecutor, ImplementationVisibility::Public, callHostFunctionAsConstructor, emptyString());
    m_promiseCapabilityExecutorExecutable.setWithoutWriteBarrier(executable);
    return executable;
}

NativeExecutable* VM::promiseAllFulfillFunctionExecutableSlow()
{
    ASSERT(!m_promiseAllFulfillFunctionExecutable);
    auto* executable = getHostFunction(promiseAllFulfillFunction, ImplementationVisibility::Public, callHostFunctionAsConstructor, emptyString());
    m_promiseAllFulfillFunctionExecutable.setWithoutWriteBarrier(executable);
    return executable;
}

NativeExecutable* VM::promiseAllSlowFulfillFunctionExecutableSlow()
{
    ASSERT(!m_promiseAllSlowFulfillFunctionExecutable);
    auto* executable = getHostFunction(promiseAllSlowFulfillFunction, ImplementationVisibility::Public, callHostFunctionAsConstructor, emptyString());
    m_promiseAllSlowFulfillFunctionExecutable.setWithoutWriteBarrier(executable);
    return executable;
}

NativeExecutable* VM::promiseAllSettledFulfillFunctionExecutableSlow()
{
    ASSERT(!m_promiseAllSettledFulfillFunctionExecutable);
    auto* executable = getHostFunction(promiseAllSettledFulfillFunction, ImplementationVisibility::Public, callHostFunctionAsConstructor, emptyString());
    m_promiseAllSettledFulfillFunctionExecutable.setWithoutWriteBarrier(executable);
    return executable;
}

NativeExecutable* VM::promiseAllSettledRejectFunctionExecutableSlow()
{
    ASSERT(!m_promiseAllSettledRejectFunctionExecutable);
    auto* executable = getHostFunction(promiseAllSettledRejectFunction, ImplementationVisibility::Public, callHostFunctionAsConstructor, emptyString());
    m_promiseAllSettledRejectFunctionExecutable.setWithoutWriteBarrier(executable);
    return executable;
}

NativeExecutable* VM::promiseAllSettledSlowFulfillFunctionExecutableSlow()
{
    ASSERT(!m_promiseAllSettledSlowFulfillFunctionExecutable);
    auto* executable = getHostFunction(promiseAllSettledSlowFulfillFunction, ImplementationVisibility::Public, callHostFunctionAsConstructor, emptyString());
    m_promiseAllSettledSlowFulfillFunctionExecutable.setWithoutWriteBarrier(executable);
    return executable;
}

NativeExecutable* VM::promiseAllSettledSlowRejectFunctionExecutableSlow()
{
    ASSERT(!m_promiseAllSettledSlowRejectFunctionExecutable);
    auto* executable = getHostFunction(promiseAllSettledSlowRejectFunction, ImplementationVisibility::Public, callHostFunctionAsConstructor, emptyString());
    m_promiseAllSettledSlowRejectFunctionExecutable.setWithoutWriteBarrier(executable);
    return executable;
}

NativeExecutable* VM::promiseAnyRejectFunctionExecutableSlow()
{
    ASSERT(!m_promiseAnyRejectFunctionExecutable);
    auto* executable = getHostFunction(promiseAnyRejectFunction, ImplementationVisibility::Public, callHostFunctionAsConstructor, emptyString());
    m_promiseAnyRejectFunctionExecutable.setWithoutWriteBarrier(executable);
    return executable;
}

NativeExecutable* VM::promiseAnySlowRejectFunctionExecutableSlow()
{
    ASSERT(!m_promiseAnySlowRejectFunctionExecutable);
    auto* executable = getHostFunction(promiseAnySlowRejectFunction, ImplementationVisibility::Public, callHostFunctionAsConstructor, emptyString());
    m_promiseAnySlowRejectFunctionExecutable.setWithoutWriteBarrier(executable);
    return executable;
}

void VM::executeEntryScopeServicesOnEntry()
{
    if (hasEntryScopeServiceRequest(EntryScopeService::FirePrimitiveGigacageEnabled)) [[unlikely]] {
        m_primitiveGigacageEnabled.fireAll(*this, "Primitive gigacage disabled asynchronously");
        clearEntryScopeService(EntryScopeService::FirePrimitiveGigacageEnabled);
    }

    // Reset the date cache between JS invocations to force the VM to
    // observe time zone changes.
    dateCache.resetIfNecessary();

    RefPtr watchdog = this->watchdog();
    if (watchdog) [[unlikely]]
        watchdog->enteredVM();

#if ENABLE(SAMPLING_PROFILER)
    RefPtr samplingProfiler = this->samplingProfiler();
    if (samplingProfiler) [[unlikely]]
        samplingProfiler->noticeVMEntry();
#endif

    if (Options::useTracePoints()) [[unlikely]]
        tracePoint(VMEntryScopeStart);

    if (hasEntryScopeServiceRequest(ConcurrentEntryScopeService::NeedStopTheWorld)) [[unlikely]]
        VMManager::singleton().notifyVMActivation(*this);
}

void VM::executeEntryScopeServicesOnExit()
{
    if (hasEntryScopeServiceRequest(ConcurrentEntryScopeService::NeedStopTheWorld)) [[unlikely]]
        VMManager::singleton().notifyVMDeactivation(*this);

    if (Options::useTracePoints()) [[unlikely]]
        tracePoint(VMEntryScopeEnd);

    RefPtr watchdog = this->watchdog();
    if (watchdog) [[unlikely]]
        watchdog->exitedVM();

    if (hasEntryScopeServiceRequest(EntryScopeService::PopListeners)) {
        auto listeners = WTF::move(m_didPopListeners);
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
        addResult.iterator->value.second = WTF::move(ptr);
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
    m_microtaskQueues.forEach([&](MicrotaskQueue* microtaskQueue) {
        microtaskQueue->beginMarking();
    });
}

template<typename Visitor>
void VM::visitAggregateImpl(Visitor& visitor)
{
    m_microtaskQueues.forEach([&](MicrotaskQueue* microtaskQueue) {
        microtaskQueue->visitAggregate(visitor);
    });
    numericStrings.visitAggregate(visitor);
    m_builtinExecutables->visitAggregate(visitor);
    m_regExpCache->visitAggregate(visitor);

    if (heap.collectionScope() != CollectionScope::Full)
        stringReplaceCache.visitAggregate(visitor);

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
    visitor.append(promiseReactionStructure);
    visitor.append(promiseCombinatorsContextStructure);
    visitor.append(promiseCombinatorsGlobalContextStructure);
    visitor.append(regExpStructure);
    visitor.append(symbolStructure);
    visitor.append(symbolTableStructure);
    for (auto& structure : cellButterflyStructures)
        visitor.append(structure);
    visitor.append(cellButterflyOnlyAtomStringsStructure);
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
    visitor.append(m_orderedHashTableDeletedValue);
    visitor.append(m_orderedHashTableSentinel);
    visitor.append(m_fastCanConstructBoundExecutable);
    visitor.append(m_slowCanConstructBoundExecutable);
    visitor.append(lastCachedString);
    visitor.append(heapBigIntConstantOne);

    visitor.append(m_promiseResolvingFunctionResolveExecutable);
    visitor.append(m_promiseResolvingFunctionRejectExecutable);
    visitor.append(m_promiseFirstResolvingFunctionResolveExecutable);
    visitor.append(m_promiseFirstResolvingFunctionRejectExecutable);
    visitor.append(m_promiseResolvingFunctionResolveWithInternalMicrotaskExecutable);
    visitor.append(m_promiseResolvingFunctionRejectWithInternalMicrotaskExecutable);
    visitor.append(m_promiseCapabilityExecutorExecutable);
    visitor.append(m_promiseAllFulfillFunctionExecutable);
    visitor.append(m_promiseAllSlowFulfillFunctionExecutable);
    visitor.append(m_promiseAllSettledFulfillFunctionExecutable);
    visitor.append(m_promiseAllSettledRejectFunctionExecutable);
    visitor.append(m_promiseAllSettledSlowFulfillFunctionExecutable);
    visitor.append(m_promiseAllSettledSlowRejectFunctionExecutable);
    visitor.append(m_promiseAnyRejectFunctionExecutable);
    visitor.append(m_promiseAnySlowRejectFunctionExecutable);
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

void VM::performOpportunisticallyScheduledTasks(MonotonicTime deadline, OptionSet<SchedulerOptions> options)
{
    constexpr bool verbose = false;

    dataLogLnIf(verbose, "[OPPORTUNISTIC TASK] QUERY", " signpost:(", JSC::activeJSGlobalObjectSignpostIntervalCount.load(), ")");
    JSLockHolder locker { *this };
    if (deferredWorkTimer->hasImminentlyScheduledWork()) {
        dataLogLnIf(verbose, "[OPPORTUNISTIC TASK] GaveUp: DeferredWorkTimer hasImminentlyScheduledWork signpost:(", JSC::activeJSGlobalObjectSignpostIntervalCount.load(), ")");
        return;
    }

    SetForScope insideOpportunisticTaskScope { heap.m_isInOpportunisticTask, true };
    [&] {
        auto secondsSinceEpoch = ApproximateTime::now().secondsSinceEpoch();
        auto remainingTime = deadline.secondsSinceEpoch() - secondsSinceEpoch;

        if (options.contains(SchedulerOptions::HasImminentlyScheduledWork)) {
            dataLogLnIf(verbose, "[OPPORTUNISTIC TASK] GaveUp: HasImminentlyScheduledWork ", remainingTime, " signpost:(", JSC::activeJSGlobalObjectSignpostIntervalCount.load(), ")");
            return;
        }

        static constexpr auto minimumDelayBeforeOpportunisticFullGC = 30_ms;
        static constexpr auto minimumDelayBeforeOpportunisticEdenGC = 10_ms;
        static constexpr auto extraDurationToAvoidExceedingDeadlineDuringFullGC = 2_ms;
        static constexpr auto extraDurationToAvoidExceedingDeadlineDuringEdenGC = 1_ms;

        auto timeSinceFinishingLastFullGC = secondsSinceEpoch - heap.m_lastFullGCEndTime.secondsSinceEpoch();
        if (timeSinceFinishingLastFullGC > minimumDelayBeforeOpportunisticFullGC && heap.m_shouldDoOpportunisticFullCollection && heap.m_totalBytesVisitedAfterLastFullCollect) {
            auto estimatedGCDuration = (heap.lastFullGCLength() * heap.m_totalBytesVisited) / heap.m_totalBytesVisitedAfterLastFullCollect;
            if (estimatedGCDuration + extraDurationToAvoidExceedingDeadlineDuringFullGC < remainingTime) {
                dataLogLnIf(verbose, "[OPPORTUNISTIC TASK] FULL", " signpost:(", JSC::activeJSGlobalObjectSignpostIntervalCount.load(), ")");
                heap.collectSync(CollectionScope::Full);
                return;
            }
        }

        auto timeSinceLastGC = secondsSinceEpoch - std::max(heap.m_lastGCEndTime, heap.m_currentGCStartTime).secondsSinceEpoch();
        if (timeSinceLastGC > minimumDelayBeforeOpportunisticEdenGC && heap.totalBytesAllocatedThisCycle() && heap.m_bytesAllocatedBeforeLastEdenCollect) {
            auto estimatedGCDuration = (heap.lastEdenGCLength() * heap.totalBytesAllocatedThisCycle()) / heap.m_bytesAllocatedBeforeLastEdenCollect;
            if (estimatedGCDuration + extraDurationToAvoidExceedingDeadlineDuringEdenGC < remainingTime) {
                dataLogLnIf(verbose, "[OPPORTUNISTIC TASK] EDEN: ", timeSinceFinishingLastFullGC, " ", timeSinceLastGC, " ", heap.m_shouldDoOpportunisticFullCollection, " ", heap.m_totalBytesVisitedAfterLastFullCollect, " ", heap.totalBytesAllocatedThisCycle(), " ", heap.m_bytesAllocatedBeforeLastEdenCollect, " ", heap.m_lastGCEndTime, " ", heap.m_currentGCStartTime, " ", (heap.lastFullGCLength() * heap.m_totalBytesVisited) / heap.m_totalBytesVisitedAfterLastFullCollect, " ", remainingTime, " ", (heap.lastEdenGCLength() * heap.totalBytesAllocatedThisCycle()) / heap.m_bytesAllocatedBeforeLastEdenCollect, " signpost:(", JSC::activeJSGlobalObjectSignpostIntervalCount.load(), ")");
                heap.collectSync(CollectionScope::Eden);
                return;
            } else if (estimatedGCDuration < 2 * remainingTime) {
                if (heap.totalBytesAllocatedThisCycle() * 2 > heap.m_minBytesPerCycle) {
                    heap.collectAsync(CollectionScope::Eden);
                    return;
                }
            }
        }

        dataLogLnIf(verbose, "[OPPORTUNISTIC TASK] GaveUp: nothing met. ", timeSinceFinishingLastFullGC, " ", timeSinceLastGC, " ", heap.m_shouldDoOpportunisticFullCollection, " ", heap.m_totalBytesVisitedAfterLastFullCollect, " ", heap.totalBytesAllocatedThisCycle(), " ", heap.m_bytesAllocatedBeforeLastEdenCollect, " ", heap.m_lastGCEndTime, " ", heap.m_currentGCStartTime, " ", (heap.lastFullGCLength() * heap.m_totalBytesVisited) / heap.m_totalBytesVisitedAfterLastFullCollect, " ", remainingTime, " ", (heap.lastEdenGCLength() * heap.totalBytesAllocatedThisCycle()) / heap.m_bytesAllocatedBeforeLastEdenCollect, " signpost:(", JSC::activeJSGlobalObjectSignpostIntervalCount.load(), ")");
    }();

    heap.sweeper().doWorkUntil(*this, deadline);
}

void VM::invalidateStructureChainIntegrity(StructureChainIntegrityEvent)
{
    if (auto* megamorphicCache = this->megamorphicCache())
        megamorphicCache->bumpEpoch();
}

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

#if ENABLE(WEBASSEMBLY_DEBUGGER)
Wasm::DebugState* VM::debugState()
{
    RELEASE_ASSERT(!!m_debugState);
    return m_debugState.get();
}
#endif

} // namespace JSC
