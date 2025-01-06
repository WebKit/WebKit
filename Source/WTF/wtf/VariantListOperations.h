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
#include <wtf/VariantExtras.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WTF {

using VariantListIndex = unsigned;

struct VariantListItemMetadata {
    size_t size;      // sizeof(T)
    size_t alignment; // alignof(T)
};

// Utility concepts for constraining VariantList based on the underlying std::variant.

template<typename> struct VariantListItemMetadataTable;
template<typename... Ts> struct VariantListItemMetadataTable<std::variant<Ts...>> {
    static constexpr auto table = std::array { VariantListItemMetadata { sizeof(Ts), alignof(Ts) }... };
};

template<typename Variant> struct VariantListOperations {
    // MARK: - Value alignment.

    // Returns a new span with the start position updated to the aligned start of the value.
    static std::span<std::byte> alignBufferForValue(std::span<std::byte>, size_t alignment);
    static std::span<const std::byte> alignBufferForValue(std::span<const std::byte>, size_t alignment);
    template<typename T> static std::span<std::byte> alignBufferForValue(std::span<std::byte>);
    template<typename T> static std::span<const std::byte> alignBufferForValue(std::span<const std::byte>);

    // MARK: - Index reading & writing

    static VariantListIndex readIndex(std::span<std::byte>);
    static VariantListIndex readIndex(std::span<const std::byte>);
    static std::span<std::byte> writeIndex(VariantListIndex, std::span<std::byte>);

    // MARK: - Value reading & writing

    template<typename T> static T& readValue(std::span<std::byte>);
    template<typename T> static const T& readValue(std::span<const std::byte>);
    template<typename T, typename U> static std::span<std::byte> writeValue(U&&, std::span<std::byte>);

    // MARK: - Value+Index writing

    template<typename T> static size_t sizeRequiredToWriteAt(std::byte* buffer);
    template<typename T> static size_t sizeRequiredToWriteAt(const std::byte* buffer);
    template<typename T, typename U> static std::span<std::byte> write(U&&, std::span<std::byte> buffer);

    // MARK: - Value visiting.

    template<typename... F> static auto visitValue(std::span<std::byte>, F&&...);
    template<typename... F> static auto visitValue(std::span<const std::byte>, F&&...);

    // MARK: - Value iteration.

    // Returns a new span with the start position updated to the start of the next index.
    //
    // Requires the type of the element to already be known. Useful for internal cases where `typeForIndex` is already
    // being called.
    template<typename T> static std::span<std::byte> nextKnownType(std::span<std::byte>);
    template<typename T> static std::span<const std::byte> nextKnownType(std::span<const std::byte>);

    // Returns a new span with the start position updated to the start of the next index.
    //
    // Uses constexpr metadata table to lookup size and alignment information for the next element by index. Used by
    // external iterators to avoid duplicate calls to `typeForIndex()`.
    static std::span<std::byte> next(std::span<std::byte>);
    static std::span<const std::byte> next(std::span<const std::byte>);
    static constexpr VariantListItemMetadata lookupMetadataByIndex(VariantListIndex);

    // MARK: - List operations.

    static bool compare(std::span<const std::byte> bufferA, std::span<const std::byte> bufferB);
    static void destruct(std::span<std::byte>);
    static void copy(std::span<std::byte> newBuffer, std::span<const std::byte> oldBuffer);
    static void move(std::span<std::byte> newBuffer, std::span<std::byte> oldBuffer);
};

template<typename V> std::span<std::byte> VariantListOperations<V>::alignBufferForValue(std::span<std::byte> buffer, size_t alignment)
{
    return alignedBytes(buffer, alignment);
}

template<typename V> std::span<const std::byte> VariantListOperations<V>::alignBufferForValue(std::span<const std::byte> buffer, size_t alignment)
{
    return alignedBytes(buffer, alignment);
}

template<typename V> template<typename T> std::span<std::byte> VariantListOperations<V>::alignBufferForValue(std::span<std::byte> buffer)
{
    return alignBufferForValue(buffer, alignof(T));
}

