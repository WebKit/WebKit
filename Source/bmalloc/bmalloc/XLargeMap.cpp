/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "XLargeMap.h"

namespace bmalloc {

XLargeRange XLargeMap::takeFree(size_t alignment, size_t size)
{
    size_t alignmentMask = alignment - 1;

    XLargeRange* candidate = m_free.end();
    for (XLargeRange* it = m_free.begin(); it != m_free.end(); ++it) {
        if (it->size() < size)
            continue;

        if (candidate != m_free.end() && candidate->begin() < it->begin())
            continue;

        if (test(it->begin(), alignmentMask)) {
            char* aligned = roundUpToMultipleOf(alignment, it->begin());
            if (aligned < it->begin()) // Check for overflow.
                continue;

            char* alignedEnd = aligned + size;
            if (alignedEnd < aligned) // Check for overflow.
                continue;

            if (alignedEnd > it->end())
                continue;
        }

        candidate = it;
    }
    
    if (candidate == m_free.end())
        return XLargeRange();

    return m_free.pop(candidate);
}

void XLargeMap::addFree(const XLargeRange& range)
{
    XLargeRange merged = range;

    for (size_t i = 0; i < m_free.size(); ++i) {
        auto& other = m_free[i];

        if (!canMerge(merged, other))
            continue;

        merged = merge(merged, m_free.pop(i--));
    }
    
    m_free.push(merged);
}

void XLargeMap::addAllocated(const XLargeRange& prev, const std::pair<XLargeRange, XLargeRange>& allocated, const XLargeRange& next)
{
    if (prev)
        m_free.push(prev);
    
    if (next)
        m_free.push(next);

    m_allocated.insert({ allocated.first, allocated.second });
}

XLargeRange XLargeMap::getAllocated(void* object)
{
    return m_allocated.find(object)->object;
}

XLargeRange XLargeMap::takeAllocated(void* object)
{
    Allocation allocation = m_allocated.take(object);
    return merge(allocation.object, allocation.unused);
}

void XLargeMap::shrinkToFit()
{
    m_free.shrinkToFit();
    m_allocated.shrinkToFit();
}

XLargeRange XLargeMap::takePhysical() {
    auto hasPhysical = [](const XLargeRange& range) {
        return range.vmState().hasPhysical();
    };

    auto it = std::find_if(m_free.begin(), m_free.end(), hasPhysical);
    if (it != m_free.end())
        return m_free.pop(it);

    auto hasUnused = [](const Allocation& allocation) {
        return allocation.unused && allocation.unused.vmState().hasPhysical();
    };
    
    XLargeRange swapped;
    auto it2 = std::find_if(m_allocated.begin(), m_allocated.end(), hasUnused);
    if (it2 != m_allocated.end())
        std::swap(it2->unused, swapped);
    
    return swapped;
}

void XLargeMap::addVirtual(const XLargeRange& range)
{
    auto canMerge = [&range](const Allocation& other) {
        return other.object.end() == range.begin();
    };

    if (range.size() < xLargeAlignment) {
        // This is an unused fragment, so it might belong in the allocated list.
        auto it = std::find_if(m_allocated.begin(), m_allocated.end(), canMerge);
        if (it != m_allocated.end()) {
            BASSERT(!it->unused);
            it->unused = range;
            return;
        }

        // If we didn't find a neighbor in the allocated list, our neighbor must
        // have been freed. We'll merge with it below.
    }

    addFree(range);
}

} // namespace bmalloc
