/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#pragma once

#include <type_traits>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

namespace WTF {

// TrailingArray offers the feature trailing array in the derived class.
// We can allocate a memory like the following layout.
//
//     [  DerivedClass  ][ Trailing Array ]
//
// And trailing array offers appropriate methods for accessing and destructions.
template<typename Derived, typename T>
class TrailingArray {
    WTF_MAKE_NONCOPYABLE(TrailingArray);
    friend class JSC::LLIntOffsetsExtractor;
public:
    using value_type = T;
    using pointer = T*;
    using reference = T&;
    using const_reference = const T&;
    using const_pointer = const T*;
    using size_type = unsigned;
    using difference_type = std::make_signed_t<size_type>;
    using iterator = T*;
    using const_iterator = const T*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    explicit TrailingArray(unsigned size)
        : m_size(size)
    {
        static_assert(std::is_final_v<Derived>);
        VectorTypeOperations<T>::initializeIfNonPOD(begin(), end());
    }

    template<typename InputIterator>
    TrailingArray(unsigned size, InputIterator first, InputIterator last)
        : m_size(size)
    {
        static_assert(std::is_final_v<Derived>);
        ASSERT(static_cast<size_t>(std::distance(first, last)) == size);
        std::uninitialized_copy(first, last, begin());
    }

    ~TrailingArray()
    {
        VectorTypeOperations<T>::destruct(begin(), end());
    }

    static constexpr size_t allocationSize(unsigned size)
    {
        return offsetOfData() + size * sizeof(T);
    }

    unsigned size() const { return m_size; }
    bool isEmpty() const { return !size(); }
    unsigned byteSize() const { return size() * sizeof(T); }

    pointer data() { return bitwise_cast<T*>(bitwise_cast<uint8_t*>(static_cast<Derived*>(this)) + offsetOfData()); }
    const_pointer data() const { return bitwise_cast<const T*>(bitwise_cast<const uint8_t*>(static_cast<const Derived*>(this)) + offsetOfData()); }

    iterator begin() { return data(); }
    iterator end() { return data() + size(); }
    const_iterator begin() const { return cbegin(); }
    const_iterator end() const { return cend(); }
    const_iterator cbegin() const { return data(); }
    const_iterator cend() const { return data() + size(); }

    reverse_iterator rbegin() { return reverse_iterator(end()); }
    reverse_iterator rend() { return reverse_iterator(begin()); }
    const_reverse_iterator rbegin() const { return crbegin(); }
    const_reverse_iterator rend() const { return crend(); }
    const_reverse_iterator crbegin() const { return const_reverse_iterator(end()); }
    const_reverse_iterator crend() const { return const_reverse_iterator(begin()); }

    reference at(unsigned i)
    {
        ASSERT(i < size());
        return begin()[i];
    }

    const_reference at(unsigned i) const
    {
        ASSERT(i < size());
        return begin()[i];
    }

    reference operator[](unsigned i) { return at(i); }
    const_reference operator[](unsigned i) const { return at(i); }

    T& first() { return (*this)[0]; }
    const T& first() const { return (*this)[0]; }
    T& last() { return (*this)[size() - 1]; }
    const T& last() const { return (*this)[size() - 1]; }

    void fill(const T& val)
    {
        std::fill(begin(), end(), val);
    }

    static ptrdiff_t offsetOfSize() { return OBJECT_OFFSETOF(Derived, m_size); }
    static constexpr ptrdiff_t offsetOfData()
    {
        return WTF::roundUpToMultipleOf<alignof(T)>(sizeof(Derived));
    }

protected:
    unsigned m_size { 0 };
};

} // namespace WTF

using WTF::TrailingArray;
