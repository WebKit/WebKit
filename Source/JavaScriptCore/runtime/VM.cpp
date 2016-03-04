/*
 * Copyright (C) 2008, 2011, 2013-2016 Apple Inc. All rights reserved.
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

#include "ArgList.h"
#include "ArrayBufferNeuteringWatchpoint.h"
#include "BuiltinExecutables.h"
#include "BytecodeIntrinsicRegistry.h"
#include "CodeBlock.h"
#include "CodeCache.h"
#include "CommonIdentifiers.h"
#include "CommonSlowPaths.h"
#include "CustomGetterSetter.h"
#include "DFGLongLivedState.h"
#include "DFGWorklist.h"
#include "Disassembler.h"
#include "ErrorInstance.h"
#include "Exception.h"
#include "FTLThunks.h"
#include "FunctionConstructor.h"
#include "GCActivityCallback.h"
#include "GeneratorFrame.h"
#include "GetterSetter.h"
#include "Heap.h"
#include "HeapIterationScope.h"
#include "HeapProfiler.h"
#include "HostCallReturnValue.h"
#include "Identifier.h"
#include "IncrementalSweeper.h"
#include "InferredTypeTable.h"
#include "Interpreter.h"
#include "JITCode.h"
#include "JSAPIValueWrapper.h"
#include "JSArray.h"
#include "JSCInlines.h"
#include "JSFunction.h"
#include "JSGlobalObjectFunctions.h"
#include "JSInternalPromiseDeferred.h"
#include "JSLexicalEnvironment.h"
#include "JSLock.h"
#include "JSNotAnObject.h"
#include "JSPromiseDeferred.h"
#include "JSPropertyNameEnumerator.h"
#include "JSTemplateRegistryKey.h"
#include "JSWithScope.h"
#include "Lexer.h"
#include "Lookup.h"
#include "MapData.h"
#include "NativeStdFunctionCell.h"
#include "Nodes.h"
#include "Parser.h"
#include "ProfilerDatabase.h"
#include "PropertyMapHashTable.h"
#include "RegExpCache.h"
#include "RegExpObject.h"
#include "RegisterAtOffsetList.h"
#include "RuntimeType.h"
#include "SamplingProfiler.h"
#include "SimpleTypedArrayController.h"
#include "SourceProviderCache.h"
#include "StackVisitor.h"
#include "StrictEvalActivation.h"
#include "StrongInlines.h"
#include "StructureInlines.h"
#include "TypeProfiler.h"
#include "TypeProfilerLog.h"
#include "UnlinkedCodeBlock.h"
#include "VMEntryScope.h"
#include "Watchdog.h"
#include "WeakGCMapInlines.h"
#include "WeakMapData.h"
#include <wtf/CurrentTime.h>
#include <wtf/ProcessID.h>
#include <wtf/RetainPtr.h>
#include <wtf/StringPrintStream.h>
#include <wtf/Threading.h>
#include <wtf/WTFThreadData.h>
#include <wtf/text/AtomicStringTable.h>
#include <wtf/text/SymbolRegistry.h>

#if ENABLE(DFG_JIT)
#include "ConservativeRoots.h"
#endif

#if ENABLE(REGEXP_TRACING)
#include "RegExp.h"
#endif

#if USE(CF)
#include <CoreFoundation/CoreFoundation.h>
#endif

using namespace WTF;

namespace JSC {

// Note: Platform.h will enforce that ENABLE(ASSEMBLER) is true if either
// ENABLE(JIT) or ENABLE(YARR_JIT) or both are enabled. The code below
// just checks for ENABLE(JIT) or ENABLE(YARR_JIT) with this premise in mind.

#if ENABLE(ASSEMBLER)
static bool enableAssembler(ExecutableAllocator& executableAllocator)
{
    if (!Options::useJIT() && !Options::useRegExpJIT())
        return false;

    if (!executableAllocator.isValid()) {
        if (Options::crashIfCantAllocateJITMemory())
            CRASH();
        return false;
    }

#if USE(CF) || OS(UNIX)
    char* canUseJITString = getenv("JavaScriptCoreUseJIT");
    return !canUseJITString || atoi(canUseJITString);
#else
    return true;
#endif
}
#endif // ENABLE(!ASSEMBLER)

VM::VM(VMType vmType, HeapType heapType)
    : m_apiLock(adoptRef(new JSLock(this)))
#if ENABLE(ASSEMBLER)
    , executableAllocator(*this)
#endif
    , heap(this, heapType)
    , vmType(vmType)
    , clientData(0)
    , topVMEntryFrame(nullptr)
    , topCallFrame(CallFrame::noCaller())
    , m_atomicStringTable(vmType == Default ? wtfThreadData().atomicStringTable() : new AtomicStringTable)
    , propertyNames(nullptr)
    , emptyList(new MarkedArgumentBuffer)
    , customGetterSetterFunctionMap(*this)
    , stringCache(*this)
    , prototypeMap(*this)
    , interpreter(0)
    , jsArrayClassInfo(JSArray::info())
    , jsFinalObjectClassInfo(JSFinalObject::info())
    , sizeOfLastScratchBuffer(0)
    , entryScope(0)
    , m_regExpCache(new RegExpCache(this))
#if ENABLE(REGEXP_TRACING)
    , m_rtTraceList(new RTTraceList())
#endif
#if ENABLE(ASSEMBLER)
    , m_canUseAssembler(enableAssembler(executableAllocator))
#endif
#if ENABLE(JIT)
    , m_canUseJIT(m_canUseAssembler && Options::useJIT())
#endif
#if ENABLE(YARR_JIT)
    , m_canUseRegExpJIT(m_canUseAssembler && Options::useRegExpJIT())
#endif
#if ENABLE(GC_VALIDATION)
    , m_initializingObjectClass(0)
#endif
    , m_stackPointerAtVMEntry(0)
    , m_stackLimit(0)
#if !ENABLE(JIT)
    , m_jsStackLimit(0)
#endif
#if ENABLE(FTL_JIT)
    , m_ftlStackLimit(0)
    , m_largestFTLStackSize(0)
#endif
    , m_inDefineOwnProperty(false)
    , m_codeCache(std::make_unique<CodeCache>())
    , m_enabledProfiler(nullptr)
    , m_builtinExecutables(std::make_unique<BuiltinExecutables>(*this))
    , m_typeProfilerEnabledCount(0)
    , m_controlFlowProfilerEnabledCount(0)
{
    interpreter = new Interpreter(*this);
    StackBounds stack = wtfThreadData().stack();
    updateReservedZoneSize(Options::reservedZoneSize());
#if !ENABLE(JIT)
    interpreter->stack().setReservedZoneSize(Options::reservedZoneSize());
#endif
    setLastStackTop(stack.origin());

    // Need to be careful to keep everything consistent here
    JSLockHolder lock(this);
    AtomicStringTable* existingEntryAtomicStringTable = wtfThreadData().setCurrentAtomicStringTable(m_atomicStringTable);
    propertyNames = new CommonIdentifiers(this);
    structureStructure.set(*this, Structure::createStructure(*this));
    structureRareDataStructure.set(*this, StructureRareData::createStructure(*this, 0, jsNull()));
    terminatedExecutionErrorStructure.set(*this, TerminatedExecutionError::createStructure(*this, 0, jsNull()));
    stringStructure.set(*this, JSString::createStructure(*this, 0, jsNull()));
    notAnObjectStructure.set(*this, JSNotAnObject::createStructure(*this, 0, jsNull()));
    propertyNameEnumeratorStructure.set(*this, JSPropertyNameEnumerator::createStructure(*this, 0, jsNull()));
    getterSetterStructure.set(*this, GetterSetter::createStructure(*this, 0, jsNull()));
    customGetterSetterStructure.set(*this, CustomGetterSetter::createStructure(*this, 0, jsNull()));
    scopedArgumentsTableStructure.set(*this, ScopedArgumentsTable::createStructure(*this, 0, jsNull()));
    apiWrapperStructure.set(*this, JSAPIValueWrapper::createStructure(*this, 0, jsNull()));
    JSScopeStructure.set(*this, JSScope::createStructure(*this, 0, jsNull()));
    executableStructure.set(*this, ExecutableBase::createStructure(*this, 0, jsNull()));
    nativeExecutableStructure.set(*this, NativeExecutable::createStructure(*this, 0, jsNull()));
    evalExecutableStructure.set(*this, EvalExecutable::createStructure(*this, 0, jsNull()));
    programExecutableStructure.set(*this, ProgramExecutable::createStructure(*this, 0, jsNull()));
    functionExecutableStructure.set(*this, FunctionExecutable::createStructure(*this, 0, jsNull()));
#if ENABLE(WEBASSEMBLY)
    webAssemblyExecutableStructure.set(*this, WebAssemblyExecutable::createStructure(*this, 0, jsNull()));
#endif
    moduleProgramExecutableStructure.set(*this, ModuleProgramExecutable::createStructure(*this, 0, jsNull()));
    regExpStructure.set(*this, RegExp::createStructure(*this, 0, jsNull()));
    symbolStructure.set(*this, Symbol::createStructure(*this, 0, jsNull()));
    symbolTableStructure.set(*this, SymbolTable::createStructure(*this, 0, jsNull()));
    structureChainStructure.set(*this, StructureChain::createStructure(*this, 0, jsNull()));
    sparseArrayValueMapStructure.set(*this, SparseArrayValueMap::createStructure(*this, 0, jsNull()));
    templateRegistryKeyStructure.set(*this, JSTemplateRegistryKey::createStructure(*this, 0, jsNull()));
    arrayBufferNeuteringWatchpointStructure.set(*this, ArrayBufferNeuteringWatchpoint::createStructure(*this));
    unlinkedFunctionExecutableStructure.set(*this, UnlinkedFunctionExecutable::createStructure(*this, 0, jsNull()));
    unlinkedProgramCodeBlockStructure.set(*this, UnlinkedProgramCodeBlock::createStructure(*this, 0, jsNull()));
    unlinkedEvalCodeBlockStructure.set(*this, UnlinkedEvalCodeBlock::createStructure(*this, 0, jsNull()));
    unlinkedFunctionCodeBlockStructure.set(*this, UnlinkedFunctionCodeBlock::createStructure(*this, 0, jsNull()));
    unlinkedModuleProgramCodeBlockStructure.set(*this, UnlinkedModuleProgramCodeBlock::createStructure(*this, 0, jsNull()));
    propertyTableStructure.set(*this, PropertyTable::createStructure(*this, 0, jsNull()));
    weakMapDataStructure.set(*this, WeakMapData::createStructure(*this, 0, jsNull()));
    inferredValueStructure.set(*this, InferredValue::createStructure(*this, 0, jsNull()));
    inferredTypeStructure.set(*this, InferredType::createStructure(*this, 0, jsNull()));
    inferredTypeTableStructure.set(*this, InferredTypeTable::createStructure(*this, 0, jsNull()));
    functionRareDataStructure.set(*this, FunctionRareData::createStructure(*this, 0, jsNull()));
    generatorFrameStructure.set(*this, GeneratorFrame::createStructure(*this, 0, jsNull()));
    exceptionStructure.set(*this, Exception::createStructure(*this, 0, jsNull()));
    promiseDeferredStructure.set(*this, JSPromiseDeferred::createStructure(*this, 0, jsNull()));
    internalPromiseDeferredStructure.set(*this, JSInternalPromiseDeferred::createStructure(*this, 0, jsNull()));
    programCodeBlockStructure.set(*this, ProgramCodeBlock::createStructure(*this, 0, jsNull()));
    moduleProgramCodeBlockStructure.set(*this, ModuleProgramCodeBlock::createStructure(*this, 0, jsNull()));
    evalCodeBlockStructure.set(*this, EvalCodeBlock::createStructure(*this, 0, jsNull()));
    functionCodeBlockStructure.set(*this, FunctionCodeBlock::createStructure(*this, 0, jsNull()));
#if ENABLE(WEBASSEMBLY)
    webAssemblyCodeBlockStructure.set(*this, WebAssemblyCodeBlock::createStructure(*this, 0, jsNull()));
#endif

    iterationTerminator.set(*this, JSFinalObject::create(*this, JSFinalObject::createStructure(*this, 0, jsNull(), 1)));
    nativeStdFunctionCellStructure.set(*this, NativeStdFunctionCell::createStructure(*this, 0, jsNull()));
    smallStrings.initializeCommonStrings(*this);

    wtfThreadData().setCurrentAtomicStringTable(existingEntryAtomicStringTable);

#if ENABLE(JIT)
    jitStubs = std::make_unique<JITThunks>();
    allCalleeSaveRegisterOffsets = std::make_unique<RegisterAtOffsetList>(RegisterSet::vmCalleeSaveRegisters(), RegisterAtOffsetList::ZeroBased);
#endif
    arityCheckData = std::make_unique<CommonSlowPaths::ArityCheckData>();

#if ENABLE(FTL_JIT)
    ftlThunks = std::make_unique<FTL::Thunks>();
#endif // ENABLE(FTL_JIT)
    
    interpreter->initialize();
    
#if ENABLE(JIT)
    initializeHostCallReturnValue(); // This is needed to convince the linker not to drop host call return support.
#endif

    heap.notifyIsSafeToCollect();
    
    LLInt::Data::performAssertions(*this);
    
    if (Options::useProfiler()) {
        m_perBytecodeProfiler = std::make_unique<Profiler::Database>(*this);

        StringPrintStream pathOut;
        const char* profilerPath = getenv("JSC_PROFILER_PATH");
        if (profilerPath)
            pathOut.print(profilerPath, "/");
        pathOut.print("JSCProfile-", getCurrentProcessID(), "-", m_perBytecodeProfiler->databaseID(), ".json");
        m_perBytecodeProfiler->registerToSaveAtExit(pathOut.toCString().data());
    }

    callFrameForCatch = nullptr;

#if ENABLE(DFG_JIT)
    if (canUseJIT())
        dfgState = std::make_unique<DFG::LongLivedState>();
#endif
    
    // Initialize this last, as a free way of asserting that VM initialization itself
    // won't use this.
    m_typedArrayController = adoptRef(new SimpleTypedArrayController());

    m_bytecodeIntrinsicRegistry = std::make_unique<BytecodeIntrinsicRegistry>(*this);

    if (Options::useTypeProfiler())
        enableTypeProfiler();
    if (Options::useControlFlowProfiler())
        enableControlFlowProfiler();
#if ENABLE(SAMPLING_PROFILER)
    if (Options::useSamplingProfiler()) {
        setShouldBuildPCToCodeOriginMapping();
        m_samplingProfiler = adoptRef(new SamplingProfiler(*this, Stopwatch::create()));
        m_samplingProfiler->start();
    }
#endif // ENABLE(SAMPLING_PROFILER)

    if (Options::alwaysGeneratePCToCodeOriginMap())
        setShouldBuildPCToCodeOriginMapping();

    if (Options::watchdog()) {
        std::chrono::milliseconds timeoutMillis(Options::watchdog());
        Watchdog& watchdog = ensureWatchdog();
        watchdog.setTimeLimit(timeoutMillis);
    }
}

VM::~VM()
{
    // Never GC, ever again.
    heap.incrementDeferralDepth();

#if ENABLE(SAMPLING_PROFILER)
    if (m_samplingProfiler)
        m_samplingProfiler->shutdown();
#endif // ENABLE(SAMPLING_PROFILER)
    
#if ENABLE(DFG_JIT)
    // Make sure concurrent compilations are done, but don't install them, since there is
    // no point to doing so.
    for (unsigned i = DFG::numberOfWorklists(); i--;) {
        if (DFG::Worklist* worklist = DFG::worklistForIndexOrNull(i)) {
            worklist->waitUntilAllPlansForVMAreReady(*this);
            worklist->removeAllReadyPlansForVM(*this);
        }
    }
#endif // ENABLE(DFG_JIT)
    
    waitForAsynchronousDisassembly();
    
    // Clear this first to ensure that nobody tries to remove themselves from it.
    m_perBytecodeProfiler = nullptr;

    ASSERT(m_apiLock->currentThreadIsHoldingLock());
    m_apiLock->willDestroyVM(this);
    heap.lastChanceToFinalize();

    delete interpreter;
#ifndef NDEBUG
    interpreter = reinterpret_cast<Interpreter*>(0xbbadbeef);
#endif

    delete emptyList;

    delete propertyNames;
    if (vmType != Default)
        delete m_atomicStringTable;

    delete clientData;
    delete m_regExpCache;
#if ENABLE(REGEXP_TRACING)
    delete m_rtTraceList;
#endif

#if ENABLE(DFG_JIT)
    for (unsigned i = 0; i < scratchBuffers.size(); ++i)
        fastFree(scratchBuffers[i]);
#endif
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

Ref<VM> VM::createLeaked(HeapType heapType)
{
    return create(heapType);
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
    if (!m_watchdog) {
        m_watchdog = adoptRef(new Watchdog());
        
        // The LLINT peeks into the Watchdog object directly. In order to do that,
        // the LLINT assumes that the internal shape of a std::unique_ptr is the
        // same as a plain C++ pointer, and loads the address of Watchdog from it.
        RELEASE_ASSERT(*reinterpret_cast<Watchdog**>(&m_watchdog) == m_watchdog.get());

        // And if we've previously compiled any functions, we need to revert
        // them because they don't have the needed polling checks for the watchdog
        // yet.
        deleteAllCode();
    }
    return *m_watchdog;
}

HeapProfiler& VM::ensureHeapProfiler()
{
    if (!m_heapProfiler)
        m_heapProfiler = std::make_unique<HeapProfiler>(*this);
    return *m_heapProfiler;
}

#if ENABLE(SAMPLING_PROFILER)
void VM::ensureSamplingProfiler(RefPtr<Stopwatch>&& stopwatch)
{
    if (!m_samplingProfiler)
        m_samplingProfiler = adoptRef(new SamplingProfiler(*this, WTFMove(stopwatch)));
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
    case Clz32Intrinsic:
        return clz32ThunkGenerator;
    case FromCharCodeIntrinsic:
        return fromCharCodeThunkGenerator;
    case SqrtIntrinsic:
        return sqrtThunkGenerator;
    case PowIntrinsic:
        return powThunkGenerator;
    case AbsIntrinsic:
        return absThunkGenerator;
    case FloorIntrinsic:
        return floorThunkGenerator;
    case CeilIntrinsic:
        return ceilThunkGenerator;
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
    default:
        return 0;
    }
}

NativeExecutable* VM::getHostFunction(NativeFunction function, NativeFunction constructor, const String& name)
{
    return jitStubs->hostFunctionStub(this, function, constructor, name);
}
NativeExecutable* VM::getHostFunction(NativeFunction function, Intrinsic intrinsic, const String& name)
{
    ASSERT(canUseJIT());
    return jitStubs->hostFunctionStub(this, function, intrinsic != NoIntrinsic ? thunkGeneratorForIntrinsic(intrinsic) : 0, intrinsic, name);
}

#else // !ENABLE(JIT)

NativeExecutable* VM::getHostFunction(NativeFunction function, NativeFunction constructor, const String& name)
{
    return NativeExecutable::create(*this,
        adoptRef(new NativeJITCode(MacroAssemblerCodeRef::createLLIntCodeRef(llint_native_call_trampoline), JITCode::HostCallThunk)), function,
        adoptRef(new NativeJITCode(MacroAssemblerCodeRef::createLLIntCodeRef(llint_native_construct_trampoline), JITCode::HostCallThunk)), constructor,
        NoIntrinsic, name);
}

#endif // !ENABLE(JIT)

VM::ClientData::~ClientData()
{
}

void VM::resetDateCache()
{
    localTimeOffsetCache.reset();
    cachedDateString = String();
    cachedDateStringValue = std::numeric_limits<double>::quiet_NaN();
    dateInstanceCache.reset();
}

void VM::startSampling()
{
    interpreter->startSampling();
}

void VM::stopSampling()
{
    interpreter->stopSampling();
}

void VM::whenIdle(std::function<void()> callback)
{
    if (!entryScope) {
        callback();
        return;
    }

    entryScope->addDidPopListener(callback);
}

void VM::deleteAllLinkedCode()
{
    whenIdle([this]() {
        heap.deleteAllCodeBlocks();
        heap.reportAbandonedObjectGraph();
    });
}

void VM::deleteAllRegExpCode()
{
    whenIdle([this]() {
        m_regExpCache->deleteAllCode();
        heap.reportAbandonedObjectGraph();
    });
}

void VM::deleteAllCode()
{
    whenIdle([this]() {
        m_codeCache->clear();
        m_regExpCache->deleteAllCode();
        heap.deleteAllCodeBlocks();
        heap.deleteAllUnlinkedCodeBlocks();
        heap.reportAbandonedObjectGraph();
    });
}

void VM::dumpSampleData(ExecState* exec)
{
    interpreter->dumpSampleData(exec);
#if ENABLE(ASSEMBLER)
    ExecutableAllocator::dumpProfile();
#endif
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

void VM::throwException(ExecState* exec, Exception* exception)
{
    if (Options::breakOnThrow()) {
        dataLog("In call frame ", RawPointer(exec), " for code block ", *exec->codeBlock(), "\n");
        CRASH();
    }

    ASSERT(exec == topCallFrame || exec == exec->lexicalGlobalObject()->globalExec() || exec == exec->vmEntryGlobalObject()->globalExec());

    interpreter->notifyDebuggerOfExceptionToBeThrown(exec, exception);

    setException(exception);
}

JSValue VM::throwException(ExecState* exec, JSValue thrownValue)
{
    Exception* exception = jsDynamicCast<Exception*>(thrownValue);
    if (!exception)
        exception = Exception::create(*this, thrownValue);

    throwException(exec, exception);
    return JSValue(exception);
}

JSObject* VM::throwException(ExecState* exec, JSObject* error)
{
    return asObject(throwException(exec, JSValue(error)));
}

void VM::setStackPointerAtVMEntry(void* sp)
{
    m_stackPointerAtVMEntry = sp;
    updateStackLimit();
}

size_t VM::updateReservedZoneSize(size_t reservedZoneSize)
{
    size_t oldReservedZoneSize = m_reservedZoneSize;
    m_reservedZoneSize = reservedZoneSize;

    updateStackLimit();

    return oldReservedZoneSize;
}

#if PLATFORM(WIN)
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

inline void VM::updateStackLimit()
{
#if PLATFORM(WIN)
    void* lastStackLimit = m_stackLimit;
#endif

    if (m_stackPointerAtVMEntry) {
        ASSERT(wtfThreadData().stack().isGrowingDownward());
        char* startOfStack = reinterpret_cast<char*>(m_stackPointerAtVMEntry);
#if ENABLE(FTL_JIT)
        m_stackLimit = wtfThreadData().stack().recursionLimit(startOfStack, Options::maxPerThreadStackUsage(), m_reservedZoneSize + m_largestFTLStackSize);
        m_ftlStackLimit = wtfThreadData().stack().recursionLimit(startOfStack, Options::maxPerThreadStackUsage(), m_reservedZoneSize + 2 * m_largestFTLStackSize);
#else
        m_stackLimit = wtfThreadData().stack().recursionLimit(startOfStack, Options::maxPerThreadStackUsage(), m_reservedZoneSize);
#endif
    } else {
#if ENABLE(FTL_JIT)
        m_stackLimit = wtfThreadData().stack().recursionLimit(m_reservedZoneSize + m_largestFTLStackSize);
        m_ftlStackLimit = wtfThreadData().stack().recursionLimit(m_reservedZoneSize + 2 * m_largestFTLStackSize);
#else
        m_stackLimit = wtfThreadData().stack().recursionLimit(m_reservedZoneSize);
#endif
    }

#if PLATFORM(WIN)
    if (lastStackLimit != m_stackLimit)
        preCommitStackMemory(m_stackLimit);
#endif
}

#if ENABLE(FTL_JIT)
void VM::updateFTLLargestStackSize(size_t stackSize)
{
    if (stackSize > m_largestFTLStackSize) {
        m_largestFTLStackSize = stackSize;
        updateStackLimit();
    }
}
#endif

#if ENABLE(DFG_JIT)
void VM::gatherConservativeRoots(ConservativeRoots& conservativeRoots)
{
    for (size_t i = 0; i < scratchBuffers.size(); i++) {
        ScratchBuffer* scratchBuffer = scratchBuffers[i];
        if (scratchBuffer->activeLength()) {
            void* bufferStart = scratchBuffer->dataBuffer();
            conservativeRoots.add(bufferStart, static_cast<void*>(static_cast<char*>(bufferStart) + scratchBuffer->activeLength()));
        }
    }
}
#endif

void logSanitizeStack(VM* vm)
{
    if (Options::verboseSanitizeStack() && vm->topCallFrame) {
        int dummy;
        dataLog(
            "Sanitizing stack with top call frame at ", RawPointer(vm->topCallFrame),
            ", current stack pointer at ", RawPointer(&dummy), ", in ",
            pointerDump(vm->topCallFrame->codeBlock()), " and last code origin = ",
            vm->topCallFrame->codeOrigin(), "\n");
    }
}

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

void VM::registerWatchpointForImpureProperty(const Identifier& propertyName, Watchpoint* watchpoint)
{
    auto result = m_impurePropertyWatchpointSets.add(propertyName.string(), nullptr);
    if (result.isNewEntry)
        result.iterator->value = adoptRef(new WatchpointSet(IsWatched));
    result.iterator->value->add(watchpoint);
}

void VM::addImpureProperty(const String& propertyName)
{
    if (RefPtr<WatchpointSet> watchpointSet = m_impurePropertyWatchpointSets.take(propertyName))
        watchpointSet->fireAll("Impure property added");
}

class SetEnabledProfilerFunctor {
public:
    bool operator()(CodeBlock* codeBlock)
    {
        if (JITCode::isOptimizingJIT(codeBlock->jitType()))
            codeBlock->jettison(Profiler::JettisonDueToLegacyProfiler);
        return false;
    }
};

void VM::setEnabledProfiler(LegacyProfiler* profiler)
{
    m_enabledProfiler = profiler;
    if (m_enabledProfiler) {
        SetEnabledProfilerFunctor functor;
        heap.forEachCodeBlock(functor);
    }
}

static bool enableProfilerWithRespectToCount(unsigned& counter, std::function<void()> doEnableWork)
{
    bool needsToRecompile = false;
    if (!counter) {
        doEnableWork();
        needsToRecompile = true;
    }
    counter++;

    return needsToRecompile;
}

static bool disableProfilerWithRespectToCount(unsigned& counter, std::function<void()> doDisableWork)
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
        this->m_typeProfiler = std::make_unique<TypeProfiler>();
        this->m_typeProfilerLog = std::make_unique<TypeProfilerLog>();
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
        this->m_controlFlowProfiler = std::make_unique<ControlFlowProfiler>();
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

    typeProfilerLog()->processLogEntries(ASCIILiteral("VM Dump Types"));
    typeProfiler()->dumpTypeProfilerData(*this);
}

void VM::queueMicrotask(JSGlobalObject* globalObject, PassRefPtr<Microtask> task)
{
    m_microtaskQueue.append(std::make_unique<QueuedTask>(*this, globalObject, task));
}

void VM::drainMicrotasks()
{
    while (!m_microtaskQueue.isEmpty())
        m_microtaskQueue.takeFirst()->run();
}

void QueuedTask::run()
{
    m_microtask->run(m_globalObject->globalExec());
}

void sanitizeStackForVM(VM* vm)
{
    logSanitizeStack(vm);
#if !ENABLE(JIT)
    vm->interpreter->stack().sanitizeStack();
#else
    sanitizeStackForVMImpl(vm);
#endif
}

} // namespace JSC
