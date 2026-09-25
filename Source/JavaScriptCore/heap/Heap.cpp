/*
 *  Copyright (C) 2003-2026 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 *  Copyright (C) 2026 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "Heap.h"

#include "JSCJSValueInlines.h"

#include "BaselineJITCode.h"
#include "BuiltinExecutables.h"
#include "CodeBlock.h"
#include "CodeBlockSetInlines.h"
#include "CollectingScope.h"
#include "CollectorInlines.h"
#include "ConservativeRoots.h"
#include "EdenGCActivityCallback.h"
#include "EvalExecutable.h"
#include "Exception.h"
#include "FastMallocAlignedMemoryAllocator.h"
#include "FullGCActivityCallback.h"
#include "FunctionExecutableInlines.h"
#include "GCActivityCallback.h"
#include "GCIncomingRefCountedInlines.h"
#include "GCIncomingRefCountedSetInlines.h"
#include "GCSegmentedArrayInlines.h"
#include "GCTypeMap.h"
#include "GigacageAlignedMemoryAllocator.h"
#include "HasOwnPropertyCache.h"
#include "HeapHelperPool.h"
#include "HeapIterationScope.h"
#include "HeapProfiler.h"
#include "HeapSnapshot.h"
#include "HeapSubspaceTypes.h"
#include "HeapVerifier.h"
#include "IncrementalSweeper.h"
#include "Interpreter.h"
#include "IsoCellSetInlines.h"
#include "IsoInlinedHeapCellTypeInlines.h"
#include "JITStubRoutineSet.h"
#include "JITWorklistInlines.h"
#include "JSFinalizationRegistry.h"
#include "JSFunctionWithFields.h"
#include "JSIterator.h"
#include "JSMicrotaskDispatcher.h"
#include "JSModuleLoader.h"
#include "JSPromiseCombinatorsContext.h"
#include "JSPromiseCombinatorsGlobalContext.h"
#include "JSPromiseReaction.h"
#include "JSRawJSONObject.h"
#include "JSRemoteFunction.h"
#include "JSSentinel.h"
#include "JSVirtualMachineInternal.h"
#include "JSWeakMap.h"
#include "JSWeakObjectRef.h"
#include "JSWeakSet.h"
#include "MachineStackMarker.h"
#include "MarkStackMergingConstraint.h"
#include "MarkedSpaceInlines.h"
#include "MarkingConstraintSet.h"
#include "MegamorphicCache.h"
#include "ModuleLoadingContext.h"
#include "ModuleProgramExecutable.h"
#include "ModuleRegistryEntry.h"
#include "NumberObject.h"
#include "PinballCompletion.h"
#include "PreventCollectionScope.h"
#include "ProgramExecutable.h"
#include "ProxyObject.h"
#include "SamplingProfiler.h"
#include "ShadowChicken.h"
#include "SpaceTimeMutatorScheduler.h"
#include "StochasticSpaceTimeMutatorScheduler.h"
#include "StopIfNecessaryTimer.h"
#include "StringSplitCache.h"
#include "StructureAlignedMemoryAllocator.h"
#include "SubspaceInlines.h"
#include "SuperSampler.h"
#include "SweepingScope.h"
#include "SymbolTableInlines.h"
#include "SynchronousStopTheWorldMutatorScheduler.h"
#include "TypeProfiler.h"
#include "TypeProfilerLog.h"
#include "UnlinkedEvalCodeBlock.h"
#include "VM.h"
#include "VerifierSlotVisitorInlines.h"
#include "WasmCallee.h"
#include "WeakMapImplInlines.h"
#include "WeakSetInlines.h"
#include <algorithm>
#include <wtf/AvailableMemory.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/ListDump.h>
#include <wtf/MemoryFootprint.h>
#include <wtf/RAMSize.h>
#include <wtf/Scope.h>
#include <wtf/SetForScope.h>
#include <wtf/SimpleStats.h>
#include <wtf/SpinBackoff.h>
#include <wtf/StringPrintStream.h>
#include <wtf/SystemTracing.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Threading.h>

#if USE(FOUNDATION)
#include <wtf/spi/cocoa/objcSPI.h>
#endif

#ifdef JSC_GLIB_API_ENABLED
#include "JSCGLibWrapperObject.h"
#endif

namespace JSC {

namespace HeapInternal {
static constexpr bool verbose = false;
static constexpr bool verboseStop = false;
}

namespace {

static GrowthMode NODELETE growthMode(size_t ramSize)
{
    // An Aggressive heap uses more memory to go faster.
    // We do this for machines with enough RAM.
    size_t aggressiveHeapThresholdInBytes = static_cast<size_t>(Options::aggressiveHeapThresholdInMB()) * MB;
    if (ramSize >= aggressiveHeapThresholdInBytes)
        return GrowthMode::Aggressive;
    return GrowthMode::Default;
}

static size_t minHeapSize(HeapType heapType, size_t ramSize)
{
    switch (heapType) {
    case HeapType::Large:
        return static_cast<size_t>(std::min(
            static_cast<double>(Options::largeHeapSize()),
            ramSize * Options::smallHeapRAMFraction()));
    case HeapType::Medium:
        return Options::mediumHeapSize();
    case HeapType::Small:
        return Options::smallHeapSize();
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

static size_t NODELETE maxEdenSizeForRateLimiting(GrowthMode growthMode, size_t minBytesPerCycle)
{
    // Only do rate limiting for Aggressive heaps.
    if (growthMode == GrowthMode::Aggressive)
        return Options::maxEdenSizeForRateLimitingMultiplier() * minBytesPerCycle;
    return 0.0;
}

static size_t proportionalHeapSize(size_t heapSize, GrowthMode growthMode, size_t ramSize)
{
    if (VM::isInMiniMode())
        return Options::miniVMHeapGrowthFactor() * heapSize;

    bool useNewHeapGrowthFactor = growthMode == GrowthMode::Aggressive;

    // Use new heuristic function for Aggressive heaps (machines >= 16GB RAM).
    // https://www.mathway.com/en/Algebra?asciimath=2%20*%20e%5E(-1%20*%20x)%20%2B%201%20%3Dy
    // Disable it for Darwin Intel machine.
#if OS(DARWIN) && CPU(X86_64)
    useNewHeapGrowthFactor = false;
#endif

    if (useNewHeapGrowthFactor) {
        double x = static_cast<double>(std::min(heapSize, ramSize)) / ramSize;
        double ratio = Options::heapGrowthMaxIncrease() * std::exp(-(Options::heapGrowthSteepnessFactor() * x)) + 1;
        return ratio * heapSize;
    }

#if USE(MEMORY_FOOTPRINT_API)
    size_t memoryFootprint = WTF::memoryFootprint();
    if (memoryFootprint < ramSize * Options::smallHeapRAMFraction())
        return Options::smallHeapGrowthFactor() * heapSize;
    if (memoryFootprint < ramSize * Options::mediumHeapRAMFraction())
        return Options::mediumHeapGrowthFactor() * heapSize;
#else
    if (heapSize < ramSize * Options::smallHeapRAMFraction())
        return Options::smallHeapGrowthFactor() * heapSize;
    if (heapSize < ramSize * Options::mediumHeapRAMFraction())
        return Options::mediumHeapGrowthFactor() * heapSize;
#endif
    return Options::largeHeapGrowthFactor() * heapSize;
}

static void recordType(TypeCountSet& set, JSCell* cell)
{
    auto typeName = "[unknown]"_s;
    const ClassInfo* info = cell->classInfo();
    if (info && info->className)
        typeName = info->className;
    set.add(typeName);
}

constexpr bool NODELETE measurePhaseTiming()
{
    return false;
}

UncheckedKeyHashMap<const char*, GCTypeMap<SimpleStats>>& timingStats()
{
    static UncheckedKeyHashMap<const char*, GCTypeMap<SimpleStats>>* result;
    static std::once_flag once;
    std::call_once(
        once,
        [] {
            result = new UncheckedKeyHashMap<const char*, GCTypeMap<SimpleStats>>();
        });
    return *result;
}

SimpleStats& timingStats(const char* name, CollectionScope scope)
{
    return timingStats().add(name, GCTypeMap<SimpleStats>()).iterator->value[scope];
}

class TimingScope {
public:
    TimingScope(std::optional<CollectionScope> scope, ASCIILiteral name)
        : m_scope(scope)
        , m_name(name)
    {
        if (measurePhaseTiming())
            m_before = MonotonicTime::now();
    }
    
    TimingScope(JSC::Heap& heap, ASCIILiteral name)
        : TimingScope(heap.collectionScope(), name)
    {
    }
    
    void NODELETE setScope(std::optional<CollectionScope> scope)
    {
        m_scope = scope;
    }
    
    void NODELETE setScope(JSC::Heap& heap)
    {
        setScope(heap.collectionScope());
    }
    
    ~TimingScope()
    {
        if (measurePhaseTiming()) {
            MonotonicTime after = MonotonicTime::now();
            Seconds timing = after - m_before;
            SimpleStats& stats = timingStats(m_name, *m_scope);
            stats.add(timing.milliseconds());
            dataLog("[GC:", *m_scope, "] ", m_name, " took: ", timing.milliseconds(), "ms (average ", stats.mean(), "ms).\n");
        }
    }
private:
    std::optional<CollectionScope> m_scope;
    MonotonicTime m_before;
    ASCIILiteral m_name;
};

} // anonymous namespace

#define INIT_SERVER_ISO_SUBSPACE(name, heapCellType, type) \
    , name ISO_SUBSPACE_INIT(*this, heapCellType, type)

#define INIT_SERVER_STRUCTURE_ISO_SUBSPACE(name, heapCellType, type) \
    , name(#name ""_s, *this, heapCellType, WTF::roundUpToMultipleOf<type::atomSize>(sizeof(type)), type::numberOfLowerTierPreciseCells, structureAllocator.get())

Heap::Heap(VM& vm, HeapType heapType)
    : m_heapType(heapType)
    , m_ramSize(Options::forceRAMSize() ? Options::forceRAMSize() : ramSize())
    , m_growthMode(growthMode(m_ramSize))
    , m_minBytesPerCycle(minHeapSize(m_heapType, m_ramSize))
    , m_maxEdenSizeForRateLimiting(maxEdenSizeForRateLimiting(m_growthMode, m_minBytesPerCycle))
    , m_maxEdenSize(m_minBytesPerCycle)
    , m_maxHeapSize(m_minBytesPerCycle)
    , m_objectSpace(this)
    , m_machineThreads(makeUnique<MachineThreads>())
    , m_collector(makeUnique<Collector>(*this))
    , m_mutatorSlotVisitor(makeUnique<SlotVisitor>(*this, *m_collector, "M"_s))
    , m_mutatorMarkStack(makeUnique<MarkStackArray>())
    , m_constraintSet(makeUnique<MarkingConstraintSet>(*this))
    , m_strongSet(vm)
    , m_codeBlocks(makeUnique<CodeBlockSet>())
    , m_jitStubRoutines(makeUnique<JITStubRoutineSet>())
    // We seed with 10ms so that GCActivityCallback::didAllocate doesn't continuously
    // schedule the timer if we've never done a collection.
    , m_fullActivityCallback(FullGCActivityCallback::tryCreate(*this))
    , m_edenActivityCallback(EdenGCActivityCallback::tryCreate(*this))
    , m_sweeper(adoptRef(*new IncrementalSweeper(this)))
    , m_stopIfNecessaryTimer(adoptRef(*new StopIfNecessaryTimer(vm)))

    // HeapCellTypes
    , auxiliaryHeapCellType(CellAttributes(DoesNotNeedDestruction, HeapCell::Auxiliary))
    , immutableButterflyHeapCellType(CellAttributes(DoesNotNeedDestruction, HeapCell::JSCellWithIndexingHeader))
    , cellHeapCellType(CellAttributes(DoesNotNeedDestruction, HeapCell::JSCell))
    , destructibleCellHeapCellType(CellAttributes(NeedsDestruction, HeapCell::JSCell))
    , apiGlobalObjectHeapCellType(IsoHeapCellType::Args<JSAPIGlobalObject>())
    , callbackConstructorHeapCellType(IsoHeapCellType::Args<JSCallbackConstructor>())
    , callbackGlobalObjectHeapCellType(IsoHeapCellType::Args<JSCallbackObject<JSGlobalObject>>())
    , callbackObjectHeapCellType(IsoHeapCellType::Args<JSCallbackObject<JSNonFinalObject>>())
    , customGetterFunctionHeapCellType(IsoHeapCellType::Args<JSCustomGetterFunction>())
    , customSetterFunctionHeapCellType(IsoHeapCellType::Args<JSCustomSetterFunction>())
    , errorInstanceHeapCellType(IsoHeapCellType::Args<ErrorInstance>())
    , finalizationRegistryCellType(IsoHeapCellType::Args<JSFinalizationRegistry>())
    , globalLexicalEnvironmentHeapCellType(IsoHeapCellType::Args<JSGlobalLexicalEnvironment>())
    , globalObjectHeapCellType(IsoHeapCellType::Args<JSGlobalObject>())
    , injectedScriptHostSpaceHeapCellType(IsoHeapCellType::Args<Inspector::JSInjectedScriptHost>())
    , javaScriptCallFrameHeapCellType(IsoHeapCellType::Args<Inspector::JSJavaScriptCallFrame>())
    , jsModuleRecordHeapCellType(IsoHeapCellType::Args<JSModuleRecord>())
    , syntheticModuleRecordHeapCellType(IsoHeapCellType::Args<SyntheticModuleRecord>())
    , moduleNamespaceObjectHeapCellType(IsoHeapCellType::Args<JSModuleNamespaceObject>())
    , nativeStdFunctionHeapCellType(IsoHeapCellType::Args<JSNativeStdFunction>())
    , weakMapHeapCellType(IsoHeapCellType::Args<JSWeakMap>())
    , weakSetHeapCellType(IsoHeapCellType::Args<JSWeakSet>())
#if JSC_OBJC_API_ENABLED
    , apiWrapperObjectHeapCellType(IsoHeapCellType::Args<JSCallbackObject<JSAPIWrapperObject>>())
    , objCCallbackFunctionHeapCellType(IsoHeapCellType::Args<ObjCCallbackFunction>())
#endif
#ifdef JSC_GLIB_API_ENABLED
    , apiWrapperObjectHeapCellType(IsoHeapCellType::Args<JSCallbackObject<JSAPIWrapperObject>>())
    , callbackAPIWrapperGlobalObjectHeapCellType(IsoHeapCellType::Args<JSCallbackObject<JSAPIWrapperGlobalObject>>())
    , jscCallbackFunctionHeapCellType(IsoHeapCellType::Args<JSCCallbackFunction>())
#endif
    , intlCollatorHeapCellType(IsoHeapCellType::Args<IntlCollator>())
    , intlDateTimeFormatHeapCellType(IsoHeapCellType::Args<IntlDateTimeFormat>())
    , intlDisplayNamesHeapCellType(IsoHeapCellType::Args<IntlDisplayNames>())
    , intlDurationFormatHeapCellType(IsoHeapCellType::Args<IntlDurationFormat>())
    , intlListFormatHeapCellType(IsoHeapCellType::Args<IntlListFormat>())
    , intlLocaleHeapCellType(IsoHeapCellType::Args<IntlLocale>())
    , intlNumberFormatHeapCellType(IsoHeapCellType::Args<IntlNumberFormat>())
    , intlPluralRulesHeapCellType(IsoHeapCellType::Args<IntlPluralRules>())
    , intlRelativeTimeFormatHeapCellType(IsoHeapCellType::Args<IntlRelativeTimeFormat>())
    , intlSegmentIteratorHeapCellType(IsoHeapCellType::Args<IntlSegmentIterator>())
    , intlSegmenterHeapCellType(IsoHeapCellType::Args<IntlSegmenter>())
    , intlSegmentsHeapCellType(IsoHeapCellType::Args<IntlSegments>())
#if ENABLE(WEBASSEMBLY)
    , webAssemblyExceptionHeapCellType(IsoHeapCellType::Args<JSWebAssemblyException>())
    , webAssemblyFunctionHeapCellType(IsoHeapCellType::Args<WebAssemblyFunction>())
    , webAssemblyGlobalHeapCellType(IsoHeapCellType::Args<JSWebAssemblyGlobal>())
    , webAssemblyInstanceHeapCellType(IsoHeapCellType::Args<JSWebAssemblyInstance>())
    , webAssemblyMemoryHeapCellType(IsoHeapCellType::Args<JSWebAssemblyMemory>())
    , webAssemblyModuleHeapCellType(IsoHeapCellType::Args<JSWebAssemblyModule>())
    , webAssemblyModuleRecordHeapCellType(IsoHeapCellType::Args<WebAssemblyModuleRecord>())
    , webAssemblyTableHeapCellType(IsoHeapCellType::Args<JSWebAssemblyTable>())
    , webAssemblyTagHeapCellType(IsoHeapCellType::Args<JSWebAssemblyTag>())
#endif

    // AlignedMemoryAllocators
    , fastMallocAllocator(makeUnique<FastMallocAlignedMemoryAllocator>())
    , primitiveGigacageAllocator(makeUnique<GigacageAlignedMemoryAllocator>(Gigacage::Primitive))
    , structureAllocator(makeUnique<StructureAlignedMemoryAllocator>())

    // Subspaces
    , primitiveGigacageAuxiliarySpace("Primitive Gigacage Auxiliary"_s, *this, auxiliaryHeapCellType, primitiveGigacageAllocator.get()) // Hash:0x3e7cd762
    , auxiliarySpace("Auxiliary"_s, *this, auxiliaryHeapCellType, fastMallocAllocator.get()) // Hash:0x96255ba1
    , immutableButterflyAuxiliarySpace("ImmutableButterfly JSCellWithIndexingHeader"_s, *this, immutableButterflyHeapCellType, fastMallocAllocator.get()) // Hash:0xaadcb3c1
    , cellSpace("JSCell"_s, *this, cellHeapCellType, fastMallocAllocator.get()) // Hash:0xadfb5a79
    , destructibleObjectSpace("JSDestructibleObject"_s, *this, destructibleObjectHeapCellType, fastMallocAllocator.get()) // Hash:0x4f5ed7a9
    FOR_EACH_JSC_COMMON_ISO_SUBSPACE(INIT_SERVER_ISO_SUBSPACE)
    FOR_EACH_JSC_STRUCTURE_ISO_SUBSPACE(INIT_SERVER_STRUCTURE_ISO_SUBSPACE)
    , codeBlockSpaceAndSet ISO_SUBSPACE_INIT(*this, destructibleCellHeapCellType, CodeBlock) // Hash:0x2b743c6a
    , functionExecutableSpaceAndSet ISO_SUBSPACE_INIT(*this, destructibleCellHeapCellType, FunctionExecutable) // Hash:0xbcb36268
    , programExecutableSpaceAndSet ISO_SUBSPACE_INIT(*this, destructibleCellHeapCellType, ProgramExecutable) // Hash:0x4c9208f7
    , unlinkedFunctionExecutableSpaceAndSet ISO_SUBSPACE_INIT(*this, destructibleCellHeapCellType, UnlinkedFunctionExecutable) // Hash:0x3ba0f4e1

{
    if (Options::forceFencedBarrier()) {
        m_mutatorShouldBeFenced = true;
        m_barrierThreshold = tautologicalThreshold;
    }

    m_worldState.store(0);

    // FIXME: move Collector related initialization into Collector

    for (unsigned i = 0, numberOfParallelThreads = heapHelperPool().numberOfThreads(); i < numberOfParallelThreads; ++i) {
        std::unique_ptr<SlotVisitor> visitor = makeUnique<SlotVisitor>(*this, *m_collector, toASCIICString("P", i + 1));
        if (Options::optimizeParallelSlotVisitorsForStoppedMutator())
            visitor->optimizeForStoppedMutator();
        m_collector->m_availableParallelSlotVisitors.append(visitor.get());
        m_collector->m_parallelSlotVisitors.append(WTF::move(visitor));
    }
    
    if (Options::useConcurrentGC()) {
        if (Options::useStochasticMutatorScheduler())
            m_collector->m_scheduler = makeUnique<StochasticSpaceTimeMutatorScheduler>(*this);
        else
            m_collector->m_scheduler = makeUnique<SpaceTimeMutatorScheduler>(*this);
    } else {
        // We simulate turning off concurrent GC by making the scheduler say that the world
        // should always be stopped when the collector is running.
        m_collector->m_scheduler = makeUnique<SynchronousStopTheWorldMutatorScheduler>();
    }
    
    if (Options::verifyHeap())
        m_verifier = makeUnique<HeapVerifier>(this, Options::numberOfGCCyclesToRecordForVerification());
    
    m_collector->m_collectorSlotVisitor->optimizeForStoppedMutator();

    // When memory is critical, allow allocating 25% of the amount above the critical threshold before collecting.
    size_t memoryAboveCriticalThreshold = static_cast<size_t>(static_cast<double>(m_ramSize) * (1.0 - Options::criticalGCMemoryThreshold()));
    m_maxEdenSizeWhenCritical = memoryAboveCriticalThreshold / 4;

    m_collector->startThread();
}

#undef INIT_SERVER_ISO_SUBSPACE
#undef INIT_SERVER_STRUCTURE_ISO_SUBSPACE

Heap::~Heap()
{
    // Scribble m_worldState to make it clear that the heap has already been destroyed if we crash in checkConn
    m_worldState.store(0xbadbeeffu);

    m_collector->forEachSlotVisitor(
        [&] (SlotVisitor& visitor) {
            visitor.clearMarkStacks();
        });
    m_mutatorMarkStack->clear();
    m_collector->m_raceMarkStack->clear();
    
    while (WeakBlock* block = m_detachedWeakBlocks.removeHead())
        WeakBlock::destroy(*this, block);
    destroyAllPooledWeakBlocks();
    ASSERT(!m_weakBlockCount);
}

void Heap::dumpHeapStatisticsAtVMDestruction()
{
    unsigned counter = 0;
    HeapIterationScope iterationScope(*this);
    m_objectSpace.forEachBlock([&] (MarkedBlock::Handle* block) {
        unsigned live = 0;
        block->forEachLiveCell([&] (size_t, HeapCell*, HeapCell::Kind) {
            live++;
            return IterationStatus::Continue;
        });
        dataLogLn("[", counter++, "] ", block->cellSize(), ", ", live, " / ", block->cellsPerBlock(), " ", static_cast<double>(live) / block->cellsPerBlock() * 100, "% ", block->attributes(), " ", block->subspace()->name());
        block->forEachLiveCell([&] (size_t, HeapCell* heapCell, HeapCell::Kind kind) {
            if (kind == HeapCell::Kind::JSCell) {
                auto* cell = static_cast<JSCell*>(heapCell);
                if (cell->isObject())
                    dataLogLn("    ", JSValue((JSObject*)cell));
                else
                    dataLogLn("    ", *cell);
            }
            return IterationStatus::Continue;
        });
    });
}

// The VM is being destroyed and the collector will never run again.
// Run all pending finalizers now because we won't get another chance.
void Heap::lastChanceToFinalize()
{
    MonotonicTime before;
    if (Options::logGC()) [[unlikely]] {
        before = MonotonicTime::now();
        dataLog("[GC<", RawPointer(this), ">: shutdown ");
    }
    
    m_isShuttingDown = true;
    
    RELEASE_ASSERT(!vm().entryScope);
    RELEASE_ASSERT(m_mutatorState == MutatorState::Running);
    
    m_collector->stopCollectingContinuously();

    dataLogIf(Options::logGC(), "1");
    
    // Prevent new collections from being started. This is probably not even necessary, since we're not
    // going to call into anything that starts collections. Still, this makes the algorithm more
    // obviously sound.
    m_isSafeToCollect = false;
    
    dataLogIf(Options::logGC(), "2");

    bool isCollecting;
    {
        Locker locker { *m_collector->m_threadLock };
        isCollecting = m_collector->hasOutstandingRequest();
    }
    if (isCollecting) {
        dataLogIf(Options::logGC(), "...]\n");
        
        // Wait for the current collection to finish.
        waitForCollector(
            [&] (const AbstractLocker&) -> bool {
                return !m_collector->hasOutstandingRequest();
            });
        
        dataLogIf(Options::logGC(), "[GC<", RawPointer(this), ">: shutdown ");
    }
    dataLogIf(Options::logGC(), "3");

    RELEASE_ASSERT(m_collector->m_requests.isEmpty());
    RELEASE_ASSERT(!m_collector->hasOutstandingRequest());
    
    // Carefully bring the thread down.
    bool stopped = false;
    {
        Locker locker { *m_collector->m_threadLock };
        stopped = m_collector->m_thread->tryStop(locker);
        m_collector->m_threadShouldStop = true;
        if (!stopped)
            m_collector->m_threadCondition->notifyOne(locker);
    }

    dataLogIf(Options::logGC(), "4");
    
    if (!stopped)
        m_collector->m_thread->join();
    
    dataLogIf(Options::logGC(), "5 ");

    if (Options::dumpHeapStatisticsAtVMDestruction()) [[unlikely]]
        dumpHeapStatisticsAtVMDestruction();
    
    m_arrayBuffers.lastChanceToFinalize();
    m_objectSpace.lastChanceToFinalize();
    releaseDelayedReleasedObjects();
#if ENABLE(WEBASSEMBLY)
    Wasm::TypeInformation::cleanupIfRequested();
#endif

    m_objectSpace.freeMemory();

    dataLogIf(Options::logGC(), (MonotonicTime::now() - before).milliseconds(), "ms]\n");
}

void Heap::releaseDelayedReleasedObjects()
{
#if USE(FOUNDATION) || defined(JSC_GLIB_API_ENABLED)
    // We need to guard against the case that releasing an object can create more objects due to the
    // release calling into JS. When those JS call(s) exit and all locks are being dropped we end up
    // back here and could try to recursively release objects. We guard that with a recursive entry
    // count. Only the initial call will release objects, recursive calls simple return and let the
    // the initial call to the function take care of any objects created during release time.
    // This also means that we need to loop until there are no objects in m_delayedReleaseObjects
    // and use a temp Vector for the actual releasing.
    if (!m_delayedReleaseRecursionCount++) {
        while (!m_delayedReleaseObjects.isEmpty()) {
            ASSERT(vm().currentThreadIsHoldingAPILock());

            auto objectsToRelease = WTF::move(m_delayedReleaseObjects);

            {
                // We need to drop locks before calling out to arbitrary code.
                JSLock::DropAllLocks dropAllLocks(vm());

#if USE(FOUNDATION)
                void* context = objc_autoreleasePoolPush();
#endif
                objectsToRelease.clear();
#if USE(FOUNDATION)
                objc_autoreleasePoolPop(context);
#endif
            }
        }
    }
    m_delayedReleaseRecursionCount--;
#endif
}

void Heap::reportExtraMemoryAllocatedPossiblyFromAlreadyMarkedCell(const JSCell* cell, size_t size)
{
    ASSERT(cell);

    // Increasing extraMemory of already marked objects will not be visible as a retained memory.
    // We need to report this additionally to tell GC that we get additional extra memory now,
    // and GC needs to consider scheduling GC based on this increase.

    if (mutatorShouldBeFenced()) [[unlikely]] {
        // In this case, the barrierThreshold is the tautological threshold, so cell could still be
        // not black. But we can't know for sure until we fire off a fence.
        WTF::storeLoadFence();
        if (cell->cellState() != CellState::PossiblyBlack)
            return;

        WTF::loadLoadFence();
        if (!isMarked(cell)) {
            // During a full collection a store into an unmarked object that had surivived past
            // collections will manifest as a store to an unmarked PossiblyBlack object. If the
            // object gets marked at some time after this then it will go down the normal marking
            // path. So, we don't have to remember this object. We could return here. But we go
            // further and attempt to re-white the object.
            ASSERT(m_collectionScope && m_collectionScope.value() == CollectionScope::Full);
            return;
        }
    } else
        ASSERT(isMarked(cell));

    // It could be that the object was *just* marked. This means that the collector may set the
    // state to DefinitelyGrey and then to PossiblyOldOrBlack at any time. It's OK for us to
    // race with the collector here. If we win then this is accurate because the object _will_
    // get scanned again. If we lose then someone else will barrier the object again. That would
    // be unfortunate but not the end of the world.
    reportExtraMemoryVisited(size);
}

void Heap::reportExtraMemoryAllocatedSlowCase(GCDeferralContext* deferralContext, const JSCell* cell, size_t size)
{
    didAllocate(size);
    if (cell) {
        if (isWithinThreshold(cell->cellState(), barrierThreshold())) [[unlikely]]
            reportExtraMemoryAllocatedPossiblyFromAlreadyMarkedCell(cell, size);
    }
    collectIfNecessaryOrDefer(deferralContext);
}

void Heap::deprecatedReportExtraMemorySlowCase(size_t size)
{
    // FIXME: Change this to use SaturatingArithmetic when available.
    // https://bugs.webkit.org/show_bug.cgi?id=170411
    CheckedSize checkedNewSize = m_deprecatedExtraMemorySize;
    checkedNewSize += size;
    size_t newSize = std::numeric_limits<size_t>::max();
    if (!checkedNewSize.hasOverflowed()) [[likely]]
        newSize = checkedNewSize.value();
    m_deprecatedExtraMemorySize = newSize;
    reportExtraMemoryAllocatedSlowCase(nullptr, nullptr, size);
}

bool Heap::overCriticalMemoryThreshold(MemoryThresholdCallType memoryThresholdCallType)
{
#if USE(MEMORY_FOOTPRINT_API)
    if (memoryThresholdCallType == MemoryThresholdCallType::Direct || ++m_percentAvailableMemoryCachedCallCount >= 100) {
        m_overCriticalMemoryThreshold = WTF::percentAvailableMemoryInUse() > Options::criticalGCMemoryThreshold();
        m_percentAvailableMemoryCachedCallCount = 0;
    }

    return m_overCriticalMemoryThreshold;
#else
    UNUSED_PARAM(memoryThresholdCallType);
    return false;
#endif
}

void Heap::reportAbandonedObjectGraph()
{
    // Our clients don't know exactly how much memory they
    // are abandoning so we just guess for them.
    size_t abandonedBytes = static_cast<size_t>(0.1 * capacity());

    m_bytesAbandonedSinceLastFullCollect += abandonedBytes;

    // We want to accelerate the next collection. Because memory has just
    // been abandoned, the next collection has the potential to
    // be more profitable. Since allocation is the trigger for collection, 
    // we hasten the next collection by pretending that we've allocated more memory. 
    if (m_fullActivityCallback) {
        m_fullActivityCallback->didAllocate(*this,
            m_sizeAfterLastCollect - m_sizeAfterLastFullCollect + totalBytesAllocatedThisCycle() + m_bytesAbandonedSinceLastFullCollect);
    }
}

void Heap::protect(JSValue k)
{
    ASSERT(k);
    ASSERT(vm().currentThreadIsHoldingAPILock());

    if (!k.isCell())
        return;

    m_protectedValues.add(k.asCell());
}

bool Heap::unprotect(JSValue k)
{
    ASSERT(k);
    ASSERT(vm().currentThreadIsHoldingAPILock());

    if (!k.isCell())
        return false;

    return m_protectedValues.remove(k.asCell());
}

void Heap::addReference(JSCell* cell, ArrayBuffer* buffer)
{
    if (m_arrayBuffers.addReference(cell, buffer)) {
        collectIfNecessaryOrDefer();
        didAllocate(buffer->gcSizeEstimateInBytes());
    }
}

template<typename CellType, typename CellSet>
void Heap::reconcileWeakReferencesInMarkedCells(CellSet& cellSet, CollectionScope collectionScope)
{
    cellSet.forEachMarkedCell(
        [&] (HeapCell* cell, HeapCell::Kind) {
            static_cast<CellType*>(cell)->reconcileWeakReferencesAtGCEnd(vm(), collectionScope);
        });
}

// Weak reference reconciliation: settle every untraced pointer against the liveness that
// marking just established. Must run after marking, because isMarked() only means "dead"
// once the closure is complete, and before sweeping, because a dying referent may still
// need to be identified or read.
void Heap::reconcileWeakReferencesAtGCEnd()
{
    CollectionScope collectionScope = this->collectionScope().value_or(CollectionScope::Full);

    {
        // Executables go before CodeBlock, since CodeBlock::reconcileWeakReferencesAtGCEnd looks at the owner executable's installed CodeBlock.

        // FunctionExecutable requires all live instances to be processed, so iterate the whole space rather than a tracking set.
        reconcileWeakReferencesInMarkedCells<FunctionExecutable>(functionExecutableSpaceAndSet.space, collectionScope);

        reconcileWeakReferencesInMarkedCells<ProgramExecutable>(programExecutableSpaceAndSet.weakReconciliationSet, collectionScope);
        if (m_evalExecutableSpace)
            reconcileWeakReferencesInMarkedCells<EvalExecutable>(m_evalExecutableSpace->weakReconciliationSet, collectionScope);
        if (m_moduleProgramExecutableSpace)
            reconcileWeakReferencesInMarkedCells<ModuleProgramExecutable>(m_moduleProgramExecutableSpace->weakReconciliationSet, collectionScope);
    }

    reconcileWeakReferencesInMarkedCells<SymbolTable>(symbolTableSpace, collectionScope);

    forEachCodeBlockSpace(
        [&] (auto& space) {
            this->reconcileWeakReferencesInMarkedCells<CodeBlock>(space.set, collectionScope);
        });
    if (collectionScope == CollectionScope::Full) {
        reconcileWeakReferencesInMarkedCells<Structure>(structureSpace, collectionScope);
        reconcileWeakReferencesInMarkedCells<BrandedStructure>(brandedStructureSpace, collectionScope);
#if ENABLE(WEBASSEMBLY)
        reconcileWeakReferencesInMarkedCells<WebAssemblyGCStructure>(webAssemblyGCStructureSpace, collectionScope);
#endif
    }
    reconcileWeakReferencesInMarkedCells<StructureRareData>(structureRareDataSpace, collectionScope);
    reconcileWeakReferencesInMarkedCells<UnlinkedFunctionExecutable>(unlinkedFunctionExecutableSpaceAndSet.set, collectionScope);
    if (m_weakSetSpace)
        reconcileWeakReferencesInMarkedCells<JSWeakSet>(*m_weakSetSpace, collectionScope);
    if (m_weakMapSpace)
        reconcileWeakReferencesInMarkedCells<JSWeakMap>(*m_weakMapSpace, collectionScope);
    if (m_weakObjectRefSpace)
        reconcileWeakReferencesInMarkedCells<JSWeakObjectRef>(*m_weakObjectRefSpace, collectionScope);
    if (m_errorInstanceSpace)
        reconcileWeakReferencesInMarkedCells<ErrorInstance>(*m_errorInstanceSpace, collectionScope);

    // FinalizationRegistries currently rely on serial finalization because they can post tasks to the deferredWorkTimer, which normally expects tasks to only be posted by the API lock holder.
    if (m_finalizationRegistrySpace)
        reconcileWeakReferencesInMarkedCells<JSFinalizationRegistry>(*m_finalizationRegistrySpace, collectionScope);

#if ENABLE(WEBASSEMBLY)
    if (m_webAssemblyInstanceSpace)
        reconcileWeakReferencesInMarkedCells<JSWebAssemblyInstance>(*m_webAssemblyInstanceSpace, collectionScope);
#endif

    vm().reconcileWeakReferencesAtGCEnd();

    if (auto* clientData = vm().clientData)
        clientData->reconcileWeakReferencesAtGCEnd(vm(), collectionScope);
}

void Heap::willStartIterating()
{
    m_objectSpace.willStartIterating();
}

void Heap::didFinishIterating()
{
    m_objectSpace.didFinishIterating();
}

void Heap::completeAllJITPlans()
{
    if (!Options::useJIT())
        return;
#if ENABLE(JIT)
    JITWorklist::ensureGlobalWorklist().completeAllPlansForVM(vm());
#endif // ENABLE(JIT)
}

template<typename Visitor>
void Heap::iterateExecutingAndCompilingCodeBlocks(Visitor& visitor, NOESCAPE const Function<void(CodeBlock*)>& func)
{
    m_codeBlocks->iterateCurrentlyExecuting(func);
#if ENABLE(JIT)
    if (Options::useJIT())
        JITWorklist::ensureGlobalWorklist().iterateCodeBlocksForGC(visitor, vm(), func);
#else
    UNUSED_PARAM(visitor);
#endif // ENABLE(JIT)
}

template<typename Func, typename Visitor>
void Heap::iterateExecutingAndCompilingCodeBlocksWithoutHoldingLocks(Visitor& visitor, const Func& func)
{
    Vector<CodeBlock*, 256> codeBlocks;
    iterateExecutingAndCompilingCodeBlocks(visitor,
        [&] (CodeBlock* codeBlock) {
            codeBlocks.append(codeBlock);
        });
    for (CodeBlock* codeBlock : codeBlocks)
        func(codeBlock);
}

// Collector::runEndPhase calls this from another translation unit, which cannot instantiate a
// definition that lives here.
template void Heap::iterateExecutingAndCompilingCodeBlocks<SlotVisitor>(SlotVisitor&, NOESCAPE const Function<void(CodeBlock*)>&);

void Heap::gatherStackRoots(ConservativeRoots& roots)
{
    m_machineThreads->gatherConservativeRoots(roots, *m_jitStubRoutines, *m_codeBlocks, m_collector->m_conductingMutatorState, m_collector->m_conductorThread);
#if ENABLE(C_LOOP)
    vm().cloopStack().gatherConservativeRoots(roots, *m_jitStubRoutines, *m_codeBlocks);
#endif
}

void Heap::gatherVMRoots(ConservativeRoots& roots)
{
    VM& vm = this->vm();
#if ENABLE(DFG_JIT)
    if (Options::useJIT()) {
        vm.gatherScratchBufferRoots(roots);
        vm.scanSideState(roots);
    }
#endif
#if !ENABLE(DFG_JIT)
    UNUSED_PARAM(roots);
    UNUSED_VARIABLE(vm);
#endif
}

void Heap::beginMarking()
{
    TimingScope timingScope(*this, "Heap::beginMarking"_s);
    m_jitStubRoutines->clearMarks();
    m_objectSpace.beginMarking();
    vm().beginMarking();
    setMutatorShouldBeFenced(true);

#if ENABLE(WEBASSEMBLY)
    prepareWasmCalleeCleanup();
#endif
}

void Heap::removeDeadCompilerWorklistEntries()
{
    if (!Options::useJIT())
        return;
#if ENABLE(JIT)
    JITWorklist::ensureGlobalWorklist().removeDeadPlans(vm());
#endif // ENABLE(JIT)
}

struct GatherExtraHeapData : MarkedBlock::CountFunctor {
    GatherExtraHeapData(HeapAnalyzer& analyzer)
        : m_analyzer(analyzer)
    {
    }

    IterationStatus operator()(HeapCell* heapCell, HeapCell::Kind kind) const
    {
        if (isJSCellKind(kind)) {
            JSCell* cell = static_cast<JSCell*>(heapCell);
            cell->methodTable()->analyzeHeap(cell, m_analyzer);
        }
        return IterationStatus::Continue;
    }

    HeapAnalyzer& m_analyzer;
};

void Heap::gatherExtraHeapData(HeapProfiler& heapProfiler)
{
    if (auto* analyzer = heapProfiler.activeHeapAnalyzer()) {
        HeapIterationScope heapIterationScope(*this);
        GatherExtraHeapData functor(*analyzer);
        m_objectSpace.forEachLiveCell(heapIterationScope, functor);
    }
}

struct RemoveDeadHeapSnapshotNodes : MarkedBlock::CountFunctor {
    RemoveDeadHeapSnapshotNodes(HeapSnapshot& snapshot)
        : m_snapshot(snapshot)
    {
    }

    IterationStatus operator()(HeapCell* cell, HeapCell::Kind kind) const
    {
        if (isJSCellKind(kind))
            m_snapshot.sweepCell(static_cast<JSCell*>(cell));
        return IterationStatus::Continue;
    }

    HeapSnapshot& m_snapshot;
};

void Heap::removeDeadHeapSnapshotNodes(HeapProfiler& heapProfiler)
{
    if (HeapSnapshot* snapshot = heapProfiler.mostRecentSnapshot()) {
        HeapIterationScope heapIterationScope(*this);
        RemoveDeadHeapSnapshotNodes functor(*snapshot);
        m_objectSpace.forEachDeadCell(heapIterationScope, functor);
        snapshot->shrinkToFit();
    }
}

void Heap::updateObjectCounts()
{
    if (m_collectionScope && m_collectionScope.value() == CollectionScope::Full) {
        m_totalBytesVisitedAfterLastFullCollect = m_totalBytesVisited;
        m_totalBytesVisited = 0;
    }

    m_totalBytesVisitedThisCycle = m_collector->bytesVisited();
    
    m_totalBytesVisited += m_totalBytesVisitedThisCycle;
}

void Heap::endMarking()
{
    m_objectSpace.endMarking();
    setMutatorShouldBeFenced(Options::forceFencedBarrier());

#if ENABLE(WEBASSEMBLY)
    finalizeWasmCalleeCleanup();
#endif
}

size_t Heap::objectCount()
{
    return m_objectSpace.objectCount();
}

size_t Heap::extraMemorySize()
{
    // FIXME: Change this to use SaturatingArithmetic when available.
    // https://bugs.webkit.org/show_bug.cgi?id=170411
    CheckedSize checkedTotal = m_extraMemorySize;
    checkedTotal += m_deprecatedExtraMemorySize;
    checkedTotal += m_arrayBuffers.size();
    size_t total = std::numeric_limits<size_t>::max();
    if (!checkedTotal.hasOverflowed()) [[likely]]
        total = checkedTotal.value();

    // It would be nice to have `ASSERT(m_objectSpace.capacity() >= m_objectSpace.size());` here but `m_objectSpace.size()`
    // requires having heap access which thread might not. Specifically, we might be called from the resource usage thread.
    return std::min(total, std::numeric_limits<size_t>::max() - m_objectSpace.capacity());
}

size_t Heap::size()
{
    return m_objectSpace.size() + extraMemorySize();
}

size_t Heap::capacity()
{
    return m_objectSpace.capacity() + extraMemorySize();
}

size_t Heap::protectedGlobalObjectCount()
{
    size_t result = 0;
    forEachProtectedCell(
        [&] (JSCell* cell) {
            if (cell->isObject() && asObject(cell)->isGlobalObject())
                result++;
        });
    return result;
}

size_t Heap::globalObjectCount()
{
    HeapIterationScope iterationScope(*this);
    size_t result = 0;
    m_objectSpace.forEachLiveCell(
        iterationScope,
        [&] (HeapCell* heapCell, HeapCell::Kind kind) -> IterationStatus {
            if (!isJSCellKind(kind))
                return IterationStatus::Continue;
            JSCell* cell = static_cast<JSCell*>(heapCell);
            if (cell->isObject() && asObject(cell)->isGlobalObject())
                result++;
            return IterationStatus::Continue;
        });
    return result;
}

size_t Heap::protectedObjectCount()
{
    size_t result = 0;
    forEachProtectedCell(
        [&] (JSCell*) {
            result++;
        });
    return result;
}

TypeCountSet Heap::protectedObjectTypeCounts()
{
    TypeCountSet result;
    forEachProtectedCell(
        [&] (JSCell* cell) {
            recordType(result, cell);
        });
    return result;
}

TypeCountSet Heap::objectTypeCounts()
{
    TypeCountSet result;
    HeapIterationScope iterationScope(*this);
    m_objectSpace.forEachLiveCell(
        iterationScope,
        [&] (HeapCell* cell, HeapCell::Kind kind) -> IterationStatus {
            if (isJSCellKind(kind))
                recordType(result, static_cast<JSCell*>(cell));
            return IterationStatus::Continue;
        });
    return result;
}

void Heap::deleteAllCodeBlocks(DeleteAllCodeEffort effort)
{
    if (m_collectionScope && effort == DeleteAllCodeIfNotCollecting)
        return;

    VM& vm = this->vm();
    PreventCollectionScope preventCollectionScope(*this);
    
    // If JavaScript is running, it's not safe to delete all JavaScript code, since
    // we'll end up returning to deleted code.
    RELEASE_ASSERT(!vm.entryScope);
    RELEASE_ASSERT(!m_collectionScope);

    completeAllJITPlans();

    forEachScriptExecutableSpace(
        [&] (auto& spaceAndSet) {
            HeapIterationScope heapIterationScope(*this);
            auto& set = spaceAndSet.clearableCodeSet;
            set.forEachLiveCell(
                [&] (HeapCell* cell, HeapCell::Kind) {
                    ScriptExecutable* executable = static_cast<ScriptExecutable*>(cell);
                    executable->clearCode(set);
                });
        });

    // MicrotaskCallCache lives outside any CodeBlock and keys its cached entry points on the callee's
    // executable, so after the code is detached above its callee check would still hit and call into it.
    vm.clearMicrotaskCallCaches();

#if ENABLE(WEBASSEMBLY)
    {
        // We must ensure that we clear the JS call ICs from Wasm. Otherwise, Wasm will
        // have no idea that we cleared the code from all of the Executables in the
        // VM. This could leave Wasm in an inconsistent state where it has an IC that
        // points into a CodeBlock that could be dead. The IC will still succeed because
        // it uses a callee check, but then it will call into dead code.

        // PreciseAllocations are always eagerly swept so we don't have to worry about handling instances pending destruction thus need a HeapIterationScope
        if (m_webAssemblyInstanceSpace) {
            m_webAssemblyInstanceSpace->forEachLiveCell([&] (HeapCell* cell, HeapCell::Kind kind) {
                ASSERT_UNUSED(kind, kind == HeapCell::JSCell);
                static_cast<JSWebAssemblyInstance*>(cell)->clearJSCallICs(vm);
            });
        }
    }
#endif
}

void Heap::deleteAllUnlinkedCodeBlocks(DeleteAllCodeEffort effort)
{
    if (m_collectionScope && effort == DeleteAllCodeIfNotCollecting)
        return;

    VM& vm = this->vm();
    PreventCollectionScope preventCollectionScope(*this);

    RELEASE_ASSERT(!m_collectionScope);

    HeapIterationScope heapIterationScope(*this);
    unlinkedFunctionExecutableSpaceAndSet.set.forEachLiveCell(
        [&] (HeapCell* cell, HeapCell::Kind) {
            UnlinkedFunctionExecutable* executable = static_cast<UnlinkedFunctionExecutable*>(cell);
            executable->clearCode(vm);
        });

#if ENABLE(JIT)
    // Shareable Baseline JIT code is cached on UnlinkedCodeBlock::m_unlinkedBaselineCode (populated by
    // CodeBlock::setupWithUnlinkedBaselineCode). That cache is not owned by any linked CodeBlock or
    // executable, so it survives deleteAllCodeBlocks and is otherwise only released when the
    // UnlinkedCodeBlock itself is collected. A "warm" UnlinkedCodeBlock can therefore pin Baseline JIT
    // executable memory across memory warnings indefinitely. Drop the cache eagerly here: any still-linked
    // CodeBlock holds its own ref to the BaselineJITCode (so we never free code that is still in use),
    // while a cache-only entry is freed as soon as its last ref goes away, synchronously here.
    if (Options::useBaselineJITCodeSharing()) {
        auto clearUnlinkedBaselineCode = [] (HeapCell* cell, HeapCell::Kind) {
            static_cast<UnlinkedCodeBlock*>(cell)->m_unlinkedBaselineCode = nullptr;
        };
        for (auto* space : { m_unlinkedFunctionCodeBlockSpace.get(), m_unlinkedProgramCodeBlockSpace.get(), m_unlinkedEvalCodeBlockSpace.get(), m_unlinkedModuleProgramCodeBlockSpace.get() }) {
            if (space)
                space->forEachLiveCell(clearUnlinkedBaselineCode);
        }
    }
#endif
}

void Heap::deleteUnmarkedCompiledCode()
{
    m_jitStubRoutines->deleteUnmarkedJettisonedStubRoutines(vm());
}

void Heap::addToRememberedSet(const JSCell* constCell)
{
    JSCell* cell = const_cast<JSCell*>(constCell);
    ASSERT(cell);
    ASSERT(!Options::useConcurrentJIT() || !isCompilationThread());
    m_barriersExecuted++;
    if (m_mutatorShouldBeFenced) {
        WTF::loadLoadFence();
        if (!isMarked(cell)) {
            // During a full collection a store into an unmarked object that had surivived past
            // collections will manifest as a store to an unmarked PossiblyBlack object. If the
            // object gets marked at some time after this then it will go down the normal marking
            // path. So, we don't have to remember this object. We could return here. But we go
            // further and attempt to re-white the object.
            
            RELEASE_ASSERT(m_collectionScope && m_collectionScope.value() == CollectionScope::Full);
            
            if (cell->atomicCompareExchangeCellStateStrong(CellState::PossiblyBlack, CellState::DefinitelyWhite) == CellState::PossiblyBlack) {
                // Now we protect against this race:
                //
                //     1) Object starts out black + unmarked.
                //     --> We do isMarked here.
                //     2) Object is marked and greyed.
                //     3) Object is scanned and blacked.
                //     --> We do atomicCompareExchangeCellStateStrong here.
                //
                // In this case we would have made the object white again, even though it should
                // be black. This check lets us correct our mistake. This relies on the fact that
                // isMarked converges monotonically to true.
                if (isMarked(cell)) {
                    // It's difficult to work out whether the object should be grey or black at
                    // this point. We say black conservatively.
                    cell->setCellState(CellState::PossiblyBlack);
                }
                
                // Either way, we can return. Most likely, the object was not marked, and so the
                // object is now labeled white. This means that future barrier executions will not
                // fire. In the unlikely event that the object had become marked, we can still
                // return anyway, since we proved that the object was not marked at the time that
                // we executed this slow path.
            }
            
            return;
        }
    } else
        ASSERT(isMarked(cell));
    // It could be that the object was *just* marked. This means that the collector may set the
    // state to DefinitelyGrey and then to PossiblyOldOrBlack at any time. It's OK for us to
    // race with the collector here. If we win then this is accurate because the object _will_
    // get scanned again. If we lose then someone else will barrier the object again. That would
    // be unfortunate but not the end of the world.
    cell->setCellState(CellState::PossiblyGrey);
    m_mutatorMarkStack->append(cell);
}

void Heap::clearConcurrentRetainedDataIfPossible()
{

    // FIXME: It's weird that we drive the runloop in the middle of a JS stack. But a few places in WebCore testing/debugger code do that so clearing would otherwise be invalid there. This is technically not strong enough to catch any bad code but it seems to work for the testing/debugger code in question.
    if (vm().entryScope) [[unlikely]]
        return;

    // It wouldn't be safe to clear the list if it's possible to have a GCOwnedDataScope on the stack since
    // m_possiblyAccessedStringsFromConcurrentThreadsOrGCOwnedDataScope
    // is what's keeping the backing bytes alive.
    ASSERT(!m_topGCOwnedDataScope);

    if (!m_possiblyAccessedStringsFromConcurrentThreadsOrGCOwnedDataScope.size())
        return;

    // The mutator needs to be fenced while marking and marker threads can access StringImpl::costDuringGC so we have to keep the Impls alive.
    if (mutatorShouldBeFenced())
        return;
#if ENABLE(JIT)
    auto* worklist = JITWorklist::existingGlobalWorklistOrNull();
    // We need to make sure no JIT thread could be looking at one of our old strings. Any thread that starts after
    // this check will load the new StringImpl rather than the one in this list so we're safe to delete these as
    // long as none were running at the time of this check.
    if (!worklist || !worklist->totalOngoingCompilations()) {
#else
    {
#endif
        m_possiblyAccessedStringsFromConcurrentThreadsOrGCOwnedDataScope.clear();
    }
}

void Heap::sweepSynchronously()
{
    if (!Options::useGC()) [[unlikely]]
        return;

    MonotonicTime before { };
    if (Options::logGC()) [[unlikely]] {
        dataLog("Full sweep: ", capacity() / 1024, "kb ");
        before = MonotonicTime::now();
    }
    m_objectSpace.sweepBlocks();
    m_objectSpace.shrink();
    destroyAllPooledWeakBlocks();
#if ENABLE(WEBASSEMBLY)
    Wasm::TypeInformation::cleanupIfRequested();
#endif
    if (Options::logGC()) [[unlikely]] {
        MonotonicTime after = MonotonicTime::now();
        dataLog("=> ", capacity() / 1024, "kb, ", (after - before).milliseconds(), "ms");
    }
}

void Heap::collect(Synchronousness synchronousness, GCRequest request)
{
    if (!Options::useGC()) [[unlikely]]
        return;

    switch (synchronousness) {
    case Async: {
        collectAsync(request);
        return;
    }
    case Sync:
        collectSync(request);
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void Heap::collectNow(Synchronousness synchronousness, GCRequest request)
{
    if (!Options::useGC()) [[unlikely]]
        return;

    if constexpr (validateDFGDoesGC)
        vm().verifyCanGC();

    switch (synchronousness) {
    case Async: {
        collectAsync(request);
        stopIfNecessary();
        return;
    }
        
    case Sync: {
        collectSync(request);
        
        DeferGCForAWhile deferGC(vm());
        if (Options::useImmortalObjects()) [[unlikely]]
            sweeper().stopSweeping();
        
        bool alreadySweptInCollectSync = shouldSweepSynchronously();
        if (!alreadySweptInCollectSync) {
            dataLogIf(Options::logGC(), "[GC<", RawPointer(this), ">: ");
            sweepSynchronously();
            dataLogIf(Options::logGC(), "]\n");
        }
        m_objectSpace.assertNoUnswept();
        return;
    } }
    RELEASE_ASSERT_NOT_REACHED();
}

void Heap::collectAsync(GCRequest request)
{
    if (!Options::useGC()) [[unlikely]]
        return;

    if constexpr (validateDFGDoesGC)
        vm().verifyCanGC();

    if (!m_isSafeToCollect)
        return;

    bool alreadyRequested = false;
    {
        Locker locker { *m_collector->m_threadLock };
        for (const GCRequest& previousRequest : m_collector->m_requests) {
            if (request.subsumedBy(previousRequest)) {
                alreadyRequested = true;
                break;
            }
        }
    }
    if (alreadyRequested)
        return;

    requestCollection(request);
}

void Heap::collectSync(GCRequest request)
{
    if (!Options::useGC()) [[unlikely]]
        return;

    if constexpr (validateDFGDoesGC)
        vm().verifyCanGC();

    if (!m_isSafeToCollect)
        return;

    waitForCollection(requestCollection(request));
}

ALWAYS_INLINE int asInt(CollectorPhase phase)
{
    return static_cast<int>(phase);
}

void Heap::checkConn(GCConductor conn)
{
    unsigned worldState = m_worldState.load();
    switch (conn) {
    case GCConductor::Mutator:
        RELEASE_ASSERT(worldState & mutatorHasConnBit, worldState, asInt(m_collector->m_lastPhase), asInt(m_collector->m_currentPhase), asInt(m_collector->m_nextPhase), vm().identifier().toUInt64(), vm().isEntered());
        return;
    case GCConductor::Collector:
        RELEASE_ASSERT(!(worldState & mutatorHasConnBit), worldState, asInt(m_collector->m_lastPhase), asInt(m_collector->m_currentPhase), asInt(m_collector->m_nextPhase), vm().identifier().toUInt64(), vm().isEntered());
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void Heap::stopThePeriphery()
{
    if (m_worldIsStopped) {
        dataLog("FATAL: world already stopped.\n");
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    if (m_mutatorDidRun)
        m_mutatorExecutionVersion++;
    
    m_mutatorDidRun = false;

    m_worldIsStopped = true;

    if (auto* shadowChicken = vm().shadowChicken())
        shadowChicken->update(vm(), vm().topCallFrame);

    m_objectSpace.stopAllocating();
}

void Heap::resumeThePeriphery()
{
    // Calling resumeAllocating does the Right Thing depending on whether this is the end of a
    // collection cycle or this is just a concurrent phase within a collection cycle:
    // - At end of collection cycle: it's a no-op because prepareForAllocation already cleared the
    //   last active block.
    // - During collection cycle: it reinstates the last active block.
    m_objectSpace.resumeAllocating();

    m_barriersExecuted = 0;
    
    if (!m_worldIsStopped) {
        dataLog("Fatal: collector does not believe that the world is stopped.\n");
        RELEASE_ASSERT_NOT_REACHED();
    }
    m_worldIsStopped = false;
}

bool Heap::stopTheMutator()
{
    for (;;) {
        unsigned oldState = m_worldState.load();
        if (oldState & stoppedBit) {
            RELEASE_ASSERT(!(oldState & hasAccessBit));
            RELEASE_ASSERT(!(oldState & mutatorWaitingBit));
            RELEASE_ASSERT(!(oldState & mutatorHasConnBit));
            return true;
        }
        
        if (oldState & mutatorHasConnBit) {
            RELEASE_ASSERT(!(oldState & hasAccessBit));
            RELEASE_ASSERT(!(oldState & stoppedBit));
            return false;
        }

        if (!(oldState & hasAccessBit)) {
            RELEASE_ASSERT(!(oldState & mutatorHasConnBit));
            RELEASE_ASSERT(!(oldState & mutatorWaitingBit));
            // We can stop the world instantly.
            if (m_worldState.compareExchangeWeak(oldState, oldState | stoppedBit))
                return true;
            continue;
        }
        
        // Transfer the conn to the mutator and bail.
        RELEASE_ASSERT(oldState & hasAccessBit);
        RELEASE_ASSERT(!(oldState & stoppedBit));
        unsigned newState = (oldState | mutatorHasConnBit) & ~mutatorWaitingBit;
        if (m_worldState.compareExchangeWeak(oldState, newState)) {
            dataLogLnIf(HeapInternal::verbose, "Handed off the conn.");
            m_stopIfNecessaryTimer->scheduleSoon();
            ParkingLot::unparkAll(&m_worldState);
            return false;
        }
    }
}

NEVER_INLINE void Heap::resumeTheMutator()
{
    dataLogLnIf(HeapInternal::verbose, "Resuming the mutator.");
    for (;;) {
        unsigned oldState = m_worldState.load();
        if (!!(oldState & hasAccessBit) != !(oldState & stoppedBit)) {
            dataLog("Fatal: hasAccess = ", !!(oldState & hasAccessBit), ", stopped = ", !!(oldState & stoppedBit), "\n");
            RELEASE_ASSERT_NOT_REACHED();
        }
        if (oldState & mutatorHasConnBit) {
            dataLog("Fatal: mutator has the conn.\n");
            RELEASE_ASSERT_NOT_REACHED();
        }
        
        if (!(oldState & stoppedBit)) {
            dataLogLnIf(HeapInternal::verbose, "Returning because not stopped.");
            return;
        }
        
        if (m_worldState.compareExchangeWeak(oldState, oldState & ~stoppedBit)) {
            dataLogLnIf(HeapInternal::verbose, "CASing and returning.");
            ParkingLot::unparkAll(&m_worldState);
            return;
        }
    }
}

void Heap::stopIfNecessarySlow()
{
    if constexpr (validateDFGDoesGC)
        vm().verifyCanGC();

    while (stopIfNecessarySlow(m_worldState.load())) { }
    
    RELEASE_ASSERT(m_worldState.load() & hasAccessBit);
    RELEASE_ASSERT(!(m_worldState.load() & stoppedBit));
    
    handleNeedCollectionEpilogue();
    m_mutatorDidRun = true;
}

bool Heap::stopIfNecessarySlow(unsigned oldState)
{
    if constexpr (validateDFGDoesGC)
        vm().verifyCanGC();

    RELEASE_ASSERT(oldState & hasAccessBit);
    RELEASE_ASSERT(!(oldState & stoppedBit));
    
    // It's possible for us to wake up with the epilogue already requested but the world not yet
    // resumed. If that happens, we can't run the epilogue yet.
    if (handleNeedCollectionEpilogue(oldState))
        return true;

    // FIXME: When entering the concurrent phase, we could arrange for this branch not to fire, and then
    // have the SlotVisitor do things to the m_worldState to make this branch fire again. That would
    // prevent us from polling this so much. Ideally, stopIfNecessary would ignore the mutatorHasConnBit
    // and there would be some other bit indicating whether we were in some GC phase other than the
    // NotRunning or Concurrent ones.
    if (oldState & mutatorHasConnBit)
        collectInMutatorThread();
    
    return false;
}

NEVER_INLINE void Heap::collectInMutatorThread()
{
    CollectingScope collectingScope(*this);
    for (;;) {
        Collector::RunCurrentPhaseResult result = m_collector->runCurrentPhase(GCConductor::Mutator, nullptr);
        switch (result) {
        case Collector::RunCurrentPhaseResult::Finished:
            return;
        case Collector::RunCurrentPhaseResult::Continue:
            break;
        case Collector::RunCurrentPhaseResult::NeedCurrentThreadState:
            sanitizeStackForVM(vm());
            auto lambda = [&] (CurrentThreadState& state) {
                for (;;) {
                    Collector::RunCurrentPhaseResult result = m_collector->runCurrentPhase(GCConductor::Mutator, &state);
                    switch (result) {
                    case Collector::RunCurrentPhaseResult::Finished:
                        return;
                    case Collector::RunCurrentPhaseResult::Continue:
                        break;
                    case Collector::RunCurrentPhaseResult::NeedCurrentThreadState:
                        RELEASE_ASSERT_NOT_REACHED();
                        break;
                    }
                }
            };
            callWithCurrentThreadState(lambda);
            return;
        }
    }
}

template<typename Func>
void Heap::waitForCollector(const Func& func)
{
    for (;;) {
        bool done;
        {
            Locker locker { *m_collector->m_threadLock };
            done = func(locker);
            if (!done) {
                setMutatorWaiting();
                
                // At this point, the collector knows that we intend to wait, and he will clear the
                // waiting bit and then unparkAll when the GC cycle finishes. Clearing the bit
                // prevents us from parking except if there is also stop-the-world. Unparking after
                // clearing means that if the clearing happens after we park, then we will unpark.
            }
        }
        
        // If we're in a stop-the-world scenario, we need to wait for that even if done is true.
        unsigned oldState = m_worldState.load();
        if (stopIfNecessarySlow(oldState))
            continue;
        
        m_mutatorDidRun = true;
        // FIXME: We wouldn't need this if stopIfNecessarySlow() had a mode where it knew to just
        // do the collection.
        relinquishConn();

        if (done) {
            clearMutatorWaiting(); // Clean up just in case.
            return;
        }
        
        // If mutatorWaitingBit is still set then we want to wait.
        ParkingLot::compareAndPark(&m_worldState, oldState | mutatorWaitingBit);
    }
}

void Heap::acquireAccessSlow()
{
    for (;;) {
        unsigned oldState = m_worldState.load();
        RELEASE_ASSERT(!(oldState & hasAccessBit));
        
        if (oldState & stoppedBit) {
            if (HeapInternal::verboseStop) {
                dataLogLn("Stopping in acquireAccess!");
                WTFReportBacktrace();
            }
            // Wait until we're not stopped anymore.
            ParkingLot::compareAndPark(&m_worldState, oldState);
            continue;
        }
        
        RELEASE_ASSERT(!(oldState & stoppedBit));
        unsigned newState = oldState | hasAccessBit;
        if (m_worldState.compareExchangeWeak(oldState, newState)) {
            handleNeedCollectionEpilogue();
            m_mutatorDidRun = true;
            stopIfNecessary();
            return;
        }
    }
}

void Heap::releaseAccessSlow()
{
    for (;;) {
        unsigned oldState = m_worldState.load();
        if (!(oldState & hasAccessBit)) {
            dataLog("FATAL: Attempting to release access but the mutator does not have access.\n");
            RELEASE_ASSERT_NOT_REACHED();
        }
        if (oldState & stoppedBit) {
            dataLog("FATAL: Attempting to release access but the mutator is stopped.\n");
            RELEASE_ASSERT_NOT_REACHED();
        }
        
        if (handleNeedCollectionEpilogue(oldState))
            continue;
        
        unsigned newState = oldState & ~(hasAccessBit | mutatorHasConnBit);
        
        if ((oldState & mutatorHasConnBit)
            && m_collector->m_nextPhase != m_collector->m_currentPhase) {
            // This means that the collector thread had given us the conn so that we would do something
            // for it. Stop ourselves as we release access. This ensures that acquireAccess blocks. In
            // the meantime, since we're handing the conn over, the collector will be awoken and it is
            // sure to have work to do.
            newState |= stoppedBit;
        }

        if (m_worldState.compareExchangeWeak(oldState, newState)) {
            if (oldState & mutatorHasConnBit)
                finishRelinquishingConn();
            return;
        }
    }
}

bool Heap::relinquishConn(unsigned oldState)
{
    RELEASE_ASSERT(oldState & hasAccessBit);
    RELEASE_ASSERT(!(oldState & stoppedBit));
    
    if (!(oldState & mutatorHasConnBit))
        return false; // Done.
    
    if (m_collector->m_threadShouldStop)
        return false;
    
    if (!m_worldState.compareExchangeWeak(oldState, oldState & ~mutatorHasConnBit))
        return true; // Loop around.
    
    finishRelinquishingConn();
    return true;
}

void Heap::finishRelinquishingConn()
{
    dataLogLnIf(HeapInternal::verbose, "Relinquished the conn.");
    
    sanitizeStackForVM(vm());
    
    Locker locker { *m_collector->m_threadLock };
    if (!m_collector->m_requests.isEmpty())
        m_collector->m_threadCondition->notifyOne(locker);
    ParkingLot::unparkAll(&m_worldState);
}

void Heap::relinquishConn()
{
    while (relinquishConn(m_worldState.load())) { }
}

NEVER_INLINE bool Heap::handleNeedCollectionEpilogue(unsigned oldState)
{
    RELEASE_ASSERT(oldState & hasAccessBit);
    RELEASE_ASSERT(!(oldState & stoppedBit));
    
    if (!(oldState & needCollectionEpilogueBit))
        return false;
    if (m_worldState.compareExchangeWeak(oldState, oldState & ~needCollectionEpilogueBit)) {
        runCollectionEpilogue();
        // Wake up anyone waiting for us to run the epilogue. Note that they may have woken up already, in
        // which case they would be waiting for us to release heap access.
        ParkingLot::unparkAll(&m_worldState);
        return true;
    }
    return true;
}

void Heap::handleNeedCollectionEpilogue()
{
    while (handleNeedCollectionEpilogue(m_worldState.load())) { }
}

void Heap::setNeedCollectionEpilogue()
{
    m_worldState.exchangeOr(needCollectionEpilogueBit);
    ParkingLot::unparkAll(&m_worldState);
    m_stopIfNecessaryTimer->scheduleSoon();
}

void Heap::waitWhileNeedCollectionEpilogue()
{
    for (;;) {
        unsigned oldState = m_worldState.load();
        if (!(oldState & needCollectionEpilogueBit)) {
            // This means that either there was no epilogue request or the main thread will run it
            // with heap access, so a subsequent call to stopTheWorld() will return only when
            // the epilogue finishes.
            return;
        }
        ParkingLot::compareAndPark(&m_worldState, oldState);
    }
}

void Heap::setMutatorWaiting()
{
    m_worldState.exchangeOr(mutatorWaitingBit);
}

void Heap::clearMutatorWaiting()
{
    m_worldState.exchangeAnd(~mutatorWaitingBit);
}

void Heap::notifyThreadStopping(const AbstractLocker&)
{
    clearMutatorWaiting();
    ParkingLot::unparkAll(&m_worldState);
}

void Heap::runCollectionEpilogue()
{
    MonotonicTime before;
    if (Options::logGC()) [[unlikely]] {
        before = MonotonicTime::now();
        dataLog("[GC<", RawPointer(this), ">: epilogue ");
    }
    
    {
        SweepingScope sweepingScope(*this);
        deleteSourceProviderCaches();
        sweepEagerlyInEpilogue();
    }
#if ENABLE(WEBASSEMBLY)
    Wasm::TypeInformation::cleanupIfRequested();
#endif
    
    if (HasOwnPropertyCache* cache = vm().hasOwnPropertyCache())
        cache->clear();
    if (auto* cache = vm().megamorphicCache())
        cache->age(m_lastCollectionScope && m_lastCollectionScope.value() == CollectionScope::Full ? CollectionScope::Full : CollectionScope::Eden);

    if (m_lastCollectionScope && m_lastCollectionScope.value() == CollectionScope::Full) {
        vm().jsonAtomStringCache.clear();
        vm().numericStrings.clearOnGarbageCollection();
        vm().stringReplaceCache.clear();
    }
    vm().keyAtomStringCache.clear();
    if (auto* cache = vm().stringSplitCache())
        cache->clear();
    vm().jsonAtomStringCache.clearJSStrings();

    m_possiblyAccessedStringsFromConcurrentThreadsOrGCOwnedDataScope.removeAllMatching([&](const auto& iter) {
        return !m_discoveredAccessedStringsFromGCOwnedDataScope.contains(iter.first);
    });
    m_discoveredAccessedStringsFromGCOwnedDataScope.clear();

    immutableButterflyToStringCache.clear();
    
    for (const GCCompletionCallback& callback : m_gcCompletionCallbacks)
        callback.run(vm());
    
    if (shouldSweepSynchronously())
        sweepSynchronously();

    if (Options::logGC()) [[unlikely]] {
        MonotonicTime after = MonotonicTime::now();
        dataLog((after - before).milliseconds(), "ms]\n");
    }
}

GCRequest::Ticket Heap::requestCollection(GCRequest request)
{
    stopIfNecessary();
    
    ASSERT(vm().currentThreadIsHoldingAPILock());
    RELEASE_ASSERT(vm().atomStringTable() == Thread::currentSingleton().atomStringTable());

    return m_collector->requestCollection(request);
}

void Heap::waitForCollection(GCRequest::Ticket ticket)
{
    waitForCollector(
        [&] (const AbstractLocker&) -> bool {
            return m_collector->hasServedTicket(ticket);
        });
}

void Heap::sweepEagerlyInEpilogue()
{
    m_objectSpace.sweepPreciseAllocations();
#if ENABLE(WEBASSEMBLY)
    // We hold onto a lot of memory, so it makes a lot of sense to be swept eagerly.
    if (m_webAssemblyMemorySpace)
        m_webAssemblyMemorySpace->sweep();
#endif
}

void Heap::willStartCollection()
{
    if (m_collectionScope) {
        dataLogLn("Collection scope already set during GC: ", *m_collectionScope);
        RELEASE_ASSERT_NOT_REACHED();
    }

    ++m_gcVersion;
    if (Options::verifyGC()) [[unlikely]] {
        m_verifierSlotVisitor = makeUnique<VerifierSlotVisitor>(*this, *m_collector);
        ASSERT(!m_isMarkingForGCVerifier);
    }

    dataLogIf(Options::logGC(), "=> ");
    
    if (shouldDoFullCollection()) {
        m_collectionScope = CollectionScope::Full;
        m_shouldDoFullCollection = false;
        dataLogIf(Options::logGC(), "FullCollection, ");
    } else {
        m_collectionScope = CollectionScope::Eden;
        dataLogIf(Options::logGC(), "EdenCollection, ");
    }
    if (m_collectionScope.value() == CollectionScope::Full) {
        m_sizeBeforeLastFullCollect = m_sizeAfterLastCollect + totalBytesAllocatedThisCycle();
        m_extraMemorySize = 0;
        m_deprecatedExtraMemorySize = 0;
#if ENABLE(RESOURCE_USAGE)
        m_externalMemorySize = 0;
#endif
        m_shouldDoOpportunisticFullCollection = false;
        if (m_fullActivityCallback)
            m_fullActivityCallback->willCollect();
    } else {
        ASSERT(m_collectionScope && m_collectionScope.value() == CollectionScope::Eden);
        m_sizeBeforeLastEdenCollect = m_sizeAfterLastCollect + totalBytesAllocatedThisCycle();
    }

    if (m_edenActivityCallback)
        m_edenActivityCallback->willCollect();

    for (auto* observer : m_observers)
        observer->willGarbageCollect();
}

void Heap::prepareForMarking()
{
    m_objectSpace.prepareForMarking();
}

void Heap::cancelDeferredWorkIfNeeded()
{
    vm().deferredWorkTimer->cancelPendingWork(vm());
}

void Heap::reapWeakHandles()
{
    m_objectSpace.reapWeakSets();
}

void Heap::reconcileWeakGCHashTables()
{
    CollectionScope collectionScope = m_collectionScope.value_or(CollectionScope::Full);
    if (collectionScope == CollectionScope::Full) {
        for (auto* weakGCHashTable : m_weakGCHashTables)
            weakGCHashTable->reconcileWeakReferencesAtGCEnd(vm(), collectionScope);
        m_dirtyWeakGCHashTables.forEach([](WeakGCHashTable* weakGCHashTable) {
            weakGCHashTable->remove();
        });
        return;
    }

    // Only a table that gained an entry since the last collection can hold an entry that dies here:
    // everything that survived that collection is old, and an eden collection cannot free it.
    m_dirtyWeakGCHashTables.forEach([&](WeakGCHashTable* weakGCHashTable) {
        weakGCHashTable->remove();
        weakGCHashTable->reconcileWeakReferencesAtGCEnd(vm(), collectionScope);
    });
}

void Heap::sweepArrayBuffers()
{
    m_arrayBuffers.sweep(vm(), collectionScope().value_or(CollectionScope::Eden));
}

void Heap::snapshotUnswept()
{
    TimingScope timingScope(*this, "Heap::snapshotUnswept"_s);
    m_objectSpace.snapshotUnswept();
}

void Heap::deleteSourceProviderCaches()
{
    if (m_lastCollectionScope && m_lastCollectionScope.value() == CollectionScope::Full)
        vm().clearSourceProviderCaches();
}

double Heap::projectedGCRateLimitingValue(MonotonicTime now)
{
    if (!m_lastGCEndTime) {
        ASSERT(!m_gcRateLimitingValue);
        return 0.0;
    }
    Seconds timeSinceLastGC = now - m_lastGCEndTime;
    return m_gcRateLimitingValue * pow(0.5, timeSinceLastGC.milliseconds() / Options::gcRateLimitingHalfLifeInMS());
}

void Heap::updateAllocationLimits()
{
    constexpr bool verbose = false;
    
    dataLogLnIf(verbose, "\nnonOversizedBytesAllocatedThisCycle = ", m_nonOversizedBytesAllocatedThisCycle, ", oversizedBytesAllocatedThisCycle", m_oversizedBytesAllocatedThisCycle);
    
    // Calculate our current heap size threshold for the purpose of figuring out when we should
    // run another collection. This isn't the same as either size() or capacity(), though it should
    // be somewhere between the two. The key is to match the size calculations involved calls to
    // didAllocate(), while never dangerously underestimating capacity(). In extreme cases of
    // fragmentation, we may have size() much smaller than capacity().
    size_t currentHeapSize = 0;

    // For marked space, we use the total number of bytes visited. This matches the logic for
    // BlockDirectory's calls to didAllocate(), which effectively accounts for the total size of
    // objects allocated rather than blocks used. This will underestimate capacity(), and in case
    // of fragmentation, this may be substantial. Fortunately, marked space rarely fragments because
    // cells usually have a narrow range of sizes. So, the underestimation is probably OK.
    currentHeapSize += m_totalBytesVisited;
    dataLogLnIf(verbose, "totalBytesVisited = ", m_totalBytesVisited, ", currentHeapSize = ", currentHeapSize);

    // It's up to the user to ensure that extraMemorySize() ends up corresponding to allocation-time
    // extra memory reporting.
    auto computedExtraMemorySize = extraMemorySize();
    currentHeapSize += computedExtraMemorySize;
    if (ASSERT_ENABLED) {
        CheckedSize checkedCurrentHeapSize = m_totalBytesVisited;
        checkedCurrentHeapSize += computedExtraMemorySize;
        ASSERT(!checkedCurrentHeapSize.hasOverflowed() && checkedCurrentHeapSize == currentHeapSize);
    }

    dataLogLnIf(verbose, "extraMemorySize() = ", computedExtraMemorySize, ", currentHeapSize = ", currentHeapSize);

    // Get critical memory threshold for next cycle.
    bool isCritical = overCriticalMemoryThreshold(MemoryThresholdCallType::Direct);

    if (m_collectionScope && m_collectionScope.value() == CollectionScope::Full) {
        // To avoid pathological GC churn in very small and very large heaps, we set
        // the new allocation limit based on the current size of the heap, with a
        // fixed minimum.
        size_t lastMaxHeapSize = m_maxHeapSize;
        m_maxHeapSize = std::max(m_minBytesPerCycle, proportionalHeapSize(currentHeapSize, m_growthMode, m_ramSize));
        m_maxEdenSize = m_maxHeapSize - currentHeapSize;
        if (m_isInOpportunisticTask && !isCritical) {
            // After an Opportunistic Full GC, we allow eden to occupy all the space we recovered.
            // In this case, m_maxHeapSize may be larger than currentHeapSize + m_maxEdenSize.
            // Note that m_maxEdenSize is still used when we increase m_maxHeapSize after an
            // Eden GC to ensure that eden can grow to at least m_maxHeapSize.
            m_maxHeapSize = std::max(m_maxHeapSize, lastMaxHeapSize);
        }
        dataLogLnIf(verbose, "Full: maxHeapSize = ", m_maxHeapSize);
        dataLogLnIf(verbose, "Full: maxEdenSize = ", m_maxEdenSize);
        m_sizeAfterLastFullCollect = currentHeapSize;
        dataLogLnIf(verbose, "Full: sizeAfterLastFullCollect = ", currentHeapSize);
        m_bytesAbandonedSinceLastFullCollect = 0;
        dataLogLnIf(verbose, "Full: bytesAbandonedSinceLastFullCollect = ", 0);
    } else {
        ASSERT(currentHeapSize >= m_sizeAfterLastCollect);
        // Theoretically, we shouldn't ever scan more memory than the heap size we planned to have.
        // But we are sloppy, so we have to defend against the overflow.
        size_t remainingHeapSize = currentHeapSize > m_maxHeapSize ? 0 : m_maxHeapSize - currentHeapSize;
        dataLogLnIf(verbose, "Eden: remainingHeapSize = ", remainingHeapSize);
        m_sizeAfterLastEdenCollect = currentHeapSize;
        dataLogLnIf(verbose, "Eden: sizeAfterLastEdenCollect = ", currentHeapSize);
        double edenToOldGenerationRatio = (double)remainingHeapSize / (double)m_maxHeapSize;
        double minEdenToOldGenerationRatio = 1.0 / 3.0;
        if (edenToOldGenerationRatio < minEdenToOldGenerationRatio)
            m_shouldDoFullCollection = true;
        m_maxHeapSize = std::max(m_maxHeapSize, currentHeapSize + m_maxEdenSize);
        dataLogLnIf(verbose, "Eden: maxHeapSize = ", m_maxHeapSize);
        dataLogLnIf(verbose, "Eden: maxEdenSize = ", m_maxEdenSize);
        if (m_fullActivityCallback) {
            ASSERT(currentHeapSize >= m_sizeAfterLastFullCollect);
            m_fullActivityCallback->didAllocate(*this, currentHeapSize - m_sizeAfterLastFullCollect);
        }
    }

    m_sizeAfterLastCollect = currentHeapSize;
    dataLogLnIf(verbose, "sizeAfterLastCollect = ", m_sizeAfterLastCollect);
    m_nonOversizedBytesAllocatedThisCycle = 0;
    m_oversizedBytesAllocatedThisCycle = 0;
    m_lastOversidedAllocationThisCycle = 0;

    dataLogIf(Options::logGC(), "=> ", currentHeapSize / 1024, "kb, ");
}

void Heap::didFinishCollection(Seconds duration)
{
    CollectionScope scope = *m_collectionScope;
    if (scope == CollectionScope::Full)
        m_lastFullGCLength = duration;
    else
        m_lastEdenGCLength = duration;

#if ENABLE(RESOURCE_USAGE)
    ASSERT(externalMemorySize() <= extraMemorySize());
#endif

    if (HeapProfiler* heapProfiler = vm().heapProfiler()) {
        gatherExtraHeapData(*heapProfiler);
        removeDeadHeapSnapshotNodes(*heapProfiler);
    }

    if (m_verifier) [[unlikely]]
        m_verifier->endGC();

    RELEASE_ASSERT(m_collectionScope);
    m_lastCollectionScope = m_collectionScope;
    m_collectionScope = std::nullopt;

    for (auto* observer : m_observers)
        observer->didGarbageCollect(scope);
}

GCActivityCallback* Heap::fullActivityCallback()
{
    return m_fullActivityCallback.get();
}

GCActivityCallback* Heap::edenActivityCallback()
{
    return m_edenActivityCallback.get();
}

void Heap::setGarbageCollectionTimerEnabled(bool enable)
{
    if (m_fullActivityCallback)
        m_fullActivityCallback->setEnabled(enable);
    if (m_edenActivityCallback)
        m_edenActivityCallback->setEnabled(enable);
}

constexpr size_t oversizedAllocationThreshold = 64 * KB;
void Heap::didAllocate(size_t bytes)
{
    if (bytes >= oversizedAllocationThreshold) {
        m_oversizedBytesAllocatedThisCycle += bytes;
        m_lastOversidedAllocationThisCycle = bytes;
    } else
        m_nonOversizedBytesAllocatedThisCycle += bytes;

    // totalBytesAllocatedThisCycle() depends on values updated above.
    // So, only do this m_edenActivityCallback after updating those values.
    if (m_edenActivityCallback)
        m_edenActivityCallback->didAllocate(*this, totalBytesAllocatedThisCycle() + m_bytesAbandonedSinceLastFullCollect);

    performIncrement(bytes);
}

void Heap::addFinalizer(JSCell* cell, CFinalizer finalizer)
{
    WeakSet::allocate(cell, &m_cFinalizerOwner, std::bit_cast<void*>(finalizer)); // Balanced by CFinalizerOwner::finalize().
}

void Heap::addFinalizer(JSCell* cell, LambdaFinalizer function)
{
    WeakSet::allocate(cell, &m_lambdaFinalizerOwner, function.leak()); // Balanced by LambdaFinalizerOwner::finalize().
}

void Heap::CFinalizerOwner::finalize(Handle<Unknown> handle, void* context)
{
    HandleSlot slot = handle.slot();
    CFinalizer finalizer = std::bit_cast<CFinalizer>(context);
    finalizer(slot->asCell());
    WeakSet::deallocate(WeakImpl::asWeakImpl(slot));
}

void Heap::LambdaFinalizerOwner::finalize(Handle<Unknown> handle, void* context)
{
    auto finalizer = WTF::adopt(static_cast<LambdaFinalizer::Impl*>(context));
    HandleSlot slot = handle.slot();
    finalizer(slot->asCell());
    WeakSet::deallocate(WeakImpl::asWeakImpl(slot));
}

void Heap::collectNowFullIfNotDoneRecently(Synchronousness synchronousness)
{
    if (!m_fullActivityCallback) {
        collectNow(synchronousness, CollectionScope::Full);
        return;
    }

    if (m_fullActivityCallback->didGCRecently()) {
        // A synchronous GC was already requested recently so we merely accelerate next collection.
        reportAbandonedObjectGraph();
        return;
    }

    m_fullActivityCallback->setDidGCRecently(true);
    collectNow(synchronousness, CollectionScope::Full);
}

void Heap::setFullActivityCallback(RefPtr<GCActivityCallback>&& callback)
{
    m_fullActivityCallback = WTF::move(callback);
}

void Heap::setEdenActivityCallback(RefPtr<GCActivityCallback>&& callback)
{
    m_edenActivityCallback = WTF::move(callback);
}

void Heap::disableStopIfNecessaryTimer()
{
    m_stopIfNecessaryTimer->disable();
}

bool Heap::useGenerationalGC()
{
    return Options::useGenerationalGC() && !VM::isInMiniMode();
}

bool Heap::shouldSweepSynchronously()
{
    // updateAllocationLimits() updates info that overCriticalMemoryThreshold() needs.
    return overCriticalMemoryThreshold() || Options::sweepSynchronously() || VM::isInMiniMode();
}

bool Heap::shouldDoFullCollection()
{
    if (!useGenerationalGC())
        return true;

    if (!m_collector->m_currentRequest.scope)
        return m_shouldDoFullCollection || overCriticalMemoryThreshold();
    return *m_collector->m_currentRequest.scope == CollectionScope::Full;
}

void Heap::addDetachedWeakBlock(WeakBlock* block)
{
    RELEASE_ASSERT(!block->next() && !block->prev());
    ASSERT(&block->heap() == this);
    block->setDetached();
    m_detachedWeakBlocks.append(block);
}

void Heap::releaseDetachedWeakBlock(WeakBlock* block)
{
    ASSERT(&block->heap() == this);
    m_detachedWeakBlocks.remove(block);
    returnWeakBlockToPool(block);
}

void Heap::returnWeakBlockToPool(WeakBlock* block)
{
    RELEASE_ASSERT(!block->next() && !block->prev());
    ASSERT(&block->heap() == this);
    ASSERT(block->isEmpty());

    if (m_pooledWeakBlockCount >= maxPooledWeakBlocks()) {
        WeakBlock::destroy(*this, block);
        return;
    }

    block->setPooled();
    m_pooledWeakBlocks.push(block);
    ++m_pooledWeakBlockCount;
}

unsigned Heap::maxPooledWeakBlocks()
{
    unsigned divisor = Options::weakBlockPoolDivisor();
    if (!divisor)
        return 0;

    // One spare per divisor MarkedBlocks, so a bigger heap keeps a proportionally bigger cache.
    // The floor covers a heap too small for the ratio to name anything; the ceiling keeps the
    // cache from becoming a memory sink in its own right.
    constexpr unsigned minPooledWeakBlocks = 8;
    constexpr unsigned maxPooledWeakBlocksEver = 1024;
    size_t pooled = m_objectSpace.capacity() / (MarkedBlock::blockSize * static_cast<size_t>(divisor));
    return clampTo<unsigned>(pooled, minPooledWeakBlocks, maxPooledWeakBlocksEver);
}

WeakBlock* Heap::takeWeakBlockFromPool()
{
    WeakBlock* block = m_pooledWeakBlocks.removeHead();
    if (block)
        --m_pooledWeakBlockCount;
    return block;
}

void Heap::destroyAllPooledWeakBlocks()
{
    while (WeakBlock* block = m_pooledWeakBlocks.removeHead())
        WeakBlock::destroy(*this, block);
    m_pooledWeakBlockCount = 0;
}

void Heap::forEachCodeBlockImpl(const ScopedLambda<void(CodeBlock*)>& func)
{
    // We don't know the full set of CodeBlocks until compilation has terminated.
    completeAllJITPlans();

    return m_codeBlocks->iterate(func);
}

void Heap::forEachCodeBlockIgnoringJITPlansImpl(const AbstractLocker& locker, const ScopedLambda<void(CodeBlock*)>& func)
{
    return m_codeBlocks->iterate(locker, func);
}

void Heap::writeBarrierSlowPath(const JSCell* from)
{
    if (mutatorShouldBeFenced()) [[unlikely]] {
        // In this case, the barrierThreshold is the tautological threshold, so from could still be
        // not black. But we can't know for sure until we fire off a fence.
        WTF::storeLoadFence();
        if (from->cellState() != CellState::PossiblyBlack)
            return;
    }
    
    addToRememberedSet(from);
}

bool Heap::currentThreadIsDoingGCWork()
{
    return Thread::mayBeGCThread() || mutatorState() != MutatorState::Running;
}

void Heap::reportExtraMemoryVisited(size_t size)
{
    size_t* counter = &m_extraMemorySize;
    
    for (;;) {
        size_t oldSize = *counter;
        // FIXME: Change this to use SaturatingArithmetic when available.
        // https://bugs.webkit.org/show_bug.cgi?id=170411
        CheckedSize checkedNewSize = oldSize;
        checkedNewSize += size;
        size_t newSize = std::numeric_limits<size_t>::max();
        if (!checkedNewSize.hasOverflowed()) [[likely]]
            newSize = checkedNewSize.value();
        if (WTF::atomicCompareExchangeWeakRelaxed(counter, oldSize, newSize))
            return;
    }
}

#if ENABLE(RESOURCE_USAGE)
void Heap::reportExternalMemoryVisited(size_t size)
{
    size_t* counter = &m_externalMemorySize;

    for (;;) {
        size_t oldSize = *counter;
        if (WTF::atomicCompareExchangeWeakRelaxed(counter, oldSize, oldSize + size))
            return;
    }
}
#endif

void Heap::collectIfNecessaryOrDefer(GCDeferralContext* deferralContext)
{
    ASSERT(deferralContext || isDeferred() || !AssertNoGC::isInEffectOnCurrentThread());
    if constexpr (validateDFGDoesGC)
        vm().verifyCanGC();

    if (!m_isSafeToCollect)
        return;

    switch (mutatorState()) {
    case MutatorState::Running:
    case MutatorState::Allocating:
        break;
    case MutatorState::Sweeping:
    case MutatorState::Collecting:
        return;
    }
    if (!Options::useGC()) [[unlikely]]
        return;
    
    if (mayNeedToStop()) {
        if (deferralContext)
            deferralContext->m_shouldGC = true;
        else if (isDeferred())
            m_didDeferGCWork = true;
        else
            stopIfNecessary();
    }
    
    auto shouldRequestGC = [&] () -> bool {
        bool logRequestGC = false;
        // Don't log if we already have a request pending or if we have to come back later so we don't flood dataFile.
        if (Options::logGC()) [[unlikely]]
            logRequestGC = m_collector->m_requests.isEmpty() && !deferralContext && !isDeferred();
        if (Options::gcMaxHeapSize()) [[unlikely]] {
            size_t bytesAllocatedThisCycle = totalBytesAllocatedThisCycle();
            if (bytesAllocatedThisCycle <= Options::gcMaxHeapSize())
                return false;
            dataLogLnIf(logRequestGC, "Requesting GC because bytes allocated this cycle: ", bytesAllocatedThisCycle, " exceed Options::gcMaxHeapSize(): ", Options::gcMaxHeapSize());
            return true;
        }

        ASSERT(m_maxHeapSize > m_sizeAfterLastCollect);
        size_t bytesAllowedThisCycle = m_maxHeapSize - m_sizeAfterLastCollect;

        bool isCritical = overCriticalMemoryThreshold();
        if (isCritical)
            bytesAllowedThisCycle = std::min(m_maxEdenSizeWhenCritical, bytesAllowedThisCycle);

        size_t bytesAllocatedThisCycle = totalBytesAllocatedThisCycle();
        if (bytesAllocatedThisCycle <= bytesAllowedThisCycle)
            return false;
        if (bytesAllocatedThisCycle < m_maxEdenSizeForRateLimiting) {
            if (projectedGCRateLimitingValue(MonotonicTime::now()) > 1.0)
                return false;
        }

        // We don't want to GC if the last oversized allocation makes up too much of the memory allocated this cycle since it's likely
        //  that object is still live and doesn't give us much indication about how much memory we could actually reclaim. That said,
        // if the system is cricital or we have a small heap we want to be very agressive about reclaiming memory to reduce overall
        // pressure on the system.
        if (!isCritical && m_heapType == HeapType::Large) {
            if (static_cast<double>(m_lastOversidedAllocationThisCycle) / bytesAllocatedThisCycle > 1.0 / 3.0)
                return false;
        }

        dataLogLnIf(logRequestGC, "Requesting GC because bytes allocated this cycle: ", bytesAllocatedThisCycle, " exceed bytes allowed: ", bytesAllowedThisCycle, ConditionalDump(isCritical, " (critical)"), " normal bytes: ", m_nonOversizedBytesAllocatedThisCycle, " oversized bytes: ", m_oversizedBytesAllocatedThisCycle, " last oversized: ", m_lastOversidedAllocationThisCycle);
        return true;
    };
    if (!shouldRequestGC())
        return;

    if (deferralContext)
        deferralContext->m_shouldGC = true;
    else if (isDeferred())
        m_didDeferGCWork = true;
    else {
        collectAsync();
        stopIfNecessary(); // This will immediately start the collection if we have the conn.
    }
}

void Heap::decrementDeferralDepthAndGCIfNeededSlow()
{
    // Can't do anything if we're still deferred.
    if (m_deferralDepth)
        return;
    
    ASSERT(!isDeferred());
    
    m_didDeferGCWork = false;
    // FIXME: Bring back something like the DeferGCProbability mode.
    // https://bugs.webkit.org/show_bug.cgi?id=166627
    collectIfNecessaryOrDefer();
}

void Heap::registerWeakGCHashTable(WeakGCHashTable* weakGCHashTable)
{
    m_weakGCHashTables.add(weakGCHashTable);
}

void Heap::unregisterWeakGCHashTable(WeakGCHashTable* weakGCHashTable)
{
    if (weakGCHashTable->isOnList())
        weakGCHashTable->remove();
    m_weakGCHashTables.remove(weakGCHashTable);
}

void Heap::addDirtyWeakGCHashTable(WeakGCHashTable* weakGCHashTable)
{
    ASSERT(!weakGCHashTable->isOnList());
    m_dirtyWeakGCHashTables.append(weakGCHashTable);
}

void WeakGCHashTable::addToDirtyList(VM& vm)
{
    vm.heap.addDirtyWeakGCHashTable(this);
}

void Heap::didAllocateBlock(size_t capacity)
{
#if ENABLE(RESOURCE_USAGE)
    m_blockBytesAllocated += capacity;
#else
    UNUSED_PARAM(capacity);
#endif
}

void Heap::didFreeBlock(size_t capacity)
{
#if ENABLE(RESOURCE_USAGE)
    m_blockBytesAllocated -= capacity;
#else
    UNUSED_PARAM(capacity);
#endif
}

#if ENABLE(SAMPLING_PROFILER)
constexpr bool samplingProfilerSupported = true;
template<typename Visitor>
static ALWAYS_INLINE void visitSamplingProfiler(VM& vm, Visitor& visitor)
{
    SamplingProfiler* samplingProfiler = vm.samplingProfiler();
    if (samplingProfiler) [[unlikely]] {
        Locker locker { samplingProfiler->getLock() };
        samplingProfiler->processUnverifiedStackTraces();
        samplingProfiler->visit(visitor);
        if (Options::logGC() == GCLogging::Verbose)
            dataLog("Sampling Profiler data:\n", visitor);
    }
};
#else
constexpr bool samplingProfilerSupported = false;
static UNUSED_FUNCTION void visitSamplingProfiler(VM&, AbstractSlotVisitor&) { };
#endif

void Heap::addCoreConstraints()
{
    m_constraintSet->add(
        "Cs"_s, "Conservative Scan"_s,
        MAKE_MARKING_CONSTRAINT_EXECUTOR_PAIR(([this, lastVersion = static_cast<uint64_t>(0)] (auto& visitor) mutable {
            bool shouldNotProduceWork = lastVersion == m_collector->m_phaseVersion;
            SuperSamplerScope superSamplerScope(false);

            // For the GC Verfier, we would like to use the identical set of conservative roots
            // as the real GC. Otherwise, the GC verifier may report false negatives due to
            // variations in stack values. For this same reason, we will skip this constraint
            // when we're running the GC verification in the End phase.
            if (shouldNotProduceWork || m_isMarkingForGCVerifier)
                return;
            
            TimingScope preConvergenceTimingScope(*this, "Constraint: conservative scan"_s);
            m_objectSpace.prepareForConservativeScan();
            m_jitStubRoutines->prepareForConservativeScan();

            {

                // We only want to do this when the mutator has the conn because that means we're under a safepoint.
                // If we tried to scan while not under a safepoint we could stop a thread that's in the process of calling
                // one of the callees we are looking for.
                // FIXME: Should we have two constraints for this? One for concurrent and one under safepoint at the bitter end.
                ASSERT(worldIsStopped());
                ConservativeRoots conservativeRoots(*this);

                gatherStackRoots(conservativeRoots);
                gatherVMRoots(conservativeRoots);

                SetRootMarkReasonScope rootScope(visitor, RootMarkReason::ConservativeScan);
                visitor.append(conservativeRoots);
                if (m_verifierSlotVisitor) [[unlikely]] {
                    SetRootMarkReasonScope rootScope(*m_verifierSlotVisitor, RootMarkReason::ConservativeScan);
                    m_verifierSlotVisitor->append(conservativeRoots);
                }
            }

            // JITStubRoutines must be visited after scanning ConservativeRoots since JITStubRoutines depend on the hook executed during gathering ConservativeRoots.
            SetRootMarkReasonScope rootScope(visitor, RootMarkReason::JITStubRoutines);
            m_jitStubRoutines->traceMarkedStubRoutines(visitor);
            if (m_verifierSlotVisitor) [[unlikely]] {
                // It's important to cast m_verifierSlotVisitor to an AbstractSlotVisitor here
                // so that we'll call the AbstractSlotVisitor version of traceMarkedStubRoutines().
                AbstractSlotVisitor& visitor = *m_verifierSlotVisitor;
                m_jitStubRoutines->traceMarkedStubRoutines(visitor);
            }
            lastVersion = m_collector->m_phaseVersion;
        })),
        ConstraintVolatility::GreyedByExecution);
    
    m_constraintSet->add(
        "Msr"_s, "Misc Small Roots"_s,
        MAKE_MARKING_CONSTRAINT_EXECUTOR_PAIR(([this] (auto& visitor) {
            VM& vm = this->vm();
#if JSC_OBJC_API_ENABLED
            {
                SetRootMarkReasonScope rootScope(visitor, RootMarkReason::ExternalRememberedSet);
                scanExternalRememberedSet(vm, visitor);
            }
#endif

            {
                SetRootMarkReasonScope rootScope(visitor, RootMarkReason::StrongReferences);
                if (vm.smallStrings.needsToBeVisited(*m_collectionScope))
                    vm.smallStrings.visitStrongReferences(visitor);
            }
            
            {
                SetRootMarkReasonScope rootScope(visitor, RootMarkReason::ProtectedValues);
                for (auto& pair : m_protectedValues)
                    visitor.appendUnbarriered(pair.key);
            }
            
            if (!m_markListSet.isEmpty()) {
                SetRootMarkReasonScope rootScope(visitor, RootMarkReason::ConservativeScan);
                MarkedVectorBase::markLists(visitor, m_markListSet);
            }

            {
                SetRootMarkReasonScope rootScope(visitor, RootMarkReason::VMExceptions);
                visitor.appendUnbarriered(vm.exception());
                visitor.appendUnbarriered(vm.lastException());

                // We're going to m_terminationException directly instead of going through
                // the exception() getter because we want to assert in the getter that the
                // TerminationException has been reified. Here, we don't care if it is
                // reified or not.
                visitor.appendUnbarriered(vm.m_terminationException);
            }
        })),
        ConstraintVolatility::GreyedByExecution);
    
    m_constraintSet->add(
        "Sh"_s, "Strong Handles"_s,
        MAKE_MARKING_CONSTRAINT_EXECUTOR_PAIR(([this] (auto& visitor) {
            SetRootMarkReasonScope rootScope(visitor, RootMarkReason::StrongHandles);
            m_strongSet.visitAggregate(visitor);
            vm().visitAggregate(visitor);
        })),
        ConstraintVolatility::GreyedByExecution);
    
    m_constraintSet->add(
        "D"_s, "Debugger"_s,
        MAKE_MARKING_CONSTRAINT_EXECUTOR_PAIR(([this] (auto& visitor) {
            SetRootMarkReasonScope rootScope(visitor, RootMarkReason::Debugger);

            VM& vm = this->vm();
            if constexpr (samplingProfilerSupported)
                visitSamplingProfiler(vm, visitor);

            if (vm.typeProfiler())
                vm.typeProfilerLog()->visit(visitor);
            
            if (auto* shadowChicken = vm.shadowChicken())
                shadowChicken->visitChildren(visitor);
        })),
        ConstraintVolatility::GreyedByExecution);
    
    m_constraintSet->add(
        "Ws"_s, "Weak Sets"_s,
        MAKE_MARKING_CONSTRAINT_EXECUTOR_PAIR(([this] (auto& visitor) {
            SetRootMarkReasonScope rootScope(visitor, RootMarkReason::WeakSets);
            RefPtr<SharedTask<void(decltype(visitor)&)>> task = m_objectSpace.forEachWeakInParallel<decltype(visitor)>(visitor);
            visitor.addParallelConstraintTask(WTF::move(task));
        })),
        ConstraintVolatility::GreyedByMarking,
        ConstraintParallelism::Parallel);
    
    m_constraintSet->add(
        "O"_s, "Output"_s,
        MAKE_MARKING_CONSTRAINT_EXECUTOR_PAIR(([] (auto& visitor) {
            JSC::Heap* heap = visitor.heap();

            auto callOutputConstraint = [] (auto& visitor, HeapCell* heapCell, HeapCell::Kind) {
                SetRootMarkReasonScope rootScope(visitor, RootMarkReason::Output);
                JSCell* cell = static_cast<JSCell*>(heapCell);
                cell->methodTable()->visitOutputConstraints(cell, visitor);
            };
            
            auto add = [&] (auto& set) {
                RefPtr<SharedTask<void(decltype(visitor)&)>> task = set.template forEachMarkedCellInParallel<decltype(visitor)>(callOutputConstraint);
                visitor.addParallelConstraintTask(WTF::move(task));
            };

            {
                SetRootMarkReasonScope rootScope(visitor, RootMarkReason::ExecutableToCodeBlockEdges);
                add(heap->functionExecutableSpaceAndSet.outputConstraintsSet);
                add(heap->programExecutableSpaceAndSet.outputConstraintsSet);
                if (heap->m_evalExecutableSpace)
                    add(heap->m_evalExecutableSpace->outputConstraintsSet);
                if (heap->m_moduleProgramExecutableSpace)
                    add(heap->m_moduleProgramExecutableSpace->outputConstraintsSet);
            }
            if (heap->m_weakMapSpace) {
                SetRootMarkReasonScope rootScope(visitor, RootMarkReason::WeakMapSpace);
                add(*heap->m_weakMapSpace);
            }
        })),
        ConstraintVolatility::GreyedByMarking,
        ConstraintParallelism::Parallel);

#if ENABLE(WEBASSEMBLY)
    m_constraintSet->add(
        "Pbc"_s, "Pinball Completions"_s,
        MAKE_MARKING_CONSTRAINT_EXECUTOR_PAIR(([this] (auto& visitor) {
            // FIXME: Unlike the "Cs" constraint which is skipped during verification
            // because conservative roots are not stable, this skip is only here because
            // ConservativeRoots::genericAddPointer asserts isMarking(), which doesn't
            // hold during verification. This constraint could run always otherwise, but
            // that would require rethinking the assumptions in ConservativeRoots.
            if (m_isMarkingForGCVerifier)
                return;
            IsoSubspace* subspace = m_pinballCompletionSpace.get();
            if (!subspace)
                return;
            ASSERT(worldIsStopped());
            // ConservativeRoots gathering requires an up-to-date precise allocations snapshot.
            m_objectSpace.prepareForConservativeScan();
            // FIXME: Add a second CellState for PinballCompletion so we can skip
            // pinballs whose conservative roots have already been gathered this cycle.
            ConservativeRoots conservativeRoots(*this);
            subspace->forEachMarkedCell([&](HeapCell* cell, HeapCell::Kind) {
                auto* pinball = uncheckedDowncast<PinballCompletion>(static_cast<JSCell*>(cell));
                pinball->gatherConservativeRoots(conservativeRoots);
            });
            SetRootMarkReasonScope rootScope(visitor, RootMarkReason::PinballCompletionConservativeRoots);
            visitor.append(conservativeRoots);
        })),
        ConstraintVolatility::GreyedByMarking,
        ConstraintConcurrency::Sequential);
#endif

#if ENABLE(JIT)
    if (Options::useJIT()) {
        m_constraintSet->add(
            "Jw"_s, "JIT Worklist"_s,
            MAKE_MARKING_CONSTRAINT_EXECUTOR_PAIR(([this] (auto& visitor) {
                SetRootMarkReasonScope rootScope(visitor, RootMarkReason::JITWorkList);

                JITWorklist::ensureGlobalWorklist().visitWeakReferences(visitor);
                
                // FIXME: This is almost certainly unnecessary.
                // https://bugs.webkit.org/show_bug.cgi?id=166829
                JITWorklist::ensureGlobalWorklist().iterateCodeBlocksForGC(visitor,
                    vm(),
                    [&] (CodeBlock* codeBlock) {
                        visitor.appendUnbarriered(codeBlock);
                    });
                
                if (Options::logGC() == GCLogging::Verbose)
                    dataLog("JIT Worklists:\n", visitor);
            })),
            ConstraintVolatility::GreyedByMarking);
    }
#endif
    
    m_constraintSet->add(
        "Cb"_s, "CodeBlocks"_s,
        MAKE_MARKING_CONSTRAINT_EXECUTOR_PAIR(([this] (auto& visitor) {
            SetRootMarkReasonScope rootScope(visitor, RootMarkReason::CodeBlocks);
            iterateExecutingAndCompilingCodeBlocksWithoutHoldingLocks(visitor,
                [&] (CodeBlock* codeBlock) {
                    // Visit the CodeBlock as a constraint only if it's black.
                    if (visitor.isMarked(codeBlock)
                        && codeBlock->cellState() == CellState::PossiblyBlack)
                        visitor.visitAsConstraint(codeBlock);
                });
        })),
        ConstraintVolatility::SeldomGreyed);
    
    m_constraintSet->add(makeUnique<MarkStackMergingConstraint>(*this));
}

void Heap::addMarkingConstraint(std::unique_ptr<MarkingConstraint> constraint)
{
    PreventCollectionScope preventCollectionScope(*this);
    m_constraintSet->add(WTF::move(constraint));
}

void Heap::notifyIsSafeToCollect()
{
    if (!Options::useGC()) [[unlikely]]
        return;

    MonotonicTime before;
    if (Options::logGC()) [[unlikely]] {
        before = MonotonicTime::now();
        dataLog("[GC<", RawPointer(this), ">: starting ");
    }
    
    addCoreConstraints();
    
    m_isSafeToCollect = true;
    
    if (Options::collectContinuously())
        m_collector->startCollectingContinuously();
    
    dataLogIf(Options::logGC(), (MonotonicTime::now() - before).milliseconds(), "ms]\n");
}

// Use WTF_IGNORES_THREAD_SAFETY_ANALYSIS because this function conditionally locks m_collector->m_collectContinuouslyLock,
// which is not supported by analysis.
void Heap::preventCollection() WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    if (!m_isSafeToCollect)
        return;
    
    // Stops the collectContinuously thread from starting a collection. Unlocked by allowCollection(),
    // see PreventCollectionScope.
    m_collector->m_collectContinuouslyLock.lock();
    
    // Wait for all collections to finish.
    waitForCollector(
        [&] (const AbstractLocker&) -> bool {
            return !m_collector->hasOutstandingRequest();
        });
    
    // Now a collection can only start if this thread starts it.
    RELEASE_ASSERT(!m_collectionScope);
}

// Use WTF_IGNORES_THREAD_SAFETY_ANALYSIS because this function conditionally unlocks m_collector->m_collectContinuouslyLock,
// which is not supported by analysis.
void Heap::allowCollection() WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    if (!m_isSafeToCollect)
        return;
    
    m_collector->m_collectContinuouslyLock.unlock();
}

void Heap::setMutatorShouldBeFenced(bool value)
{
    m_mutatorShouldBeFenced = value;
    m_barrierThreshold = value ? tautologicalThreshold : blackThreshold;
}

void Heap::performIncrement(size_t bytes)
{
    if (!m_objectSpace.isMarking())
        return;

    if (isDeferred())
        return;

    m_incrementBalance += bytes * Options::gcIncrementScale();

    // Save ourselves from crazy. Since this is an optimization, it's OK to go back to any consistent
    // state when the double goes wild.
    if (std::isnan(m_incrementBalance) || std::isinf(m_incrementBalance))
        m_incrementBalance = 0;
    
    if (m_incrementBalance < static_cast<double>(Options::gcIncrementBytes()))
        return;

    double targetBytes = m_incrementBalance;
    if (targetBytes <= 0)
        return;
    targetBytes = std::min(targetBytes, Options::gcIncrementMaxBytes());

    SlotVisitor& visitor = *m_mutatorSlotVisitor;
    ParallelModeEnabler parallelModeEnabler(visitor);
    size_t bytesVisited = visitor.performIncrementOfDraining(static_cast<size_t>(targetBytes));
    // incrementBalance may go negative here because it'll remember how many bytes we overshot.
    m_incrementBalance -= bytesVisited;
}

void Heap::addGCCompletionCallback(const GCCompletionCallback& callback)
{
    m_gcCompletionCallbacks.append(callback);
}

void Heap::removeGCCompletionCallback(const GCCompletionCallback& callback)
{
    m_gcCompletionCallbacks.removeFirst(callback);
}

void Heap::verifierMark()
{
    RELEASE_ASSERT(!m_isMarkingForGCVerifier);

    SetForScope isMarkingForGCVerifierScope(m_isMarkingForGCVerifier, true);
    VerifierSlotVisitor& visitor = *m_verifierSlotVisitor;
    do {
        while (!visitor.isEmpty())
            visitor.drain();
        m_constraintSet->executeAllSynchronously(visitor);
        visitor.executeConstraintTasks();
    } while (!visitor.isEmpty());

    visitor.setDoneMarking();
}

void Heap::dumpVerifierMarkerData(HeapCell* cell)
{
    if (!Options::verifyGC())
        return;

    if (!Heap::isMarked(cell)) {
        dataLogLn("\n" "GC Verifier: cell ", RawPointer(cell), " was not marked by SlotVisitor");
        return;
    }

    // Use VerifierSlotVisitorScope to keep it live.
    RELEASE_ASSERT(m_verifierSlotVisitor && !m_isMarkingForGCVerifier);
    VerifierSlotVisitor& visitor = *m_verifierSlotVisitor;
    RELEASE_ASSERT(visitor.doneMarking());

    if (!visitor.isMarked(cell)) {
        dataLogLn("\n" "GC Verifier: ERROR cell ", RawPointer(cell), " was not marked by VerifierSlotVisitor");
        return;
    }

    dataLogLn("\n" "GC Verifier: Found marked cell ", RawPointer(cell), " with MarkerData:");
    visitor.dumpMarkerData(cell);
}

void Heap::verifyGC()
{
    RELEASE_ASSERT(m_verifierSlotVisitor);
    verifierMark();
    VerifierSlotVisitor& visitor = *m_verifierSlotVisitor;
    RELEASE_ASSERT(visitor.doneMarking() && !m_isMarkingForGCVerifier);

    visitor.forEachLiveCell([&] (HeapCell* cell) {
        if (Heap::isMarked(cell))
            return;

        dataLogLn("\n" "GC Verifier: ERROR cell ", RawPointer(cell), " was not marked");
        if (Options::verboseVerifyGC()) [[unlikely]]
            visitor.dumpMarkerData(cell);
        RELEASE_ASSERT(this->isMarked(cell));
    });

    if (!m_keepVerifierSlotVisitor)
        clearVerifierSlotVisitor();
}

void Heap::setKeepVerifierSlotVisitor() { m_keepVerifierSlotVisitor = true; }

void Heap::clearVerifierSlotVisitor()
{
    m_verifierSlotVisitor = nullptr;
    m_keepVerifierSlotVisitor = false;
}

void Heap::scheduleOpportunisticFullCollection()
{
    m_shouldDoOpportunisticFullCollection = true;
}

#define DEFINE_DYNAMIC_ISO_SUBSPACE_MEMBER_SLOW(name, heapCellType, type) \
    IsoSubspace* Heap::name##Slow() \
    { \
        ASSERT(!m_##name); \
        auto space = makeUnique<IsoSubspace> ISO_SUBSPACE_INIT(*this, heapCellType, type); \
        WTF::storeStoreFence(); \
        m_##name = WTF::move(space); \
        return m_##name.get(); \
    }

FOR_EACH_JSC_DYNAMIC_ISO_SUBSPACE(DEFINE_DYNAMIC_ISO_SUBSPACE_MEMBER_SLOW)

#undef DEFINE_DYNAMIC_ISO_SUBSPACE_MEMBER_SLOW

#define DEFINE_DYNAMIC_SPACE_AND_SET_MEMBER_SLOW(name, heapCellType, type, spaceType) \
    IsoSubspace* Heap::name##Slow() \
    { \
        ASSERT(!m_##name); \
        auto space = makeUnique<spaceType> ISO_SUBSPACE_INIT(*this, heapCellType, type); \
        WTF::storeStoreFence(); \
        m_##name = WTF::move(space); \
        return &m_##name->space; \
    }

DEFINE_DYNAMIC_SPACE_AND_SET_MEMBER_SLOW(evalExecutableSpace, destructibleCellHeapCellType, EvalExecutable, Heap::ScriptExecutableSpaceAndSets) // Hash:0x958e3e9d
DEFINE_DYNAMIC_SPACE_AND_SET_MEMBER_SLOW(moduleProgramExecutableSpace, destructibleCellHeapCellType, ModuleProgramExecutable, Heap::ScriptExecutableSpaceAndSets) // Hash:0x6506fa3c

#undef DEFINE_DYNAMIC_SPACE_AND_SET_MEMBER_SLOW

#define DEFINE_DYNAMIC_NON_ISO_SUBSPACE_MEMBER_SLOW(name, heapCellType, type, SubspaceType) \
    SubspaceType* Heap::name##Slow() \
    { \
        ASSERT(!m_##name); \
        auto space = makeUnique<SubspaceType>(ASCIILiteral(#SubspaceType " " #name), *this, heapCellType, fastMallocAllocator.get()); \
        WTF::storeStoreFence(); \
        m_##name = WTF::move(space); \
        return m_##name.get(); \
    }

FOR_EACH_JSC_WEBASSEMBLY_DYNAMIC_NON_ISO_SUBSPACE(DEFINE_DYNAMIC_NON_ISO_SUBSPACE_MEMBER_SLOW)
#undef DEFINE_DYNAMIC_NON_ISO_SUBSPACE_MEMBER_SLOW

#if ENABLE(WEBASSEMBLY)

void Heap::reportWasmCalleePendingDestruction(Ref<Wasm::Callee>&& callee)
{
    void* boxedCallee = CalleeBits::boxNativeCallee(callee.ptr());
    // This better be true or we won't find the callee in ConservativeRoots.
    ASSERT_UNUSED(boxedCallee, boxedCallee == removeArrayPtrTag(boxedCallee));

    Locker locker(m_wasmCalleesPendingDestructionLock);
    m_wasmCalleesPendingDestruction.add(WTF::move(callee));
}

bool Heap::isWasmCalleePendingDestruction(Wasm::Callee& callee)
{
    Locker locker(m_wasmCalleesPendingDestructionLock);
    return m_wasmCalleesPendingDestruction.contains(callee);
}

bool Heap::didDiscoverPendingWasmCallee(Wasm::Callee* callee)
{
    if (!m_wasmCalleesPendingDestructionSnapshot.contains(callee))
        return false;
    m_wasmCalleesDiscoveredDuringGC.add(callee);
    return true;
}

void Heap::prepareWasmCalleeCleanup()
{
    ASSERT(worldIsStopped());
    ASSERT(m_wasmCalleesPendingDestructionSnapshot.isEmpty());
    ASSERT(m_wasmCalleesDiscoveredDuringGC.isEmpty());
    m_wasmCalleesPendingDestructionSnapshot.clear();
    m_wasmCalleesDiscoveredDuringGC.clear();
    m_boxedWasmCalleeFilter = TinyBloomFilter<uintptr_t>();

    Locker locker(m_wasmCalleesPendingDestructionLock);
    for (auto& callee : m_wasmCalleesPendingDestruction) {
        m_wasmCalleesPendingDestructionSnapshot.add(callee.ptr());
        m_boxedWasmCalleeFilter.add(std::bit_cast<uintptr_t>(CalleeBits::boxNativeCallee(callee.ptr())));
    }
}

void Heap::finalizeWasmCalleeCleanup()
{
    ASSERT(worldIsStopped());
    if (m_wasmCalleesPendingDestructionSnapshot.isEmpty())
        return;

    // Release refs outside the lock since Callee destructors may call reportWasmCalleePendingDestruction.
    Vector<RefPtr<Wasm::Callee>, 8> wasmCalleesToRelease;
    {
        Locker locker(m_wasmCalleesPendingDestructionLock);
        wasmCalleesToRelease = m_wasmCalleesPendingDestruction.takeIf<8>([&](const auto& callee) {
            return m_wasmCalleesPendingDestructionSnapshot.contains(callee.ptr())
                && !m_wasmCalleesDiscoveredDuringGC.contains(callee.ptr());
        });
    }

    // We need to ensure our thread sees all the new callsites otherwise we could be discarding a BBQCallee
    // for foo but a different Callee could still have a stale direct call to foo's BBQ code on this core.
    // Realistically, this is probably not needed, since we're essentially guarenteed to make a syscall
    // that will syncronize the instruction cache during GC. That said, this happens so infrequently it's
    // better to just have the code be clear.
    if (!wasmCalleesToRelease.isEmpty())
        WTF::crossModifyingCodeFence();

    m_wasmCalleesPendingDestructionSnapshot.clear();
    m_wasmCalleesDiscoveredDuringGC.clear();
}

#endif

namespace GCClient {

#define INIT_CLIENT_ISO_SUBSPACE_FROM_SPACE_AND_SET(subspace) subspace(heap.subspace##AndSet.space)

#define INIT_CLIENT_ISO_SUBSPACE(name, heapCellType, type) \
    , name(heap.name)

Heap::Heap(JSC::Heap& heap)
    : m_server(heap)
    FOR_EACH_JSC_ISO_SUBSPACE(INIT_CLIENT_ISO_SUBSPACE)
    , INIT_CLIENT_ISO_SUBSPACE_FROM_SPACE_AND_SET(codeBlockSpace)
    , INIT_CLIENT_ISO_SUBSPACE_FROM_SPACE_AND_SET(functionExecutableSpace)
    , INIT_CLIENT_ISO_SUBSPACE_FROM_SPACE_AND_SET(programExecutableSpace)
    , INIT_CLIENT_ISO_SUBSPACE_FROM_SPACE_AND_SET(unlinkedFunctionExecutableSpace)
{
}

Heap::~Heap() = default;

#undef INIT_CLIENT_ISO_SUBSPACE
#undef CLIENT_ISO_SUBSPACE_INIT_FROM_SPACE_AND_SET


#define DEFINE_DYNAMIC_ISO_SUBSPACE_MEMBER_SLOW_IMPL(name, heapCellType, type) \
    IsoSubspace* Heap::name##Slow() \
    { \
        ASSERT(!m_##name); \
        Locker locker { server().m_lock }; \
        JSC::IsoSubspace& serverSpace = *server().name<SubspaceAccess::OnMainThread>(); \
        auto space = makeUnique<IsoSubspace>(serverSpace); \
        WTF::storeStoreFence(); \
        m_##name = WTF::move(space); \
        return m_##name.get(); \
    }

#define DEFINE_DYNAMIC_ISO_SUBSPACE_MEMBER_SLOW(name) \
    DEFINE_DYNAMIC_ISO_SUBSPACE_MEMBER_SLOW_IMPL(name, unused, unused2) \

FOR_EACH_JSC_DYNAMIC_ISO_SUBSPACE(DEFINE_DYNAMIC_ISO_SUBSPACE_MEMBER_SLOW_IMPL)

DEFINE_DYNAMIC_ISO_SUBSPACE_MEMBER_SLOW(evalExecutableSpace)
DEFINE_DYNAMIC_ISO_SUBSPACE_MEMBER_SLOW(moduleProgramExecutableSpace)

#undef DEFINE_DYNAMIC_ISO_SUBSPACE_MEMBER_SLOW_IMPL
#undef DEFINE_DYNAMIC_ISO_SUBSPACE_MEMBER_SLOW

} // namespace GCClient

} // namespace JSC