template<typename V> template<typename T> std::span<const std::byte> VariantListOperations<V>::alignBufferForValue(std::span<const std::byte> buffer)
{
    return alignBufferForValue(buffer, alignof(T));
}

template<typename V> auto VariantListOperations<V>::readIndex(std::span<std::byte> buffer) -> VariantListIndex
{
    VariantListIndex value;
    memcpy(&value, buffer.data(), sizeof(VariantListIndex));
    return value;
}

template<typename V> auto VariantListOperations<V>::readIndex(std::span<const std::byte> buffer) -> VariantListIndex
{
    VariantListIndex value;
    memcpy(&value, buffer.data(), sizeof(VariantListIndex));
    return value;
}

template<typename V> auto VariantListOperations<V>::writeIndex(VariantListIndex index, std::span<std::byte> buffer) -> std::span<std::byte>
{
    memcpy(buffer.data(), &index, sizeof(VariantListIndex));
    return buffer.subspan(sizeof(VariantListIndex));
}

template<typename V> template<typename T> T& VariantListOperations<V>::readValue(std::span<std::byte> buffer)
{
    constexpr auto indexFromType = alternativeIndexV<T, V>;
    ASSERT_UNUSED(indexFromType, indexFromType == readIndex(buffer));

    return reinterpretCastSpanStartTo<T>(alignBufferForValue<T>(buffer.subspan(sizeof(VariantListIndex))));
}

template<typename V> template<typename T> const T& VariantListOperations<V>::readValue(std::span<const std::byte> buffer)
{
    constexpr auto indexFromType = alternativeIndexV<T, V>;
    ASSERT_UNUSED(indexFromType, indexFromType == readIndex(buffer));

    return reinterpretCastSpanStartTo<T>(alignBufferForValue<T>(buffer.subspan(sizeof(VariantListIndex))));
}

template<typename V> template<typename T, typename U> auto VariantListOperations<V>::writeValue(U&& value, std::span<std::byte> buffer) -> std::span<std::byte>
{
    new (NotNull, buffer.data()) T(std::forward<U>(value));
    return buffer.subspan(sizeof(T));
}

template<typename V> template<typename... F> auto VariantListOperations<V>::visitValue(std::span<std::byte> buffer, F&& ...f)
{
    auto visitor = makeVisitor(std::forward<F>(f)...);
    return typeForIndex<V>(readIndex(buffer), [&]<typename T>() {
        return std::invoke(visitor, readValue<T>(buffer));
    });
}

template<typename V> template<typename... F> auto VariantListOperations<V>::visitValue(std::span<const std::byte> buffer, F&& ...f)
{
    auto visitor = makeVisitor(std::forward<F>(f)...);
    return typeForIndex<V>(readIndex(buffer), [&]<typename T>() {
        return std::invoke(visitor, readValue<T>(buffer));
    });
}

template<typename V> template<typename T> std::span<std::byte> VariantListOperations<V>::nextKnownType(std::span<std::byte> buffer)
{
    return alignBufferForValue<T>(buffer.subspan(sizeof(VariantListIndex))).subspan(sizeof(T));
}

template<typename V> template<typename T> std::span<const std::byte> VariantListOperations<V>::nextKnownType(std::span<const std::byte> buffer)
{
    return alignBufferForValue<T>(buffer.subspan(sizeof(VariantListIndex))).subspan(sizeof(T));
}

template<typename V> constexpr VariantListItemMetadata VariantListOperations<V>::lookupMetadataByIndex(VariantListIndex index)
{
    constexpr auto table = VariantListItemMetadataTable<V> { };
    return table.table[index];
}
template<typename V> std::span<std::byte> VariantListOperations<V>::next(std::span<std::byte> buffer)
{
    auto metadata = lookupMetadataByIndex(readIndex(buffer));
    return alignBufferForValue(buffer.subspan(sizeof(VariantListIndex)), metadata.alignment).subspan(metadata.size);
}

