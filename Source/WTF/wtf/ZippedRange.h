/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include <wtf/IndexedRange.h>

namespace WTF {

template<typename IteratorA, typename IteratorB> class ZippedRangeIterator {
public:
    ZippedRangeIterator(IteratorA&& iteratorA, IteratorB&& iteratorB)
        : m_iteratorA(WTFMove(iteratorA))
        , m_iteratorB(WTFMove(iteratorB))
    {
    }

    ZippedRangeIterator& operator++()
    {
        ++m_iteratorA;
        ++m_iteratorB;
        return *this;
    }

    auto operator*()
    {
        return std::pair<decltype(*m_iteratorA), decltype(*m_iteratorB)> { *m_iteratorA, *m_iteratorB };
    }

    bool operator==(const ZippedRangeIterator& other) const
    {
        // To ensure that we compare equal to end() even when we iterate two
        // collections of different sizes, we need to compare both A and B.
        // (Otherwise comparing either A or B would be sufficient, since they
        // increment in lockstep.)
        return m_iteratorA == other.m_iteratorA || m_iteratorB == other.m_iteratorB;
    }

private:
    IteratorA m_iteratorA;
    IteratorB m_iteratorB;
};

template<typename Iterator>
class ZippedRange {
public:
    template<typename CollectionA, typename CollectionB>
    ZippedRange(CollectionA&& collectionA, CollectionB&& collectionB)
        : m_begin(boundsCheckedBegin(std::forward<CollectionA>(collectionA)), boundsCheckedBegin(std::forward<CollectionB>(collectionB)))
        , m_end(boundsCheckedEnd(std::forward<CollectionA>(collectionA)), boundsCheckedEnd(std::forward<CollectionB>(collectionB)))
    {
        // Prevent use after destruction of a returned temporary.
        static_assert(std::ranges::borrowed_range<CollectionA>);
        static_assert(std::ranges::borrowed_range<CollectionB>);
    }

    auto begin() { return m_begin; }
    auto end() { return m_end; }

private:
    Iterator m_begin;
    Iterator m_end;
};

// Usage: for (auto [ valueA, valueB ] : zippedRange(collectionA, collectionB)) { ... }
template<typename CollectionA, typename CollectionB> auto zippedRange(CollectionA&& collectionA, CollectionB&& collectionB)
{
    using Iterator = ZippedRangeIterator<decltype(boundsCheckedBegin(std::forward<CollectionA>(collectionA))), decltype(boundsCheckedBegin(std::forward<CollectionB>(collectionB)))>;
    return ZippedRange<Iterator>(std::forward<CollectionA>(collectionA), std::forward<CollectionB>(collectionB));
}

} // namespace WTF

using WTF::zippedRange;
