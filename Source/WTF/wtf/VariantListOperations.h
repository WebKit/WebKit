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
#include <wtf/VectorTraits.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WTF {

using VariantListTag = unsigned;

struct VariantListItemMetadata {
    size_t size;      // sizeof(T)
    size_t alignment; // alignof(T)
};

// Utility concepts for constraining VariantList based on the underlying std::variant.

template<typename V, typename Arg>
concept VariantAllowsConstructionOfArg = requires(Arg&& arg) { V(std::forward<Arg>(arg)); };

template<typename V>
concept VariantAllowsEqualityOperator = requires(const V& a, const V& b) { { a == b } -> std::convertible_to<bool>; };

template<typename> struct VariantListItemMetadataTable;
template<typename... Ts> struct VariantListItemMetadataTable<std::variant<Ts...>> {
    static constexpr auto table = std::array { VariantListItemMetadata { sizeof(Ts), alignof(Ts) }... };
};

template<typename>       struct VariantAllAlternativesCanCopyWithMemcpyHelper;
template<typename... Ts> struct VariantAllAlternativesCanCopyWithMemcpyHelper<std::variant<Ts...>> : std::integral_constant<bool, all<VectorTraits<Ts>::canCopyWithMemcpy...>> { };
template<typename V>
concept VariantAllAlternativesCanCopyWithMemcpy = VariantAllAlternativesCanCopyWithMemcpyHelper<V>::value;

template<typename>       struct VariantAllAlternativesCanMoveWithMemcpyHelper;
template<typename... Ts> struct VariantAllAlternativesCanMoveWithMemcpyHelper<std::variant<Ts...>> : std::integral_constant<bool, all<VectorTraits<Ts>::canMoveWithMemcpy...>> { };
template<typename V>
concept VariantAllAlternativesCanMoveWithMemcpy = VariantAllAlternativesCanMoveWithMemcpyHelper<V>::value;