template<typename V> std::span<const std::byte> VariantListOperations<V>::next(std::span<const std::byte> buffer)
{
    auto metadata = lookupMetadataByIndex(readIndex(buffer));
    return alignBufferForValue(buffer.subspan(sizeof(VariantListIndex)), metadata.alignment).subspan(metadata.size);
}

// MARK: - Value appending

template<typename V> template<typename T> size_t VariantListOperations<V>::sizeRequiredToWriteAt(std::byte* buffer)
{
    auto* bufferPlusIndex = buffer + sizeof(VariantListIndex);
    auto* bufferPlusIndexPlusAlignment = alignedBytes(bufferPlusIndex, alignof(T));
    auto* bufferPlusIndexPlusAlignmentPlusValue = bufferPlusIndexPlusAlignment + sizeof(T);

    return bufferPlusIndexPlusAlignmentPlusValue - buffer;
}

template<typename V> template<typename T> size_t VariantListOperations<V>::sizeRequiredToWriteAt(const std::byte* buffer)
{
    auto* bufferPlusIndex = buffer + sizeof(VariantListIndex);
    auto* bufferPlusIndexPlusAlignment = alignedBytes(bufferPlusIndex, alignof(T));
    auto* bufferPlusIndexPlusAlignmentPlusValue = bufferPlusIndexPlusAlignment + sizeof(T);

    return bufferPlusIndexPlusAlignmentPlusValue - buffer;
}

template<typename V> template<typename T, typename U> std::span<std::byte> VariantListOperations<V>::write(U&& value, std::span<std::byte> buffer)
{
    static constexpr VariantListIndex indexValue = alternativeIndexV<T, V>;

    buffer = writeIndex(indexValue, buffer);
    buffer = alignBufferForValue<T>(buffer);
    buffer = writeValue<T>(std::forward<U>(value), buffer);

    return buffer;
}

// MARK: - List operations

template<typename V> bool VariantListOperations<V>::compare(std::span<const std::byte> bufferA, std::span<const std::byte> bufferB)
{
    if ((bufferA.empty() && bufferB.empty()) || bufferA.data() == bufferB.data())
        return true;
    if (bufferA.empty() || bufferB.empty())
        return false;

    while (true) {
        auto indexA = readIndex(bufferA);
        auto indexB = readIndex(bufferB);
        if (indexA != indexB)
            return false;

        bool equal = typeForIndex<V>(indexA, [&]<typename T>() {
            if (readValue<T>(bufferA) != readValue<T>(bufferB))
                return false;

            bufferA = nextKnownType<T>(bufferA);
            bufferB = nextKnownType<T>(bufferB);
            return true;
        });

        if (!equal)
            return false;

        if (bufferA.empty() && bufferB.empty())
            break;
        if (bufferA.empty() || bufferB.empty())
            return false;
    }

    return true;
}

template<typename V> void VariantListOperations<V>::destruct(std::span<std::byte> buffer)
{
    while (!buffer.empty()) {
        visitValue(buffer, [&]<typename T>(T& value) {
            if constexpr (!std::is_trivially_destructible_v<T>)
                value.~T();
            buffer = nextKnownType<T>(buffer);
        });
    }
}

template<typename V> void VariantListOperations<V>::copy(std::span<std::byte> newBuffer, std::span<const std::byte> oldBuffer)
{
    if constexpr (VariantAllAlternativesCanCopyWithMemcpy<V>)
        memcpySpan(newBuffer, oldBuffer);
    else {
        while (!oldBuffer.empty()) {
            // Copy index.
            memcpySpan(newBuffer.first(sizeof(VariantListIndex)), oldBuffer.first(sizeof(VariantListIndex)));

            // Copy value.
            visitValue(oldBuffer, [&]<typename T>(const T& value) {
                oldBuffer = alignBufferForValue<T>(oldBuffer.subspan(sizeof(VariantListIndex)));
                newBuffer = alignBufferForValue<T>(newBuffer.subspan(sizeof(VariantListIndex)));

                if constexpr (VectorTraits<T>::canCopyWithMemcpy) {
                    memcpySpan(newBuffer.first(sizeof(T)), oldBuffer.first(sizeof(T)));
                } else {
                    new (NotNull, newBuffer.data()) T(value);
                }

                oldBuffer = oldBuffer.subspan(sizeof(T));
                newBuffer = newBuffer.subspan(sizeof(T));
            });
        }
    }
}

