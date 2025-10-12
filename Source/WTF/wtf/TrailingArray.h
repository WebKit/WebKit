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

#include <concepts>
#include <type_traits>
#include <wtf/IndexedRange.h>
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

protected:
    explicit TrailingArray(unsigned size)
        : m_size(size)
    {
        static_assert(std::is_final_v<Derived>);
        VectorTypeOperations<T>::initializeIfNonPOD(begin(), end());
    }

    explicit TrailingArray(std::initializer_list<T> initializerList)
        : m_size(initializerList.size())
    {
        static_assert(std::is_final_v<Derived>);
        std::uninitialized_copy(initializerList.begin(), initializerList.end(), begin());
    }

    template<typename U, size_t Extent>
    TrailingArray(std::span<U, Extent> span)
        : m_size(span.size())
    {
        static_assert(std::is_final_v<Derived>);
        std::uninitialized_copy(span.begin(), span.end(), begin());
    }

    template<typename InputIterator>
    TrailingArray(unsigned size, InputIterator first, InputIterator last)
        : m_size(size)
    {
        static_assert(std::is_final_v<Derived>);
        ASSERT(static_cast<size_t>(std::distance(first, last)) == size);
        std::uninitialized_copy(first, last, begin());
    }

    template<typename... Args>
    TrailingArray(unsigned size, Args&&... args) // create with given size and constructor arguments for all elements
        : m_size(size)
    {
        static_assert(std::is_final_v<Derived>);
        VectorTypeOperations<T>::initializeWithArgs(begin(), end(), std::forward<Args>(args)...);
    }

    template<std::invocable<size_t> Generator>
    explicit TrailingArray(unsigned size, NOESCAPE Generator&& generator)
        : m_size(size)
    {
        static_assert(std::is_final_v<Derived>);

        for (auto[i, item] : indexedRange(span()))
            new (NotNull, std::addressof(item)) T(generator(i));
    }

    // This constructor, which is used via the `Failable` token, will attempt
    // to initialize the array from the generator. The generator returns
    // `std::optional` values, and if one is `nullopt`, that indicates a failure.
    // The constructor sets `m_size` to the index of the most recently successful
    // item to be added in order for the destructor to destroy the right number
    // of elements.
    //
    // It is the responsibility of the caller to check that `size()` is equal
    // to the `size` the caller passed in. If it is not, that is failure, and
    // should be used as appropriate.
    struct Failable { };
    template<std::invocable<size_t> FailableGenerator>
    explicit TrailingArray(Failable, unsigned size, NOESCAPE FailableGenerator&& generator)
        : m_size(size)
    {
        static_assert(std::is_final_v<Derived>);

        for (auto[i, item] : indexedRange(span())) {
            if (auto value = generator(i))
                new (NotNull, std::addressof(item)) T(WTFMove(*value));
            else {
                m_size = i;
                return;
            }
        }
    }

    template<typename SizedRange, typename Mapper>
    explicit TrailingArray(unsigned size, SizedRange&& range, NOESCAPE Mapper&& mapper)
        : m_size(size)
    {
        static_assert(std::is_final_v<Derived>);

        auto span = this->span();
        size_t index = 0;
        for (const auto& element : range)
            new (NotNull, std::addressof(span[index++])) T(mapper(element));
    }

    ~TrailingArray()
    {
        VectorTypeOperations<T>::destruct(begin(), end());
    }

public:
    static constexpr size_t allocationSize(unsigned size)
    {
        return offsetOfData() + size * sizeof(T);
    }

    unsigned size() const { return m_size; }
    bool isEmpty() const { return !size(); }
    unsigned byteSize() const { return size() * sizeof(T); }

    std::span<T> span() LIFETIME_BOUND { return unsafeMakeSpan(data(), size()); }
    std::span<const T> span() const LIFETIME_BOUND { return unsafeMakeSpan(data(), size()); }

    iterator begin() LIFETIME_BOUND { return std::to_address(span().begin()); }
    iterator end() LIFETIME_BOUND { return std::to_address(span().end()); }
    const_iterator begin() const LIFETIME_BOUND { return cbegin(); }
    const_iterator end() const LIFETIME_BOUND { return cend(); }
    const_iterator cbegin() const LIFETIME_BOUND { return std::to_address(span().begin()); }
    const_iterator cend() const LIFETIME_BOUND { return std::to_address(span().end()); }

    reverse_iterator rbegin() LIFETIME_BOUND { return reverse_iterator(end()); }
    reverse_iterator rend() LIFETIME_BOUND { return reverse_iterator(begin()); }
    const_reverse_iterator rbegin() const LIFETIME_BOUND { return crbegin(); }
    const_reverse_iterator rend() const LIFETIME_BOUND { return crend(); }
    const_reverse_iterator crbegin() const LIFETIME_BOUND { return const_reverse_iterator(end()); }
    const_reverse_iterator crend() const LIFETIME_BOUND { return const_reverse_iterator(begin()); }

    reference at(unsigned i) LIFETIME_BOUND { return span()[i]; }

    const_reference at(unsigned i) const LIFETIME_BOUND { return span()[i]; }

    reference operator[](unsigned i) LIFETIME_BOUND { return at(i); }
    const_reference operator[](unsigned i) const LIFETIME_BOUND { return at(i); }

    T& first() LIFETIME_BOUND { return (*this)[0]; }
    const T& first() const LIFETIME_BOUND { return (*this)[0]; }
    T& last() LIFETIME_BOUND { return (*this)[size() - 1]; }
    const T& last() const LIFETIME_BOUND { return (*this)[size() - 1]; }

    void fill(const T& val)
    {
        std::fill(begin(), end(), val);
    }

    static constexpr ptrdiff_t offsetOfSize() { return OBJECT_OFFSETOF(Derived, m_size); }
    static constexpr ptrdiff_t offsetOfData()
    {
        return WTF::roundUpToMultipleOf<alignof(T)>(sizeof(Derived));
    }

protected:
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    pointer data() LIFETIME_BOUND { return std::bit_cast<T*>(std::bit_cast<uint8_t*>(static_cast<Derived*>(this)) + offsetOfData()); }
    const_pointer data() const LIFETIME_BOUND { return std::bit_cast<const T*>(std::bit_cast<const uint8_t*>(static_cast<const Derived*>(this)) + offsetOfData()); }
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

    unsigned m_size { 0 };
};

} // namespace WTF

using WTF::TrailingArray;
