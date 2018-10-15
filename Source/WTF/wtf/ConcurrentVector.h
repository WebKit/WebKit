/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/ConcurrentBuffer.h>
#include <wtf/Noncopyable.h>

namespace WTF {

// An iterator for ConcurrentVector. It supports only the pre ++ operator
template <typename T, size_t SegmentSize = 8> class ConcurrentVector;
template <typename T, size_t SegmentSize = 8> class ConcurrentVectorIterator {
private:
    friend class ConcurrentVector<T, SegmentSize>;
public:
    typedef ConcurrentVectorIterator<T, SegmentSize> Iterator;

    ~ConcurrentVectorIterator() { }

    T& operator*() const { return m_vector.at(m_index); }
    T* operator->() const { return &m_vector.at(m_index); }

    // Only prefix ++ operator supported
    Iterator& operator++()
    {
        m_index++;
        return *this;
    }

    bool operator==(const Iterator& other) const
    {
        return m_index == other.m_index && &m_vector == &other.m_vector;
    }

    bool operator!=(const Iterator& other) const
    {
        return m_index != other.m_index || &m_vector != &other.m_vector;
    }

    ConcurrentVectorIterator& operator=(const ConcurrentVectorIterator<T, SegmentSize>& other)
    {
        m_vector = other.m_vector;
        m_index = other.m_index;
        return *this;
    }

private:
    ConcurrentVectorIterator(ConcurrentVector<T, SegmentSize>& vector, size_t index)
        : m_vector(vector)
        , m_index(index)
    {
    }

    ConcurrentVector<T, SegmentSize>& m_vector;
    size_t m_index;
};

// ConcurrentVector is like SegmentedVector, but suitable for scenarios where one thread appends
// elements and another thread continues to access elements at lower indices. Only one thread can
// append at a time, so that activity still needs locking. size() and last() are racy with append(),
// in the sense that last() may crash if an append() is running concurrently because size()-1 does yet
// have a segment.
//
// Typical users of ConcurrentVector already have some way of ensuring that by the time someone is
// trying to use an index, some synchronization has happened to ensure that this index contains fully
// initialized data. Thereafter, the keeper of that index is allowed to use it on this vector without
// any locking other than what is needed to protect the integrity of the element at that index. This
// works because we guarantee shrinking the vector is impossible and that growing the vector doesn't
// delete old vector spines.
template <typename T, size_t SegmentSize>
class ConcurrentVector {
    friend class ConcurrentVectorIterator<T, SegmentSize>;
    WTF_MAKE_NONCOPYABLE(ConcurrentVector);
    WTF_MAKE_FAST_ALLOCATED;

public:
    typedef ConcurrentVectorIterator<T, SegmentSize> Iterator;

    ConcurrentVector() = default;

    ~ConcurrentVector()
    {
    }

    // This may return a size that is bigger than the underlying storage, since this does not fence
    // manipulations of size. So if you access at size()-1, you may crash because this hasn't
    // allocated storage for that index yet.
    size_t size() const { return m_size; }

    bool isEmpty() const { return !size(); }

    T& at(size_t index)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(index < m_size);
        return segmentFor(index)->entries[subscriptFor(index)];
    }

    const T& at(size_t index) const
    {
        return const_cast<ConcurrentVector<T, SegmentSize>*>(this)->at(index);
    }

    T& operator[](size_t index)
    {
        return at(index);
    }

    const T& operator[](size_t index) const
    {
        return at(index);
    }

    T& first()
    {
        ASSERT_WITH_SECURITY_IMPLICATION(!isEmpty());
        return at(0);
    }
    const T& first() const
    {
        ASSERT_WITH_SECURITY_IMPLICATION(!isEmpty());
        return at(0);
    }
    
    // This may crash if run concurrently to append(). If you want to accurately track the size of
    // this vector, you'll have to do it yourself, with your own fencing.
    T& last()
    {
        ASSERT_WITH_SECURITY_IMPLICATION(!isEmpty());
        return at(size() - 1);
    }
    const T& last() const
    {
        ASSERT_WITH_SECURITY_IMPLICATION(!isEmpty());
        return at(size() - 1);
    }

    T takeLast()
    {
        ASSERT_WITH_SECURITY_IMPLICATION(!isEmpty());
        T result = WTFMove(last());
        --m_size;
        return result;
    }

    template<typename... Args>
    void append(Args&&... args)
    {
        ++m_size;
        if (!segmentExistsFor(m_size - 1))
            allocateSegment();
        new (NotNull, &last()) T(std::forward<Args>(args)...);
    }

    template<typename... Args>
    T& alloc(Args&&... args)
    {
        append(std::forward<Args>(args)...);
        return last();
    }

    void removeLast()
    {
        last().~T();
        --m_size;
    }

    void grow(size_t size)
    {
        if (size == m_size)
            return;
        ASSERT(size > m_size);
        ensureSegmentsFor(size);
        size_t oldSize = m_size;
        m_size = size;
        for (size_t i = oldSize; i < m_size; ++i)
            new (NotNull, &at(i)) T();
    }

    Iterator begin()
    {
        return Iterator(*this, 0);
    }

    Iterator end()
    {
        return Iterator(*this, m_size);
    }

private:
    struct Segment {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
            
        T entries[SegmentSize];
    };

    bool segmentExistsFor(size_t index)
    {
        return index / SegmentSize < m_numSegments;
    }

    Segment* segmentFor(size_t index)
    {
        return m_segments[index / SegmentSize].get();
    }

    size_t subscriptFor(size_t index)
    {
        return index % SegmentSize;
    }

    void ensureSegmentsFor(size_t size)
    {
        size_t segmentCount = (m_size + SegmentSize - 1) / SegmentSize;
        size_t neededSegmentCount = (size + SegmentSize - 1) / SegmentSize;

        for (size_t i = segmentCount ? segmentCount - 1 : 0; i < neededSegmentCount; ++i)
            ensureSegment(i);
    }

    void ensureSegment(size_t segmentIndex)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(segmentIndex <= m_numSegments);
        if (segmentIndex == m_numSegments)
            allocateSegment();
    }

    void allocateSegment()
    {
        m_segments.grow(m_numSegments + 1);
        m_segments[m_numSegments++] = std::make_unique<Segment>();
    }

    size_t m_size { 0 };
    ConcurrentBuffer<std::unique_ptr<Segment>> m_segments;
    size_t m_numSegments { 0 };
};

} // namespace WTF

using WTF::ConcurrentVector;