template<typename V> void VariantListOperations<V>::move(std::span<std::byte> newBuffer, std::span<std::byte> oldBuffer)
{
    if constexpr (VariantAllAlternativesCanMoveWithMemcpy<V>)
        memcpySpan(newBuffer, oldBuffer);
    else {
        while (!oldBuffer.empty()) {
            // Move index.
            memcpySpan(newBuffer.first(sizeof(VariantListIndex)), oldBuffer.first(sizeof(VariantListIndex)));

            // Move value.
            visitValue(oldBuffer, [&]<typename T>(T& value) {
                oldBuffer = alignBufferForValue<T>(oldBuffer.subspan(sizeof(VariantListIndex)));
                newBuffer = alignBufferForValue<T>(newBuffer.subspan(sizeof(VariantListIndex)));

                if constexpr (VectorTraits<T>::canMoveWithMemcpy) {
                    memcpySpan(newBuffer.first(sizeof(T)), oldBuffer.first(sizeof(T)));
                } else {
                    new (NotNull, newBuffer.data()) T(WTFMove(value));
                    value.~T();
                }

                oldBuffer = oldBuffer.subspan(sizeof(T));
                newBuffer = newBuffer.subspan(sizeof(T));
            });
        }
    }
}

// `VariantListProxy` acts as a replacement for a real `std::variant`, for use when
// iterating a `VariantList`, allowing access to elements without incurring the cost of
// copying into a `std::variant`. If a `std::variant` is needed, the `asVariant` function
// will perform the conversion.
template<typename V> struct VariantListProxy {
    using Variant = V;
    using Operations = VariantListOperations<Variant>;

    explicit VariantListProxy(std::span<const std::byte> buffer)
        : buffer { buffer }
    {
    }

    template<typename T> bool holds_alternative() const
    {
        return Operations::readIndex(buffer) == alternativeIndexV<T, Variant>;
    }

    template<size_t I> bool holds_alternative() const
    {
        static_assert(I <= std::variant_size_v<Variant>);
        return Operations::readIndex(buffer) == I;
    }

    template<typename... F> auto switchOn(F&& ...f) const
    {
        return Operations::visitValue(buffer, std::forward<F>(f)...);
    }

    Variant asVariant() const
    {
        return switchOn([](const auto& alternative) -> Variant { return alternative; });
    }

    bool operator==(const VariantListProxy& other) const
    {
        auto index = Operations::readIndex(buffer);
        auto otherIndex = Operations::readIndex(other.buffer);
        if (index != otherIndex)
            return false;

        return typeForIndex<V>(index, [&]<typename T>() {
            return Operations::template readValue<T>(buffer) == Operations::template readValue<T>(other.buffer);
        });
    }

    std::span<const std::byte> buffer;
};

// The `VariantListSizer` can be used to emulate adding elements, by type, to a `VariantList`
// in order to calculate the exact size requirements. This can then be passed to a `VariantList`
// to expand the capacity to the required amount.
template<typename V> struct VariantListSizer {
    using Variant = V;

    unsigned size { 0 };

    // Emulate appending a value of type `Arg` to the VariantList.
    template<typename Arg> void append()
        requires std::constructible_from<V, Arg>
    {
        using T = typename VariantBestMatch<V, Arg>::type;

        unsigned currentOffset = size;
        unsigned currentOffsetPlusIndex = currentOffset + sizeof(VariantListIndex);
        unsigned currentOffsetPlusIndexPlusAlignment = (currentOffsetPlusIndex - 1u + alignof(T)) & -alignof(T);
        unsigned currentOffsetPlusIndexPlusAlignmentPlusSize = currentOffsetPlusIndexPlusAlignment + sizeof(T);

        size = currentOffsetPlusIndexPlusAlignmentPlusSize;
    }
};

} // namespace WTF

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
