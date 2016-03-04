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

#ifndef SortedVector_h
#define SortedVector_h

#include "Vector.h"
#include <algorithm>

namespace bmalloc {

template<typename T>
class SortedVector {
    static_assert(std::is_trivially_destructible<T>::value, "SortedVector must have a trivial destructor.");

    struct Bucket {
        explicit Bucket(T value)
            : value(value)
            , isDeleted(false)
        {
        }
        
        template<typename U> bool operator<(const U& other) const
        {
            return value < other;
        }

        T value;
        bool isDeleted;
    };

public:
    class iterator : public std::iterator<std::forward_iterator_tag, T> {
    public:
        iterator(Bucket* bucket, Bucket* end)
            : m_bucket(bucket)
            , m_end(end)
        {
            skipDeletedBuckets();
        }
        
        iterator(const iterator& other)
            : m_bucket(other.m_bucket)
            , m_end(other.m_end)
        {
        }

        iterator& operator++()
        {
            BASSERT(m_bucket != m_end);
            ++m_bucket;
            skipDeletedBuckets();
            return *this;
        }

        bool operator!=(const iterator& other)
        {
            return m_bucket != other.m_bucket;
        }

        T& operator*()
        {
            BASSERT(m_bucket < m_end);
            BASSERT(!m_bucket->isDeleted);
            return m_bucket->value;
        }

        T* operator->()  { return &operator*(); }

    private:
        friend class SortedVector;

        void skipDeletedBuckets()
        {
            while (m_bucket != m_end && m_bucket->isDeleted)
                ++m_bucket;
        }

        Bucket* m_bucket;
        Bucket* m_end;
    };

    iterator begin() { return iterator(m_vector.begin(), m_vector.end()); }
    iterator end() { return iterator(m_vector.end(), m_vector.end()); }

    void insert(const T&);

    template<typename U> iterator find(const U&);
    template<typename U> T get(const U&);
    template<typename U> T take(const U&);

    void shrinkToFit();

private:
    Vector<Bucket> m_vector;
};

template<typename T>
void SortedVector<T>::insert(const T& value)
{
    auto it = std::lower_bound(m_vector.begin(), m_vector.end(), value);
    if (it != m_vector.end() && it->isDeleted) {
        *it = Bucket(value);
        return;
    }

    m_vector.insert(it, Bucket(value));
}

template<typename T> template<typename U>
typename SortedVector<T>::iterator SortedVector<T>::find(const U& value)
{
    auto it = std::lower_bound(m_vector.begin(), m_vector.end(), value);
    return iterator(it, m_vector.end());
}

template<typename T> template<typename U>
T SortedVector<T>::get(const U& value)
{
    return *find(value);
}

template<typename T> template<typename U>
T SortedVector<T>::take(const U& value)
{
    auto it = find(value);
    it.m_bucket->isDeleted = true;
    return it.m_bucket->value;
}

template<typename T>
void SortedVector<T>::shrinkToFit()
{
    auto isDeleted = [](const Bucket& bucket) {
        return bucket.isDeleted;
    };

    auto newEnd = std::remove_if(m_vector.begin(), m_vector.end(), isDeleted);
    size_t newSize = newEnd - m_vector.begin();
    m_vector.shrink(newSize);

    m_vector.shrinkToFit();
}

} // namespace bmalloc

#endif // SortedVector_h
