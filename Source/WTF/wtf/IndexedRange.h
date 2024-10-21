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

template<typename Iterator> class IndexedRangeIterator {
public:
    IndexedRangeIterator(Iterator&& iterator)
        : m_index(0)
        , m_iterator(WTFMove(iterator))
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
    size_t m_index;
    Iterator m_iterator;
};

// Usage: for (auto [ index, value ] : IndexedRange(collection)) { ... }
template<typename T> class IndexedRange {
public:
    IndexedRange(const T& range)
        : m_range(range)
    {
    }

    auto begin() { return IndexedRangeIterator(m_range.begin()); }
    auto end() { return IndexedRangeIterator(m_range.end()); }

private:
    const T& m_range;
};

} // namespace WTF

using WTF::IndexedRange;
