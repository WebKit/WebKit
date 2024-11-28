/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <array>
#include <span>
#include <variant>
#include <wtf/StdLibExtras.h>
#include <wtf/VariantListOperations.h>
#include <wtf/Vector.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WTF {

// `VariantList<std::variant<Ts...>>` acts like a simplified `Vector<std::variant<Ts...>>` but with each
// element in the list only taking up space at the size of the specific element, not the size of the
// largest alternative, as would be the case with `Vector<std::variant<Ts...>>`.
//
// The tradeoff is that `VariantList<std::variant<Ts...>>` is not random-access indexable. Instead, users
// must iterate through the elements in order to access them.

template<typename V, size_t inlineCapacity> class VariantList : private VectorBuffer<std::byte, inlineCapacity, VectorBufferMalloc> {
    using Base = VectorBuffer<std::byte, inlineCapacity, VectorBufferMalloc>;
public:
    using Variant = V;
    using Tag = VariantListTag;
    using Operations = VariantListOperations<Variant>;
    using Proxy = VariantListProxy<Variant>;
    using Sizer = VariantListSizer<Variant>;

    using const_iterator = VariantListConstIterator<Variant, inlineCapacity>;

    VariantList() = default;

    // Initializes the `VariantList` with an initial capacity derived from a provided `VariantListSizer`.
    VariantList(Sizer);

    VariantList(const VariantList&)
        requires std::is_copy_constructible_v<Variant>;
    VariantList& operator=(const VariantList&)
        requires std::is_copy_assignable_v<Variant>;

    VariantList(VariantList&&)
        requires std::is_move_constructible_v<Variant>;
    VariantList& operator=(VariantList&&)
        requires std::is_move_assignable_v<Variant>;

    ~VariantList();

    bool operator==(const VariantList&) const
        requires VariantAllowsEqualityOperator<V>;

    bool isEmpty() const { return m_size == 0; }

    size_t sizeInBytes() const { return static_cast<size_t>(m_size); }
    size_t capacityInBytes() const { return static_cast<size_t>(capacity()); }

    template<typename Arg> void append(Arg&&)
        requires VariantAllowsConstructionOfArg<V, std::decay_t<Arg>>;

    void append(const Variant&);
    void append(Variant&&);

    void append(const Proxy&);
    void append(Proxy&&);

    void append(const VariantList&);
    void append(VariantList&&);

    auto begin() const { return const_iterator(spanToSize()); }
    auto end() const   { return const_iterator(spanFromSizeToSize()); }

    template<typename...F> void forEach(F&&...) const;

private:
    friend VariantListConstIterator<V, inlineCapacity>;

    using Base::m_size;
    using Base::buffer;
    using Base::capacity;
    using Base::allocateBuffer;
    using Base::deallocateBuffer;

    static constexpr size_t minCapacityInBytes = 32;

    // Span from [buffer()] -> [buffer() + sizeInBytes()]
    std::span<std::byte> spanToSize() { return std::span(buffer(), sizeInBytes()); }
    std::span<const std::byte> spanToSize() const { return std::span(buffer(), sizeInBytes()); }

    // Span from [buffer()] -> [buffer() + capacityInBytes()]
    std::span<std::byte> spanToCapacity() { return std::span(buffer(), capacityInBytes()); }
    std::span<const std::byte> spanToCapacity() const { return std::span(buffer(), capacityInBytes()); }

    // Span from [buffer() + sizeInBytes()] -> [buffer() + capacityInBytes()]
    std::span<std::byte> spanFromSizeToCapacity() { return std::span(buffer(), capacityInBytes()).subspan(sizeInBytes()); }
    std::span<const std::byte> spanFromSizeToCapacity() const { return std::span(buffer(), capacityInBytes()).subspan(sizeInBytes()); }

    // Span from [buffer() + sizeInBytes()] -> [buffer() + sizeInBytes()]
    std::span<std::byte> spanFromSizeToSize() { return std::span(buffer(), sizeInBytes()).subspan(sizeInBytes()); }
    std::span<const std::byte> spanFromSizeToSize() const { return std::span(buffer(), sizeInBytes()).subspan(sizeInBytes()); }

    template<typename Arg> void appendImpl(Arg&&)
        requires VariantAllowsConstructionOfArg<V, std::decay_t<Arg>>;

    void reserveCapacity(size_t newCapacityInBytes);
    void expandCapacity(size_t newMinCapacityInBytes);
    void clear();
};