template<typename Variant> struct VariantListOperations {
    // MARK: - Value alignment.

    // Returns a new span with the start position updated to the aligned start of the value.
    static std::span<std::byte> alignBufferForValue(std::span<std::byte>, size_t alignment);
    static std::span<const std::byte> alignBufferForValue(std::span<const std::byte>, size_t alignment);
    template<typename T> static std::span<std::byte> alignBufferForValue(std::span<std::byte>);
    template<typename T> static std::span<const std::byte> alignBufferForValue(std::span<const std::byte>);

    // MARK: - Tag reading & writing

    static VariantListTag readTag(std::span<std::byte>);
    static VariantListTag readTag(std::span<const std::byte>);
    static std::span<std::byte> writeTag(VariantListTag, std::span<std::byte>);

    // MARK: - Value reading & writing

    template<typename T> static T& readValue(std::span<std::byte>);
    template<typename T> static const T& readValue(std::span<const std::byte>);
    template<typename T> static std::span<std::byte> writeValue(T&&, std::span<std::byte>);

    // MARK: - Value+Tag writing

    template<typename T> static size_t sizeRequiredToWriteAt(std::byte* buffer);
    template<typename T> static size_t sizeRequiredToWriteAt(const std::byte* buffer);
    template<typename T> static std::span<std::byte> write(T&&, std::span<std::byte> buffer);

    // MARK: - Type switching

    // Calls a zero argument functor with a template argument corresponding to the tag's mapped type.
    //
    // e.g.
    //   typeForTag(0, /* tag for first parameter, <int> */
    //       []<typename T>() {
    //           if constexpr (std::is_same_v<T, int>) {
    //               print("we got an int");  <--- this will get called
    //           } else if constexpr (std::is_same_v<T, float>) {
    //               print("we got an float");  <--- this will NOT get called
    //           }
    //       }
    //   );

    template<size_t I = 0, typename F> static constexpr ALWAYS_INLINE decltype(auto) visitTypeForTag(VariantListTag, F&&);
    template<typename... F> static constexpr auto typeForTag(VariantListTag tag, F&&... f) -> decltype(visitTypeForTag(tag, makeVisitor(std::forward<F>(f)...)));

    // MARK: - Value visiting.

    template<typename... F> static auto visitValue(std::span<std::byte>, F&&...);
    template<typename... F> static auto visitValue(std::span<const std::byte>, F&&...);

    // MARK: - Value iteration.

    // Returns a new span with the start position updated to the start of the next tag.
    //
    // Requires the type of the element to already be known. Useful for internal cases where `typeForTag` is already
    // being called.
    template<typename T> static std::span<std::byte> nextKnownType(std::span<std::byte>);
    template<typename T> static std::span<const std::byte> nextKnownType(std::span<const std::byte>);

    // Returns a new span with the start position updated to the start of the next tag.
    //
    // Uses constexpr metadata table to lookup size and alignment information for the next element by tag. Used by
    // external iterators to avoid duplicate calls to `typeForTag()`.
    static std::span<std::byte> next(std::span<std::byte>);
    static std::span<const std::byte> next(std::span<const std::byte>);
    static constexpr VariantListItemMetadata lookupMetadataByTag(VariantListTag);

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

template<typename V> auto VariantListOperations<V>::readTag(std::span<std::byte> buffer) -> VariantListTag
{
    VariantListTag value;
    memcpy(&value, buffer.data(), sizeof(VariantListTag));
    return value;
}

template<typename V> auto VariantListOperations<V>::readTag(std::span<const std::byte> buffer) -> VariantListTag
{
    VariantListTag value;
    memcpy(&value, buffer.data(), sizeof(VariantListTag));
    return value;
}

template<typename V> auto VariantListOperations<V>::writeTag(VariantListTag tag, std::span<std::byte> buffer) -> std::span<std::byte>
{
    memcpy(buffer.data(), &tag, sizeof(VariantListTag));
    return buffer.subspan(sizeof(VariantListTag));
}

template<typename V> template<typename T> T& VariantListOperations<V>::readValue(std::span<std::byte> buffer)
{
    constexpr auto tagFromType = alternativeIndexV<T, V>;
    ASSERT_UNUSED(tagFromType, tagFromType == readTag(buffer));

    return reinterpretCastSpanStartTo<T>(alignBufferForValue<T>(buffer.subspan(sizeof(VariantListTag))));
}

template<typename V> template<typename T> const T& VariantListOperations<V>::readValue(std::span<const std::byte> buffer)
{
    constexpr auto tagFromType = alternativeIndexV<T, V>;
    ASSERT_UNUSED(tagFromType, tagFromType == readTag(buffer));

    return reinterpretCastSpanStartTo<T>(alignBufferForValue<T>(buffer.subspan(sizeof(VariantListTag))));
}

template<typename V> template<typename Arg> auto VariantListOperations<V>::writeValue(Arg&& value, std::span<std::byte> buffer) -> std::span<std::byte>
{
    using T = std::decay_t<Arg>;

    new (NotNull, buffer.data()) T(std::forward<Arg>(value));
    return buffer.subspan(sizeof(T));
}

template<typename V> template<size_t I, typename F> constexpr ALWAYS_INLINE decltype(auto) VariantListOperations<V>::visitTypeForTag(VariantListTag tag, F&& f)
{
    constexpr auto size = std::variant_size_v<V>;

    // To implement dispatch for variants, this recursively looping switch will work for
    // variants with any number of alternatives, with variants with greater than 32 recursing
    // and starting the switch at 32 (and so on).

#define WTF_VARIANT_LIST_VISIT_CASE_COUNT 32
#define WTF_VARIANT_LIST_VISIT_CASE(N, D) \
        case I + N:                                                                                 \
        {                                                                                           \
            if constexpr (I + N < size) {                                                           \
                return f.template operator()<std::variant_alternative_t<I + N, V>>();               \
            } else {                                                                                \
                WTF_UNREACHABLE();                                                                  \
            }                                                                                       \
        }                                                                                           \

    switch (tag) {
        WTF_VARIANT_LIST_VISIT_CASE(0, WTF_VARIANT_LIST_VISIT_CASE_COUNT)
        WTF_VARIANT_LIST_VISIT_CASE(1, WTF_VARIANT_LIST_VISIT_CASE_COUNT)
        WTF_VARIANT_LIST_VISIT_CASE(2, WTF_VARIANT_LIST_VISIT_CASE_COUNT)
        WTF_VARIANT_LIST_VISIT_CASE(3, WTF_VARIANT_LIST_VISIT_CASE_COUNT)
        WTF_VARIANT_LIST_VISIT_CASE(4, WTF_VARIANT_LIST_VISIT_CASE_COUNT)
        WTF_VARIANT_LIST_VISIT_CASE(5, WTF_VARIANT_LIST_VISIT_CASE_COUNT)
        WTF_VARIANT_LIST_VISIT_CASE(6, WTF_VARIANT_LIST_VISIT_CASE_COUNT)
        WTF_VARIANT_LIST_VISIT_CASE(7, WTF_VARIANT_LIST_VISIT_CASE_COUNT)
        WTF_VARIANT_LIST_VISIT_CASE(8, WTF_VARIANT_LIST_VISIT_CASE_COUNT)
        WTF_VARIANT_LIST_VISIT_CASE(9, WTF_VARIANT_LIST_VISIT_CASE_COUNT)
        WTF_VARIANT_LIST_VISIT_CASE(10, WTF_VARIANT_LIST_VISIT_CASE_COUNT)
        WTF_VARIANT_LIST_VISIT_CASE(11, WTF_VARIANT_LIST_VISIT_CASE_COUNT)
        WTF_VARIANT_LIST_VISIT_CASE(12, WTF_VARIANT_LIST_VISIT_CASE_COUNT)
        WTF_VARIANT_LIST_VISIT_CASE(13, WTF_VARIANT_LIST_VISIT_CASE_COUNT)
        WTF_VARIANT_LIST_VISIT_CASE(14, WTF_VARIANT_LIST_VISIT_CASE_COUNT)
        WTF_VARIANT_LIST_VISIT_CASE(15, WTF_VARIANT_LIST_VISIT_CASE_COUNT)
        WTF_VARIANT_LIST_VISIT_CASE(16, WTF_VARIANT_LIST_VISIT_CASE_COUNT)
        WTF_VARIANT_LIST_VISIT_CASE(17, WTF_VARIANT_LIST_VISIT_CASE_COUNT)
        WTF_VARIANT_LIST_VISIT_CASE(18, WTF_VARIANT_LIST_VISIT_CASE_COUNT)
        WTF_VARIANT_LIST_VISIT_CASE(19, WTF_VARIANT_LIST_VISIT_CASE_COUNT)
        WTF_VARIANT_LIST_VISIT_CASE(20, WTF_VARIANT_LIST_VISIT_CASE_COUNT)
        WTF_VARIANT_LIST_VISIT_CASE(21, WTF_VARIANT_LIST_VISIT_CASE_COUNT)
        WTF_VARIANT_LIST_VISIT_CASE(22, WTF_VARIANT_LIST_VISIT_CASE_COUNT)
        WTF_VARIANT_LIST_VISIT_CASE(23, WTF_VARIANT_LIST_VISIT_CASE_COUNT)
        WTF_VARIANT_LIST_VISIT_CASE(24, WTF_VARIANT_LIST_VISIT_CASE_COUNT)
        WTF_VARIANT_LIST_VISIT_CASE(25, WTF_VARIANT_LIST_VISIT_CASE_COUNT)
        WTF_VARIANT_LIST_VISIT_CASE(26, WTF_VARIANT_LIST_VISIT_CASE_COUNT)
        WTF_VARIANT_LIST_VISIT_CASE(27, WTF_VARIANT_LIST_VISIT_CASE_COUNT)
        WTF_VARIANT_LIST_VISIT_CASE(28, WTF_VARIANT_LIST_VISIT_CASE_COUNT)
        WTF_VARIANT_LIST_VISIT_CASE(29, WTF_VARIANT_LIST_VISIT_CASE_COUNT)
        WTF_VARIANT_LIST_VISIT_CASE(30, WTF_VARIANT_LIST_VISIT_CASE_COUNT)
        WTF_VARIANT_LIST_VISIT_CASE(31, WTF_VARIANT_LIST_VISIT_CASE_COUNT)
    }

    constexpr auto nextI = std::min(I + WTF_VARIANT_LIST_VISIT_CASE_COUNT, size);

    if constexpr (nextI < size)
        return visitTypeForTag<nextI>(tag, std::forward<F>(f));

    WTF_UNREACHABLE();

#undef WTF_VARIANT_LIST_VISIT_CASE_COUNT
#undef WTF_VARIANT_LIST_VISIT_CASE
}

template<typename V> template<typename... F> constexpr auto VariantListOperations<V>::typeForTag(VariantListTag tag, F&&... f) -> decltype(visitTypeForTag(tag, makeVisitor(std::forward<F>(f)...)))
{
    return visitTypeForTag(tag, makeVisitor(std::forward<F>(f)...));
}

template<typename V> template<typename... F> auto VariantListOperations<V>::visitValue(std::span<std::byte> buffer, F&& ...f)
{
    auto visitor = makeVisitor(std::forward<F>(f)...);
    return typeForTag(readTag(buffer), [&]<typename T>() {
        return std::invoke(visitor, readValue<T>(buffer));
    });
}

template<typename V> template<typename... F> auto VariantListOperations<V>::visitValue(std::span<const std::byte> buffer, F&& ...f)
{
    auto visitor = makeVisitor(std::forward<F>(f)...);
    return typeForTag(readTag(buffer), [&]<typename T>() {
        return std::invoke(visitor, readValue<T>(buffer));
    });
}

template<typename V> template<typename T> std::span<std::byte> VariantListOperations<V>::nextKnownType(std::span<std::byte> buffer)
{
    return alignBufferForValue<T>(buffer.subspan(sizeof(VariantListTag))).subspan(sizeof(T));
}

template<typename V> template<typename T> std::span<const std::byte> VariantListOperations<V>::nextKnownType(std::span<const std::byte> buffer)
{
    return alignBufferForValue<T>(buffer.subspan(sizeof(VariantListTag))).subspan(sizeof(T));
}

template<typename V> constexpr VariantListItemMetadata VariantListOperations<V>::lookupMetadataByTag(VariantListTag tag)
{
    constexpr auto table = VariantListItemMetadataTable<V> { };
    return table.table[tag];
}
template<typename V> std::span<std::byte> VariantListOperations<V>::next(std::span<std::byte> buffer)
{
    auto metadata = lookupMetadataByTag(readTag(buffer));
    return alignBufferForValue(buffer.subspan(sizeof(VariantListTag)), metadata.alignment).subspan(metadata.size);
}

template<typename V> std::span<const std::byte> VariantListOperations<V>::next(std::span<const std::byte> buffer)
{
    auto metadata = lookupMetadataByTag(readTag(buffer));
    return alignBufferForValue(buffer.subspan(sizeof(VariantListTag)), metadata.alignment).subspan(metadata.size);
}

// MARK: - Value appending

template<typename V> template<typename T> size_t VariantListOperations<V>::sizeRequiredToWriteAt(std::byte* buffer)
{
    auto* bufferPlusTag = buffer + sizeof(VariantListTag);
    auto* bufferPlusTagPlusAlignment = alignedBytes(bufferPlusTag, alignof(T));
    auto* bufferPlusTagPlusAlignmentPlusValue = bufferPlusTagPlusAlignment + sizeof(T);

    return bufferPlusTagPlusAlignmentPlusValue - buffer;
}

template<typename V> template<typename T> size_t VariantListOperations<V>::sizeRequiredToWriteAt(const std::byte* buffer)
{
    auto* bufferPlusTag = buffer + sizeof(VariantListTag);
    auto* bufferPlusTagPlusAlignment = alignedBytes(bufferPlusTag, alignof(T));
    auto* bufferPlusTagPlusAlignmentPlusValue = bufferPlusTagPlusAlignment + sizeof(T);

    return bufferPlusTagPlusAlignmentPlusValue - buffer;
}

template<typename V> template<typename Arg> std::span<std::byte> VariantListOperations<V>::write(Arg&& value, std::span<std::byte> buffer)
{
    using T = std::decay_t<Arg>;
    static constexpr VariantListTag tagValue = alternativeIndexV<T, V>;

    buffer = writeTag(tagValue, buffer);
    buffer = alignBufferForValue<T>(buffer);
    buffer = writeValue(std::forward<Arg>(value), buffer);

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
        auto tagA = readTag(bufferA);
        auto tagB = readTag(bufferB);
        if (tagA != tagB)
            return false;

        bool equal = typeForTag(tagA, [&]<typename T>() {
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
            // Copy tag.
            memcpySpan(newBuffer.first(sizeof(VariantListTag)), oldBuffer.first(sizeof(VariantListTag)));

            // Copy value.
            visitValue(oldBuffer, [&]<typename T>(const T& value) {
                oldBuffer = alignBufferForValue<T>(oldBuffer.subspan(sizeof(VariantListTag)));
                newBuffer = alignBufferForValue<T>(newBuffer.subspan(sizeof(VariantListTag)));

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
            // Move tag.
            memcpySpan(newBuffer.first(sizeof(VariantListTag)), oldBuffer.first(sizeof(VariantListTag)));

            // Move value.
            visitValue(oldBuffer, [&]<typename T>(T& value) {
                oldBuffer = alignBufferForValue<T>(oldBuffer.subspan(sizeof(VariantListTag)));
                newBuffer = alignBufferForValue<T>(newBuffer.subspan(sizeof(VariantListTag)));

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
        return alternativeIndexV<T, Variant> == Operations::readTag(buffer);
    }

    template<typename... F> auto visit(F&& ...f) const
    {
        return Operations::visitValue(buffer, std::forward<F>(f)...);
    }

    Variant asVariant() const
    {
        return visit([](const auto& alternative) -> Variant { return alternative; });
    }

    bool operator==(const VariantListProxy& other) const
    {
        auto tag = Operations::readTag(buffer);
        auto otherTag = Operations::readTag(other.buffer);
        if (tag != otherTag)
            return false;

        return Operations::typeForTag(tag, [&]<typename T>() {
            return Operations::template readValue<T>(buffer) == Operations::template readValue<T>(other.buffer);
        });
    }

    std::span<const std::byte> buffer;
};

// Overload WTF::switchOn to allow it to be used with a `VariantListProxy`
template<class V, class... F> ALWAYS_INLINE auto switchOn(V&& v, F&&... f) -> decltype(v.visit(std::forward<F>(f)...))
{
    return v.visit(std::forward<F>(f)...);
}


// The `VariantListSizer` can be used to emulate adding elements, by type, to a `VariantList`
// in order to calculate the exact size requirements. This can then be passed to a `VariantList`
// to expand the capacity to the required amount.
template<typename V> struct VariantListSizer {
    using Variant = V;

    unsigned size { 0 };

    // Emulate appending a value of type `Arg` to the VariantList.
    template<typename Arg> void append()
        requires VariantAllowsConstructionOfArg<V, std::decay_t<Arg>>
    {
        using T = std::decay_t<Arg>;

        unsigned currentOffset = size;
        unsigned currentOffsetPlusTag = currentOffset + sizeof(VariantListTag);
        unsigned currentOffsetPlusTagPlusAlignment = (currentOffsetPlusTag - 1u + alignof(T)) & -alignof(T);
        unsigned currentOffsetPlusTagPlusAlignmentPlusSize = currentOffsetPlusTagPlusAlignment + sizeof(T);

        size = currentOffsetPlusTagPlusAlignmentPlusSize;
    }
};

} // namespace WTF

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
