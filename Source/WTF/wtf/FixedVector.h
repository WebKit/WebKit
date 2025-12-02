/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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

#include <memory>
#include <wtf/EmbeddedFixedVector.h>
#include <wtf/MallocCommon.h>

namespace WTF {

template<typename T, typename Malloc>
class FixedVector {
    WTF_MAKE_CONFIGURABLE_ALLOCATED(Malloc);
public:
    using Storage = EmbeddedFixedVector<T, Malloc>;
    using Self = FixedVector<T, Malloc>;
    using value_type = typename Storage::value_type;
    using pointer = typename Storage::pointer;
    using reference = typename Storage::reference;
    using const_reference = typename Storage::const_reference;
    using const_pointer = typename Storage::const_pointer;
    using size_type = typename Storage::size_type;
    using difference_type = typename Storage::difference_type;
    using iterator = typename Storage::iterator;
    using const_iterator = typename Storage::const_iterator;
    using reverse_iterator = typename Storage::reverse_iterator;
    using const_reverse_iterator = typename Storage::const_reverse_iterator;

    FixedVector() = default;
    FixedVector(const FixedVector& other)
        : m_storage(other.m_storage ? other.m_storage->clone().moveToUniquePtr() : nullptr)
    { }
    FixedVector(FixedVector&& other) = default;

    FixedVector(std::initializer_list<T> initializerList)
        : m_storage(initializerList.size() ? Storage::create(initializerList).moveToUniquePtr() : nullptr)
    {
    }

    template<typename U, size_t Extent> FixedVector(std::span<U, Extent> span)
        : m_storage(span.empty() ? nullptr : Storage::create(span).moveToUniquePtr())
    {
    }

    template<typename InputIterator> FixedVector(InputIterator begin, InputIterator end)
        : m_storage(begin == end ? nullptr : Storage::create(begin, end).moveToUniquePtr())
    {
    }

    FixedVector& operator=(const FixedVector& other)
    {
        if (&other == this)
            return *this;

        FixedVector tmp(other);
        swap(tmp);
        return *this;
    }

    FixedVector& operator=(FixedVector&& other)
    {
        FixedVector tmp(WTFMove(other));
        swap(tmp);
        return *this;
    }

    explicit FixedVector(size_t size)
        : m_storage(size ? Storage::create(size).moveToUniquePtr() : nullptr)
    { }

    FixedVector(size_t size, const T& value)
        : m_storage(size ? Storage::create(size).moveToUniquePtr() : nullptr)
    {
        fill(value);
    }

    template<size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename VectorMalloc>
    explicit FixedVector(const Vector<T, inlineCapacity, OverflowHandler, minCapacity, VectorMalloc>& other)
        : m_storage(other.isEmpty() ? nullptr : Storage::createFromVector(other).moveToUniquePtr())
    { }

    // FIXME: Should we remove this now that it's not required for HashTable::add? This assignment is non-trivial and
    // should probably go through the explicit constructor.
    template<size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename VectorMalloc>
    FixedVector& operator=(const Vector<T, inlineCapacity, OverflowHandler, minCapacity, VectorMalloc>& other)
    {
        m_storage = other.isEmpty() ? nullptr : Storage::createFromVector(other).moveToUniquePtr();
        return *this;
    }

    template<size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename VectorMalloc>
    explicit FixedVector(Vector<T, inlineCapacity, OverflowHandler, minCapacity, VectorMalloc>&& other)
    {
        auto target = WTFMove(other);
        m_storage = target.isEmpty() ? nullptr : Storage::createFromVector(WTFMove(target)).moveToUniquePtr();
    }

    // FIXME: Should we remove this now that it's not required for HashTable::add? This assignment is non-trivial and
    // should probably go through the explicit constructor.
    template<size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename VectorMalloc>
    FixedVector& operator=(Vector<T, inlineCapacity, OverflowHandler, minCapacity, VectorMalloc>&& other)
    {
        auto target = WTFMove(other);
        m_storage = target.isEmpty() ? nullptr : Storage::createFromVector(WTFMove(target)).moveToUniquePtr();
        return *this;
    }

    template<typename... Args>
    static FixedVector createWithSizeAndConstructorArguments(size_t size, Args&&... args)
    {
        return Self { size ? Storage::createWithSizeAndConstructorArguments(size, std::forward<Args>(args)...).moveToUniquePtr() : std::unique_ptr<Storage> { nullptr } };
    }

    template<std::invocable<size_t> Generator>
    static FixedVector createWithSizeFromGenerator(size_t size, NOESCAPE Generator&& generator)
    {
        return Self { size ? Storage::createWithSizeFromGenerator(size, std::forward<Generator>(generator)).moveToUniquePtr() : std::unique_ptr<Storage> { nullptr } };
    }

    template<std::invocable<size_t> FailableGenerator>
    static FixedVector createWithSizeFromFailableGenerator(size_t size, NOESCAPE FailableGenerator&& generator)
    {
        return Self { size ? Storage::createWithSizeFromFailableGenerator(size, std::forward<FailableGenerator>(generator)) : std::unique_ptr<Storage> { nullptr } };
    }

    template<typename SizedRange, typename Mapper>
    static FixedVector map(SizedRange&& range, NOESCAPE Mapper&& mapper)
    {
        auto size = std::size(range);
        return Self { size ? Storage::map(size, std::forward<SizedRange>(range), std::forward<Mapper>(mapper)).moveToUniquePtr() : std::unique_ptr<Storage> { nullptr } };
    }

    size_t size() const { return m_storage ? m_storage->size() : 0; }
    bool isEmpty() const { return m_storage ? m_storage->isEmpty() : true; }
    size_t byteSize() const { return m_storage ? m_storage->byteSize() : 0; }

