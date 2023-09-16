/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include <wtf/UniqueRef.h>
#include <wtf/Vector.h>

namespace WTF {

template<typename T>
class UniqueRefVectorIterator {
    using Iterator = UniqueRefVectorIterator<T>;

public:
    using difference_type = ptrdiff_t;
    using value_type = T;
    using pointer = T*;
    using reference = T&;
    using iterator_category = std::bidirectional_iterator_tag;

    UniqueRefVectorIterator(UniqueRef<T>* iterator)
        : m_iterator(iterator)
    {
    }

    T& operator*() const { return m_iterator->get(); }
    T* operator->() const { return m_iterator->ptr(); }

    friend bool operator==(Iterator, Iterator) = default;

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
    WTF::UniqueRef<T>* m_iterator;
};

template<typename T>
class UniqueRefVectorConstIterator {
    using Iterator = UniqueRefVectorConstIterator<T>;

public:
    using difference_type = ptrdiff_t;
    using value_type = T;
    using pointer = T*;
    using reference = T&;
    using iterator_category = std::bidirectional_iterator_tag;

    UniqueRefVectorConstIterator(const UniqueRef<T>* iterator)
        : m_iterator(iterator)
    {
    }

    const T& operator*() const { return m_iterator->get(); }
    const T* operator->() const { return m_iterator->ptr(); }

    friend bool operator==(Iterator, Iterator) = default;

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
    const WTF::UniqueRef<T>* m_iterator;
};

template<typename T, size_t inlineCapacity = 0>
class UniqueRefVector : public WTF::Vector<WTF::UniqueRef<T>, inlineCapacity> {
    using Base = WTF::Vector<WTF::UniqueRef<T>, inlineCapacity>;

public:
    using ValueType = T;
    using iterator = UniqueRefVectorIterator<T>;
    using const_iterator = UniqueRefVectorConstIterator<T>;
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
template<typename MatchFunction>
size_t UniqueRefVector<T, inlineCapacity>::findIf(const MatchFunction& matches) const
{
    for (size_t i = 0; i < size(); ++i) {
        if (matches(at(i)))
            return i;
    }
    return notFound;
}

} // namespace WTF

using WTF::UniqueRefVector;
