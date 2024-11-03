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

template<typename Collection, typename Iterator> class BoundsCheckedIterator {
public:
    static BoundsCheckedIterator begin(const Collection& collection)
    {
        return BoundsCheckedIterator(collection, collection.begin());
    }

    static BoundsCheckedIterator end(const Collection& collection)
    {
        return BoundsCheckedIterator(collection, collection.end());
    }

    BoundsCheckedIterator& operator++()
    {
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
        RELEASE_ASSERT(m_iterator != m_end);
        ++m_iterator;
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

        return *this;
    }

    auto operator*() const
    {
        RELEASE_ASSERT(m_iterator != m_end);
        return *m_iterator;
    }

    bool operator==(const BoundsCheckedIterator& other) const
    {
        return m_iterator == other.m_iterator;
    }

private:
    BoundsCheckedIterator(const Collection& collection, Iterator&& iterator)
        : m_iterator(WTFMove(iterator))
        , m_end(collection.end())
    {
    }

    Iterator m_iterator;
    Iterator m_end;
};

template<typename Collection> auto boundsCheckedBegin(const Collection& collection)
{
    return BoundsCheckedIterator<Collection, decltype(collection.begin())>::begin(collection);
}

template<typename Collection> auto boundsCheckedEnd(const Collection& collection)
{
    return BoundsCheckedIterator<Collection, decltype(collection.end())>::end(collection);
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
        return std::tuple<size_t, decltype(*m_iterator)> { m_index, *m_iterator };
    }

    bool operator==(const IndexedRangeIterator& other) const
    {
        return m_iterator == other.m_iterator;
    }

private:
    size_t m_index { 0 };
    Iterator m_iterator;
};

// Usage: for (auto [ index, value ] : IndexedRange(collection)) { ... }
template<typename Collection>
class IndexedRange {
    using Iterator = IndexedRangeIterator<decltype(boundsCheckedBegin(std::declval<Collection>()))>;
public:
    IndexedRange(const Collection& collection)
        : m_begin(boundsCheckedBegin(collection))
        , m_end(boundsCheckedEnd(collection))
    {
    }

    IndexedRange(Collection&& collection)
        : m_begin(boundsCheckedBegin(collection))
        , m_end(boundsCheckedEnd(collection))
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

} // namespace WTF

using WTF::IndexedRange;
