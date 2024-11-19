/*
 * Copyright (C) 2011-2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ConservativeRoots.h"

#include "CalleeBits.h"
#include "CodeBlock.h"
#include "CodeBlockSetInlines.h"
#include "HeapUtil.h"
#include "JITStubRoutineSet.h"
#include "JSCast.h"
#include "JSCellInlines.h"
#include "MarkedBlockInlines.h"
#include "WasmCallee.h"
#include <wtf/OSAllocator.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

static constexpr bool verboseWasmCalleeScan = false;

ConservativeRoots::ConservativeRoots(JSC::Heap& heap)
    : m_roots(m_inlineRoots)
    , m_size(0)
    , m_capacity(inlineCapacity)
    , m_heap(heap)
{
#if ENABLE(WEBASSEMBLY)
    Locker locker(heap.m_wasmCalleesPendingDestructionLock);
    for (auto& iter : heap.m_wasmCalleesPendingDestruction) {
        void* boxedWasmCallee = CalleeBits::boxNativeCallee(iter.ptr());
        ASSERT_UNUSED(boxedWasmCallee, boxedWasmCallee == removeArrayPtrTag(boxedWasmCallee));
        dataLogLnIf(verboseWasmCalleeScan, "Looking for ", RawPointer(iter.ptr()), " boxed: ", RawPointer(boxedWasmCallee));
        // FIXME: This seems like it could have some kind of bulk add.
        m_wasmCalleesPendingDestructionCopy.add(iter.ptr());
        m_boxedWasmCalleeFilter.add(std::bit_cast<uintptr_t>(CalleeBits::boxNativeCallee(iter.ptr())));
    }
#endif
}

ConservativeRoots::~ConservativeRoots()
{
#if ENABLE(WEBASSEMBLY)
    if (m_wasmCalleesPendingDestructionCopy.size()) {
        // We need to deref these from outside the lock since other Callees could be registered by the destructors.
        Vector<RefPtr<Wasm::Callee>, 8> wasmCalleesToRelease;
        {
            Locker locker(m_heap.m_wasmCalleesPendingDestructionLock);
            wasmCalleesToRelease = m_heap.m_wasmCalleesPendingDestruction.takeIf<8>([&] (const auto& callee) {
                dataLogLnIf(verboseWasmCalleeScan && m_wasmCalleesPendingDestructionCopy.contains(callee.ptr()), RawPointer(callee.ptr()), " was ", m_wasmCalleesDiscovered.contains(callee.ptr()) ? "discovered" : "not discovered");
                return m_wasmCalleesPendingDestructionCopy.contains(callee.ptr()) && !m_wasmCalleesDiscovered.contains(callee.ptr());
            });
            dataLogLnIf(verboseWasmCalleeScan, m_heap.m_wasmCalleesPendingDestruction.size(), " callees remaining");
        }
    }
#endif

    if (m_roots != m_inlineRoots)
        OSAllocator::decommitAndRelease(m_roots, m_capacity * sizeof(HeapCell*));
}

void ConservativeRoots::grow()
{
    size_t newCapacity = m_capacity * 2;
    HeapCell** newRoots = static_cast<HeapCell**>(OSAllocator::reserveAndCommit(newCapacity * sizeof(HeapCell*)));
    memcpy(newRoots, m_roots, m_size * sizeof(HeapCell*));
    if (m_roots != m_inlineRoots)
        OSAllocator::decommitAndRelease(m_roots, m_capacity * sizeof(HeapCell*));
    m_capacity = newCapacity;
    m_roots = newRoots;
}

// This function must be run after stopThePeriphery() is called and
// before liveness data is cleared to be accurate.
template<bool lookForWasmCallees, typename MarkHook>
inline void ConservativeRoots::genericAddPointer(char* pointer, HeapVersion markingVersion, HeapVersion newlyAllocatedVersion, TinyBloomFilter<uintptr_t> jsGCFilter, TinyBloomFilter<uintptr_t> boxedWasmCalleeFilter, MarkHook& markHook)
{
    ASSERT(m_heap.worldIsStopped());
    pointer = removeArrayPtrTag(pointer);
    markHook.mark(pointer);

    auto markFoundGCPointer = [&] (void* p, HeapCell::Kind cellKind) {
        if (isJSCellKind(cellKind))
            markHook.markKnownJSCell(static_cast<JSCell*>(p));

        if (m_size == m_capacity)
            grow();

        m_roots[m_size++] = std::bit_cast<HeapCell*>(p);
    };

    const HashSet<MarkedBlock*>& set = m_heap.objectSpace().blocks().set();

    ASSERT(m_heap.objectSpace().isMarking());
    static constexpr bool isMarking = true;

#if ENABLE(WEBASSEMBLY) && USE(JSVALUE64)
    if constexpr (lookForWasmCallees) {
        CalleeBits calleeBits = std::bit_cast<CalleeBits>(pointer);
        // No point in even checking the hash set if the pointer doesn't even look like a native callee.
        if (calleeBits.isNativeCallee()) {
            if (!boxedWasmCalleeFilter.ruleOut(std::bit_cast<uintptr_t>(pointer))) {
                Wasm::Callee* wasmCallee = static_cast<Wasm::Callee*>(calleeBits.asNativeCallee());
                if (m_wasmCalleesPendingDestructionCopy.contains(wasmCallee)) {
                    m_wasmCalleesDiscovered.add(wasmCallee);
                    return;
                }
            }
            // FIXME: We could probably just return here.
        }
    }
#else
    UNUSED_PARAM(boxedWasmCalleeFilter);
#endif

    // It could point to a precise allocation.
    if (m_heap.objectSpace().preciseAllocationsForThisCollectionSize()) {
        if (m_heap.objectSpace().preciseAllocationsForThisCollectionBegin()[0]->aboveLowerBound(pointer)
            && m_heap.objectSpace().preciseAllocationsForThisCollectionEnd()[-1]->belowUpperBound(pointer)) {
            PreciseAllocation** result = approximateBinarySearch<PreciseAllocation*>(
                m_heap.objectSpace().preciseAllocationsForThisCollectionBegin(),
                m_heap.objectSpace().preciseAllocationsForThisCollectionSize(),
                PreciseAllocation::fromCell(pointer),
                [] (PreciseAllocation** ptr) -> PreciseAllocation* { return *ptr; });
            if (result) {
                auto attemptLarge = [&] (PreciseAllocation* allocation) {
                    if (allocation->contains(pointer) && allocation->hasValidCell())
                        markFoundGCPointer(allocation->cell(), allocation->attributes().cellKind);
                };

                if (result > m_heap.objectSpace().preciseAllocationsForThisCollectionBegin())
                    attemptLarge(result[-1]);
                attemptLarge(result[0]);
                if (result + 1 < m_heap.objectSpace().preciseAllocationsForThisCollectionEnd())
                    attemptLarge(result[1]);
            }
        }
    }

    MarkedBlock* candidate = MarkedBlock::blockFor(pointer);
    // It's possible for a butterfly pointer to point past the end of a butterfly. Check this now.
    if (pointer <= std::bit_cast<char*>(candidate) + sizeof(IndexingHeader)) {
        // We may be interested in the last cell of the previous MarkedBlock.
        char* previousPointer = std::bit_cast<char*>(std::bit_cast<uintptr_t>(pointer) - sizeof(IndexingHeader) - 1);
        MarkedBlock* previousCandidate = MarkedBlock::blockFor(previousPointer);
        if (!jsGCFilter.ruleOut(std::bit_cast<uintptr_t>(previousCandidate))
            && set.contains(previousCandidate)
            && mayHaveIndexingHeader(previousCandidate->handle().cellKind())) {
            previousPointer = static_cast<char*>(previousCandidate->handle().cellAlign(previousPointer));
            if (previousCandidate->handle().isLiveCell(markingVersion, newlyAllocatedVersion, isMarking, previousPointer))
                markFoundGCPointer(previousPointer, previousCandidate->handle().cellKind());
        }
    }

    if (jsGCFilter.ruleOut(std::bit_cast<uintptr_t>(candidate))) {
        ASSERT(!candidate || !set.contains(candidate));
        return;
    }

    if (!set.contains(candidate))
        return;

    HeapCell::Kind cellKind = candidate->handle().cellKind();

    auto tryPointer = [&] (void* pointer) {
        bool isLive = candidate->handle().isLiveCell(markingVersion, newlyAllocatedVersion, isMarking, pointer);
        if (isLive)
            markFoundGCPointer(pointer, cellKind);
        // Only return early if we are marking a non-butterfly, since butterflies without indexed properties could point past the end of their allocation.
        // If we do, and there is another live butterfly immediately following the first, we will mark the latter one here but we still need to
        // mark the former.
        return isLive && !mayHaveIndexingHeader(cellKind);
    };

    if (isJSCellKind(cellKind)) {
        if (LIKELY(MarkedBlock::isAtomAligned(pointer))) {
            if (tryPointer(pointer))
                return;
        }
    }

    // We could point into the middle of an object.
    char* alignedPointer = static_cast<char*>(candidate->handle().cellAlign(pointer));
    if (tryPointer(alignedPointer))
        return;

    // Also, a butterfly could point at the end of an object plus sizeof(IndexingHeader). In that
    // case, this is pointing to the object to the right of the one we should be marking.
    if (candidate->candidateAtomNumber(alignedPointer) > 0 && pointer <= alignedPointer + sizeof(IndexingHeader))
        tryPointer(alignedPointer - candidate->cellSize());
}

template<typename MarkHook>
SUPPRESS_ASAN
void ConservativeRoots::genericAddSpan(void* begin, void* end, MarkHook& markHook)
{
    if (begin > end)
        std::swap(begin, end);

    RELEASE_ASSERT(isPointerAligned(begin));
    RELEASE_ASSERT(isPointerAligned(end));
    // Make a local copy of filters to show the compiler it won't alias, and can be register-allocated.
    TinyBloomFilter<uintptr_t> jsGCFilter = m_heap.objectSpace().blocks().filter();
    TinyBloomFilter<uintptr_t> boxedWasmCalleeFilter = m_boxedWasmCalleeFilter;

    HeapVersion markingVersion = m_heap.objectSpace().markingVersion();
    HeapVersion newlyAllocatedVersion = m_heap.objectSpace().newlyAllocatedVersion();
#if ENABLE(WEBASSEMBLY)
    if (boxedWasmCalleeFilter.bits()) {
        constexpr bool lookForWasmCallees = true;
        for (char** it = static_cast<char**>(begin); it != static_cast<char**>(end); ++it)
            genericAddPointer<lookForWasmCallees>(*it, markingVersion, newlyAllocatedVersion, jsGCFilter, boxedWasmCalleeFilter, markHook);
    } else {
#else
    {
#endif
        constexpr bool lookForWasmCallees = false;
        for (char** it = static_cast<char**>(begin); it != static_cast<char**>(end); ++it)
            genericAddPointer<lookForWasmCallees>(*it, markingVersion, newlyAllocatedVersion, jsGCFilter, boxedWasmCalleeFilter, markHook);
    }
}

class DummyMarkHook {
public:
    void mark(void*) { }
    void markKnownJSCell(JSCell*) { }
};

void ConservativeRoots::add(void* begin, void* end)
{
    DummyMarkHook dummy;
    genericAddSpan(begin, end, dummy);
}

class CompositeMarkHook {
public:
    CompositeMarkHook(JITStubRoutineSet& stubRoutines, CodeBlockSet& codeBlocks, const AbstractLocker& locker)
        : m_stubRoutines(stubRoutines)
        , m_codeBlocks(codeBlocks)
        , m_codeBlocksLocker(locker)
    {
    }
    
    void mark(void* address)
    {
        m_stubRoutines.mark(address);
    }
    
    void markKnownJSCell(JSCell* cell)
    {
        if (cell->type() == CodeBlockType)
            m_codeBlocks.mark(m_codeBlocksLocker, jsCast<CodeBlock*>(cell));
    }

private:
    JITStubRoutineSet& m_stubRoutines;
    CodeBlockSet& m_codeBlocks;
    const AbstractLocker& m_codeBlocksLocker;
};

void ConservativeRoots::add(
    void* begin, void* end, JITStubRoutineSet& jitStubRoutines, CodeBlockSet& codeBlocks)
{
    Locker locker { codeBlocks.getLock() };
    CompositeMarkHook markHook(jitStubRoutines, codeBlocks, locker);
    genericAddSpan(begin, end, markHook);
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
