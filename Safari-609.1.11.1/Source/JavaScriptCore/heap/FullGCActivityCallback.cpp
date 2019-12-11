/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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
#include "FullGCActivityCallback.h"

#include "VM.h"

namespace JSC {

#if !PLATFORM(IOS_FAMILY)
const constexpr Seconds pagingTimeOut { 100_ms }; // Time in seconds to allow opportunistic timer to iterate over all blocks to see if the Heap is paged out.
#endif

FullGCActivityCallback::FullGCActivityCallback(Heap* heap)
    : GCActivityCallback(heap)
{
}

void FullGCActivityCallback::doCollection(VM& vm)
{
    Heap& heap = vm.heap;
    m_didGCRecently = false;

#if !PLATFORM(IOS_FAMILY)
    MonotonicTime startTime = MonotonicTime::now();
    if (heap.isPagedOut(startTime + pagingTimeOut)) {
        cancel();
        heap.increaseLastFullGCLength(pagingTimeOut);
        return;
    }
#endif

    heap.collectAsync(CollectionScope::Full);
}

Seconds FullGCActivityCallback::lastGCLength(Heap& heap)
{
    return heap.lastFullGCLength();
}

double FullGCActivityCallback::deathRate(Heap& heap)
{
    size_t sizeBefore = heap.sizeBeforeLastFullCollection();
    size_t sizeAfter = heap.sizeAfterLastFullCollection();
    if (!sizeBefore)
        return 1.0;
    if (sizeAfter > sizeBefore) {
        // GC caused the heap to grow(!)
        // This could happen if the we visited more extra memory than was reported allocated.
        // We don't return a negative death rate, since that would schedule the next GC in the past.
        return 0;
    }
    return static_cast<double>(sizeBefore - sizeAfter) / static_cast<double>(sizeBefore);
}

double FullGCActivityCallback::gcTimeSlice(size_t bytes)
{
    return std::min((static_cast<double>(bytes) / MB) * Options::percentCPUPerMBForFullTimer(), Options::collectionTimerMaxPercentCPU());
}

} // namespace JSC