    iterator begin() LIFETIME_BOUND { return m_storage ? m_storage->begin() : nullptr; }
    iterator end() LIFETIME_BOUND { return m_storage ? m_storage->end() : nullptr; }

    const_iterator begin() const LIFETIME_BOUND { return const_cast<FixedVector*>(this)->begin(); }
    const_iterator end() const LIFETIME_BOUND { return const_cast<FixedVector*>(this)->end(); }

    reverse_iterator rbegin() LIFETIME_BOUND { return m_storage ? m_storage->rbegin() : reverse_iterator(nullptr); }
    reverse_iterator rend() LIFETIME_BOUND { return m_storage ? m_storage->rend() : reverse_iterator(nullptr); }
    const_reverse_iterator rbegin() const LIFETIME_BOUND { return m_storage ? m_storage->rbegin() : const_reverse_iterator(nullptr); }
    const_reverse_iterator rend() const LIFETIME_BOUND { return m_storage ? m_storage->rend() : const_reverse_iterator(nullptr); }

    T& at(size_t i) LIFETIME_BOUND { return m_storage->at(i); }
    const T& at(size_t i) const LIFETIME_BOUND { return m_storage->at(i); }

    T& operator[](size_t i) LIFETIME_BOUND { return m_storage->at(i); }
    const T& operator[](size_t i) const LIFETIME_BOUND { return m_storage->at(i); }

    T& first() LIFETIME_BOUND { return (*this)[0]; }
    const T& first() const LIFETIME_BOUND { return (*this)[0]; }
    T& last() LIFETIME_BOUND { return (*this)[size() - 1]; }
    const T& last() const LIFETIME_BOUND { return (*this)[size() - 1]; }

    void clear() { m_storage = nullptr; }

    void fill(const T& val)
    {
        if (!m_storage)
            return;
        m_storage->fill(val);
    }

    bool operator==(const Self& other) const
    {
        if (!m_storage) {
            if (!other.m_storage)
                return true;
            return other.m_storage->isEmpty();
        }
        if (!other.m_storage)
            return m_storage->isEmpty();
        return *m_storage == *other.m_storage;
    }

    bool contains(const auto&) const;
    bool containsIf(NOESCAPE const Invocable<bool(const T&)> auto&) const;
    size_t find(const auto&) const;
    size_t findIf(NOESCAPE const Invocable<bool(const T&)> auto&) const;
    size_t reverseFind(const auto&) const;
    size_t reverseFindIf(NOESCAPE const Invocable<bool(const T&)> auto&) const;

    void swap(Self& other)
    {
        using std::swap;
        swap(m_storage, other.m_storage);
    }

    static constexpr ptrdiff_t offsetOfStorage() { return OBJECT_OFFSETOF(FixedVector, m_storage); }

    Storage* storage() LIFETIME_BOUND { return m_storage.get(); }

    std::span<const T> span() const LIFETIME_BOUND { return m_storage ? m_storage->span() : std::span<const T> { }; }
    std::span<T> mutableSpan() LIFETIME_BOUND { return m_storage ? m_storage->span() : std::span<T> { }; }

    Vector<T> subvector(size_t offset, size_t length = std::dynamic_extent) const
    {
        return { span().subspan(offset, length) };
    }

    std::span<const T> subspan(size_t offset, size_t length = std::dynamic_extent) const LIFETIME_BOUND
    {
        return span().subspan(offset, length);
    }

private:
    friend class JSC::LLIntOffsetsExtractor;

    FixedVector(std::unique_ptr<Storage>&& storage)
        :  m_storage { WTFMove(storage) }
    { }

    std::unique_ptr<Storage> m_storage;
};
static_assert(sizeof(FixedVector<int>) == sizeof(int*));

template<typename T, typename Malloc>
bool FixedVector<T, Malloc>::containsIf(NOESCAPE const Invocable<bool(const T&)> auto& matches) const
{
    return findIf(matches) != notFound;
}

template<typename T, typename Malloc>
bool FixedVector<T, Malloc>::contains(const auto& value) const
{
    return find(value) != notFound;
}

template<typename T, typename Malloc>
size_t FixedVector<T, Malloc>::findIf(NOESCAPE const Invocable<bool(const T&)> auto& matches) const
{
    for (size_t i = 0; i < size(); ++i) {
        if (matches(at(i)))
            return i;
    }
    return notFound;
}

template<typename T, typename Malloc>
size_t FixedVector<T, Malloc>::find(const auto& value) const
{
    return findIf([&](auto& item) {
        return item == value;
    });
}

template<typename T, typename Malloc>
size_t FixedVector<T, Malloc>::reverseFindIf(NOESCAPE const Invocable<bool(const T&)> auto& matches) const
{
    for (size_t i = 1; i <= size(); ++i) {
        const size_t index = size() - i;
        if (matches(at(index)))
            return index;
    }
    return notFound;
}

template<typename T, typename Malloc>
size_t FixedVector<T, Malloc>::reverseFind(const auto& value) const
{
    return reverseFindIf([&](auto& item) {
        return item == value;
    });
}

template<typename T, typename Malloc>
inline void swap(FixedVector<T, Malloc>& a, FixedVector<T, Malloc>& b)
{
    a.swap(b);
}

template<typename T, typename Mapper, typename Malloc, typename ReturnType = typename std::invoke_result<Mapper, const T&>::type>
FixedVector<ReturnType, Malloc> map(const FixedVector<T, Malloc>& source, Mapper&& mapper)
{
    return FixedVector<ReturnType, Malloc>::map(source, std::forward<Mapper>(mapper));
}

} // namespace WTF

using WTF::FixedVector;
