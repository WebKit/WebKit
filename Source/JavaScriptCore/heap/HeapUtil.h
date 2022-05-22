/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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

#pragma once

namespace JSC {

// Are you tired of waiting for all of WebKit to build because you changed the implementation of a
// function in HeapInlines.h?  Does it bother you that you're waiting on rebuilding the JS DOM
// bindings even though your change is in a function called from only 2 .cpp files?  Then HeapUtil.h
// is for you!  Everything in this class should be a static method that takes a Heap& if needed.
// This is a friend of Heap, so you can access all of Heap's privates.
//
// This ends up being an issue because Heap exposes a lot of methods that ought to be inline for
// performance or that must be inline because they are templates.  This class ought to contain
// methods that are used for the implementation of the collector, or for unusual clients that need
// to reach deep into the collector for some reason.  Don't put things in here that would cause you
// to have to include it from more than a handful of places, since that would defeat the purpose.
// This class isn't here to look pretty.  It's to let us hack the GC more easily!

class HeapUtil {
public:
    // This function must be run after stopAllocation() is called and 
    // before liveness data is cleared to be accurate.
    template<typename Func>
    static void findGCObjectPointersForMarking(
        Heap& heap, HeapVersion markingVersion, HeapVersion newlyAllocatedVersion, TinyBloomFilter filter,
        void* passedPointer, const Func& func)
    {
        const HashSet<MarkedBlock*>& set = heap.objectSpace().blocks().set();
        
        ASSERT(heap.objectSpace().isMarking());
        static constexpr bool isMarking = true;
        
        char* pointer = static_cast<char*>(passedPointer);
        
        // It could point to a large allocation.
        if (heap.objectSpace().preciseAllocationsForThisCollectionSize()) {
            if (heap.objectSpace().preciseAllocationsForThisCollectionBegin()[0]->aboveLowerBound(pointer)
                && heap.objectSpace().preciseAllocationsForThisCollectionEnd()[-1]->belowUpperBound(pointer)) {
                PreciseAllocation** result = approximateBinarySearch<PreciseAllocation*>(
                    heap.objectSpace().preciseAllocationsForThisCollectionBegin(),
                    heap.objectSpace().preciseAllocationsForThisCollectionSize(),
                    PreciseAllocation::fromCell(pointer),
                    [] (PreciseAllocation** ptr) -> PreciseAllocation* { return *ptr; });
                if (result) {
                    auto attemptLarge = [&] (PreciseAllocation* allocation) {
                        if (allocation->contains(pointer) && allocation->hasValidCell())
                            func(allocation->cell(), allocation->attributes().cellKind);
                    };
                    
                    if (result > heap.objectSpace().preciseAllocationsForThisCollectionBegin())
                        attemptLarge(result[-1]);
                    attemptLarge(result[0]);
                    if (result + 1 < heap.objectSpace().preciseAllocationsForThisCollectionEnd())
                        attemptLarge(result[1]);
                }
            }
        }
    
        MarkedBlock* candidate = MarkedBlock::blockFor(pointer);
        // It's possible for a butterfly pointer to point past the end of a butterfly. Check this now.
        if (pointer <= bitwise_cast<char*>(candidate) + sizeof(IndexingHeader)) {
            // We may be interested in the last cell of the previous MarkedBlock.
            char* previousPointer = bitwise_cast<char*>(bitwise_cast<uintptr_t>(pointer) - sizeof(IndexingHeader) - 1);
            MarkedBlock* previousCandidate = MarkedBlock::blockFor(previousPointer);
            if (!filter.ruleOut(bitwise_cast<Bits>(previousCandidate))
                && set.contains(previousCandidate)
                && mayHaveIndexingHeader(previousCandidate->handle().cellKind())) {
                previousPointer = static_cast<char*>(previousCandidate->handle().cellAlign(previousPointer));
                if (previousCandidate->handle().isLiveCell(markingVersion, newlyAllocatedVersion, isMarking, previousPointer))
                    func(previousPointer, previousCandidate->handle().cellKind());
            }
        }
    
        if (filter.ruleOut(bitwise_cast<Bits>(candidate))) {
            ASSERT(!candidate || !set.contains(candidate));
            return;
        }
    
        if (!set.contains(candidate))
            return;

        HeapCell::Kind cellKind = candidate->handle().cellKind();
        
        auto tryPointer = [&] (void* pointer) {
            bool isLive = candidate->handle().isLiveCell(markingVersion, newlyAllocatedVersion, isMarking, pointer);
            if (isLive)
                func(pointer, cellKind);
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
        if (candidate->candidateAtomNumber(alignedPointer) > 0
            && pointer <= alignedPointer + sizeof(IndexingHeader))
            tryPointer(alignedPointer - candidate->cellSize());
    }
    
    static bool isPointerGCObjectJSCell(Heap& heap, TinyBloomFilter filter, JSCell* pointer)
    {
        // It could point to a large allocation.
        if (pointer->isPreciseAllocation()) {
            auto* set = heap.objectSpace().preciseAllocationSet();
            ASSERT(set);
            if (set->isEmpty())
                return false;
            return set->contains(pointer);
        }
    
        const HashSet<MarkedBlock*>& set = heap.objectSpace().blocks().set();
        
        MarkedBlock* candidate = MarkedBlock::blockFor(pointer);
        if (filter.ruleOut(bitwise_cast<Bits>(candidate))) {
            ASSERT(!candidate || !set.contains(candidate));
            return false;
        }
        
        if (!MarkedBlock::isAtomAligned(pointer))
            return false;
        
        if (!set.contains(candidate))
            return false;
        
        if (candidate->handle().cellKind() != HeapCell::JSCell)
            return false;
        
        if (!candidate->handle().isLiveCell(pointer))
            return false;
        
        return true;
    }
    
    // This does not find the cell if the pointer is pointing at the middle of a JSCell.
    static bool isValueGCObject(
        Heap& heap, TinyBloomFilter filter, JSValue value)
    {
        ASSERT(heap.objectSpace().preciseAllocationSet());
        if (!value.isCell())
            return false;
        return isPointerGCObjectJSCell(heap, filter, value.asCell());
    }
};

} // namespace JSC

