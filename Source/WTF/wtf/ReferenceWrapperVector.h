/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include <wtf/Vector.h>

namespace WTF {

template<typename T>
class ReferenceWrapperVectorIterator {
    using Iterator = ReferenceWrapperVectorIterator<T>;

public:
    using difference_type = ptrdiff_t;
    using value_type = T;
    using pointer = T*;
    using reference = T&;
    using iterator_category = std::bidirectional_iterator_tag;

    ReferenceWrapperVectorIterator(std::reference_wrapper<T>* iterator)
        : m_iterator(iterator)
    {
    }

    T& operator*() const { return m_iterator->get(); }
    T* operator->() const { return m_iterator->ptr(); }

    friend bool operator==(const Iterator&, const Iterator&) = default;

    Iterator& operator++()
    {
        ++m_iterator;
        return *this;
    }
    Iterator& operator--()
    {
        --m_iterator;
        return *this;
    }

private:
    std::reference_wrapper<T>* m_iterator;
};

template<typename T>
class ReferenceWrapperVectorConstIterator {
    using Iterator = ReferenceWrapperVectorConstIterator<T>;

public:
    using difference_type = ptrdiff_t;
    using value_type = T;
    using pointer = T*;
    using reference = T&;
    using iterator_category = std::bidirectional_iterator_tag;

    ReferenceWrapperVectorConstIterator(const std::reference_wrapper<T>* iterator)
        : m_iterator(iterator)
    {
    }

    const T& operator*() const { return m_iterator->get(); }
    const T* operator->() const { return m_iterator->ptr(); }

    friend bool operator==(const Iterator&, const Iterator&) = default;

    Iterator& operator++()
    {
        ++m_iterator;
        return *this;
    }
    Iterator& operator--()
    {
        --m_iterator;
        return *this;
    }

private:
    const std::reference_wrapper<T>* m_iterator;
};

template<typename T, size_t inlineCapacity = 0>
class ReferenceWrapperVector : public Vector<std::reference_wrapper<T>, inlineCapacity> {
    using Base = Vector<std::reference_wrapper<T>, inlineCapacity>;

public:
    using ValueType = T;
    using iterator = ReferenceWrapperVectorIterator<T>;
    using const_iterator = ReferenceWrapperVectorConstIterator<T>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    using Base::size;

    iterator begin() { return iterator { Base::begin() }; }
    iterator end() { return iterator { Base::end() }; }
    const_iterator begin() const { return const_iterator { Base::begin() }; }
    const_iterator end() const { return const_iterator { Base::end() }; }
    reverse_iterator rbegin() { return reverse_iterator(end()); }
    reverse_iterator rend() { return reverse_iterator(begin()); }
    const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
    const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }

    ReferenceWrapperVector() = default;
    ReferenceWrapperVector(std::initializer_list<std::reference_wrapper<T>>);

    T& at(size_t i) { return Base::at(i).get(); }
    const T& at(size_t i) const { return Base::at(i).get(); }

    T& operator[](size_t i) { return Base::at(i).get(); }
    const T& operator[](size_t i) const { return Base::at(i).get(); }

    T& first() { return Base::at(0).get(); }
    const T& first() const { return Base::at(0).get(); }
    T& last() { return Base::at(Base::size() - 1).get(); }
    const T& last() const { return Base::at(Base::size() - 1).get(); }

    template<typename MatchFunction> size_t findIf(const MatchFunction&) const;
    template<typename MatchFunction> bool containsIf(const MatchFunction& matches) const { return findIf(matches) != notFound; }
};

template<typename T, size_t inlineCapacity>
inline ReferenceWrapperVector<T, inlineCapacity>::ReferenceWrapperVector(std::initializer_list<std::reference_wrapper<T>> initializerList)
    : Base(initializerList)
{
}

template<typename T, size_t inlineCapacity>
template<typename MatchFunction>
size_t ReferenceWrapperVector<T, inlineCapacity>::findIf(const MatchFunction& matches) const
{
    for (size_t i = 0; i < size(); ++i) {
        if (matches(at(i)))
            return i;
    }
    return notFound;
}

} // namespace WTF

using WTF::ReferenceWrapperVector;
