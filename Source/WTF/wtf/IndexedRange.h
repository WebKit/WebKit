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

namespace WTF {

template<typename Iterator> class BoundsCheckedIterator {
public:
    // We require the caller to ask for 'begin' or 'end', rather than passing
    // us arbitrary 'it' and 'end' iterators, because that way we can prove by
    // construction that we have the correct 'end'.
    template<typename Collection>
    static BoundsCheckedIterator begin(Collection&& collection)
    {
        return BoundsCheckedIterator(std::forward<Collection>(collection), collection.begin());
    }

    template<typename Collection>
    static BoundsCheckedIterator end(Collection&& collection)
    {
        return BoundsCheckedIterator(std::forward<Collection>(collection), collection.end());
    }

    BoundsCheckedIterator& operator++()
    {
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
        RELEASE_ASSERT(m_iterator != m_end);
        ++m_iterator;
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

        return *this;
    }

    auto&& operator*() const
    {
        RELEASE_ASSERT(m_iterator != m_end);
        return *m_iterator;
    }

    bool operator==(const BoundsCheckedIterator& other) const
    {
        return m_iterator == other.m_iterator;
    }

private:
    template<typename Collection>
    BoundsCheckedIterator(Collection&& collection, Iterator&& iterator)
        : m_iterator(WTFMove(iterator))
        , m_end(collection.end())
    {
    }

    Iterator m_iterator;
    Iterator m_end;
};

template<typename Collection> auto boundsCheckedBegin(Collection&& collection)
{
    return BoundsCheckedIterator<decltype(collection.begin())>::begin(std::forward<Collection>(collection));
}

template<typename Collection> auto boundsCheckedEnd(Collection&& collection)
{
    return BoundsCheckedIterator<decltype(collection.end())>::end(std::forward<Collection>(collection));
}

template<typename Iterator> class IndexedRangeIterator {
public:
    IndexedRangeIterator(Iterator&& iterator)
        : m_iterator(WTFMove(iterator))
    {
    }

    IndexedRangeIterator& operator++()
    {
        ++m_index;
        ++m_iterator;
        return *this;
    }

    auto operator*()
    {
        return std::pair<size_t, decltype(*m_iterator)> { m_index, *m_iterator };
    }

    bool operator==(const IndexedRangeIterator& other) const
    {
        return m_iterator == other.m_iterator;
    }

private:
    size_t m_index { 0 };
    Iterator m_iterator;
};

template<typename Iterator>
class IndexedRange {
public:
    template<typename Collection>
    IndexedRange(Collection&& collection)
        : m_begin(boundsCheckedBegin(std::forward<Collection>(collection)))
        , m_end(boundsCheckedEnd(std::forward<Collection>(collection)))
    {
        // Prevent use after destruction of a returned temporary.
        static_assert(std::ranges::borrowed_range<Collection>);
    }

    auto begin() { return m_begin; }
    auto end() { return m_end; }

private:
    Iterator m_begin;
    Iterator m_end;
};

// Usage: for (auto [ index, value ] : indexedRange(collection)) { ... }
template<typename Collection> auto indexedRange(Collection&& collection)
{
    using Iterator = IndexedRangeIterator<decltype(boundsCheckedBegin(std::forward<Collection>(collection)))>;
    return IndexedRange<Iterator>(std::forward<Collection>(collection));
}

} // namespace WTF

using WTF::indexedRange;