template<typename V, size_t inlineCapacity> struct VariantListConstIterator {
    using List = VariantList<V, inlineCapacity>;
    using Operations = typename List::Operations;
    using Variant = V;

    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = VariantListProxy<V>;

    VariantListConstIterator()
        : buffer { std::span<const std::byte> { } }
    {
    }

    VariantListConstIterator(const VariantListConstIterator& other)
        : buffer { other.buffer }
    {
    }

    VariantListConstIterator(VariantListConstIterator&& other)
        : buffer { std::exchange(other.buffer, std::span<const std::byte> { }) }
    {
    }

    VariantListConstIterator& operator=(const VariantListConstIterator& other)
    {
        buffer = other.buffer;
        return *this;
    }

    VariantListConstIterator& operator=(VariantListConstIterator&& other)
    {
        buffer = std::exchange(other.buffer, std::span<const std::byte> { });
        return *this;
    }

    value_type operator*() const
    {
        return proxy();
    }

    VariantListConstIterator& operator++()
    {
        buffer = Operations::next(buffer);
        return *this;
    }

    VariantListConstIterator operator++(int)
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    bool operator==(const VariantListConstIterator& other) const
    {
        return buffer.data() == other.buffer.data() && buffer.size() == other.buffer.size();
    }

    value_type proxy() const
    {
        return value_type(buffer);
    }

private:
    friend VariantList<V, inlineCapacity>;

    VariantListConstIterator(std::span<const std::byte> buffer)
        : buffer { buffer }
    {
    }

    std::span<const std::byte> buffer;
};

static_assert(std::forward_iterator<typename VariantList<std::variant<int, float>>::const_iterator>);

// Initializes the `VariantList` with an initial capacity derived from a `VariantListSizer`.
template<typename V, size_t inlineCapacity> VariantList<V, inlineCapacity>::VariantList(Sizer sizer)
    : Base(sizer.size, 0)
{
}

template<typename V, size_t inlineCapacity> VariantList<V, inlineCapacity>::VariantList(const VariantList<V, inlineCapacity>& other) requires std::is_copy_constructible_v<V>
    : Base(other.m_size, other.m_size)
{
    if (m_size)
        Operations::copy(spanToSize(), other.spanToSize());
}

template<typename V, size_t inlineCapacity> VariantList<V, inlineCapacity>& VariantList<V, inlineCapacity>::operator=(const VariantList<V, inlineCapacity>& other) requires std::is_copy_assignable_v<V>
{
    if (&other == this)
        return *this;

    Operations::destruct(spanToSize());
    m_size = 0;

    if (other.m_size > capacityInBytes())
        expandCapacity(other.m_size);

    Operations::copy(spanToCapacity(), other.spanToSize());
    m_size = other.m_size;

    return *this;
}

template<typename V, size_t inlineCapacity> VariantList<V, inlineCapacity>::VariantList(VariantList<V, inlineCapacity>&& other) requires std::is_move_constructible_v<V>
    : Base(WTFMove(other))
{
}

template<typename V, size_t inlineCapacity> VariantList<V, inlineCapacity>& VariantList<V, inlineCapacity>::operator=(VariantList<V, inlineCapacity>&& other) requires std::is_move_assignable_v<V>
{
    if (m_size)
        Operations::destruct(spanToSize());

    Base::adopt(WTFMove(other));
    return *this;
}

template<typename V, size_t inlineCapacity> VariantList<V, inlineCapacity>::~VariantList()
{
    Operations::destruct(spanToSize());
}

template<typename V, size_t inlineCapacity> bool VariantList<V, inlineCapacity>::operator==(const VariantList<V, inlineCapacity>& other) const requires VariantAllowsEqualityOperator<V>
{
    return Operations::compare(spanToSize(), other.spanToSize());
}

template<typename V, size_t inlineCapacity> template<typename Arg> void VariantList<V, inlineCapacity>::appendImpl(Arg&& arg) requires VariantAllowsConstructionOfArg<V, std::decay_t<Arg>>
{
    using T = std::decay_t<Arg>;

    auto remainingBuffer = spanFromSizeToCapacity();
    auto newSizeInBytes = m_size + Operations::template sizeRequiredToWriteAt<T>(remainingBuffer.data());

    if (newSizeInBytes > capacityInBytes()) {
        expandCapacity(newSizeInBytes);
        return appendImpl(std::forward<Arg>(arg));
    }

    Operations::write(std::forward<Arg>(arg), remainingBuffer);
    m_size = newSizeInBytes;
}

template<typename V, size_t inlineCapacity> template<typename Arg> void VariantList<V, inlineCapacity>::append(Arg&& arg) requires VariantAllowsConstructionOfArg<V, std::decay_t<Arg>>
{
    appendImpl(std::forward<Arg>(arg));
}

template<typename V, size_t inlineCapacity> void VariantList<V, inlineCapacity>::append(const V& arg)
{
    WTF::switchOn(arg, [&](const auto& alternative) { appendImpl(alternative); });
}

template<typename V, size_t inlineCapacity> void VariantList<V, inlineCapacity>::append(V&& arg)
{
    WTF::switchOn(WTFMove(arg), [&](auto&& alternative) { appendImpl(WTFMove(alternative)); });
}

template<typename V, size_t inlineCapacity> void VariantList<V, inlineCapacity>::append(const Proxy& arg)
{
    WTF::switchOn(arg, [&](const auto& alternative) { appendImpl(alternative); });
}

template<typename V, size_t inlineCapacity> void VariantList<V, inlineCapacity>::append(Proxy&& arg)
{
    WTF::switchOn(WTFMove(arg), [&](auto&& alternative) { appendImpl(WTFMove(alternative)); });
}

template<typename V, size_t inlineCapacity> void VariantList<V, inlineCapacity>::append(const VariantList<V, inlineCapacity>& arg)
{
    arg.forEach([&](const auto& element) { appendImpl(element); });
}

template<typename V, size_t inlineCapacity> void VariantList<V, inlineCapacity>::append(VariantList<V, inlineCapacity>&& arg)
{
    arg.forEach([&](const auto& element) { appendImpl(element); });
}

template<typename V, size_t inlineCapacity> template<typename...F> void VariantList<V, inlineCapacity>::forEach(F&&... f) const
{
    auto visitor = makeVisitor(std::forward<F>(f)...);

    auto buffer = spanToSize();
    while (!buffer.empty()) {
        Operations::visitValue(buffer, [&]<typename T>(const T& value) {
            std::invoke(visitor, value);
            buffer = Operations::template nextKnownType<T>(buffer);
        });
    }
}

template<typename V, size_t inlineCapacity> void VariantList<V, inlineCapacity>::reserveCapacity(size_t newCapacityInBytes)
{
    if (newCapacityInBytes <= capacityInBytes())
        return;

    auto oldBuffer = spanToSize();
    Base::template allocateBuffer<FailureAction::Crash>(newCapacityInBytes);
    auto newBuffer = spanToSize();

    Operations::move(newBuffer, oldBuffer);

    Base::deallocateBuffer(oldBuffer.data());
}

template<typename V, size_t inlineCapacity> void VariantList<V, inlineCapacity>::expandCapacity(size_t newMinCapacityInBytes)
{
    return reserveCapacity(
        std::max({ newMinCapacityInBytes, minCapacityInBytes, VectorBufferMalloc::nextCapacity(capacityInBytes()) })
    );
}

} // namespace WTF

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
