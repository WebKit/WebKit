/*
 * Copyright (C) 2012-2024 Apple Inc. All rights reserved.
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

#include "config.h"
#include "JSStringJoiner.h"

#include "JSCJSValueInlines.h"
#include "MathCommon.h"
#include "PageCount.h"
#include <charconv>
#include <wtf/Float16.h>
#include <wtf/dtoa.h>
#include <wtf/text/ParsingUtilities.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

JSStringJoiner::~JSStringJoiner() = default;

template<typename CharacterType>
static inline void appendStringToData(std::span<CharacterType>& data, StringView string)
{
    string.getCharacters(data);
    skip(data, string.length());
}

template<typename OutputCharacterType, typename SeparatorCharacterType>
static inline void appendStringToData(std::span<OutputCharacterType>& data, std::span<const SeparatorCharacterType> separator)
{
    StringImpl::copyCharacters(data, separator);
    skip(data, separator.size());
}

template<std::integral IntegerType>
    requires (sizeof(IntegerType) == 1)
struct ByteIntegerStrings {
    constexpr ByteIntegerStrings()
    {
        for (unsigned i = 0; i < 256; ++i) {
            IntegerType value = static_cast<IntegerType>(i);
            lengths[i] = lengthOfIntegerAsString(value);
            int magnitude = value < 0 ? -static_cast<int>(value) : static_cast<int>(value);
            unsigned index = lengths[i];
            do {
                characters[i][--index] = '0' + magnitude % 10;
                magnitude /= 10;
            } while (magnitude);
            if (value < 0)
                characters[i][0] = '-';
        }
    }

    std::array<std::array<Latin1Character, 4>, 256> characters { };
    std::array<uint8_t, 256> lengths { };
};

template<std::integral IntegerType>
static constexpr ByteIntegerStrings<IntegerType> byteIntegerStrings;

template<typename CharacterType, std::integral IntegerType>
static inline void appendStringToData(std::span<CharacterType>& data, IntegerType value)
{
    if constexpr (std::is_same_v<CharacterType, Latin1Character> && sizeof(IntegerType) == 1) {
        uint8_t index = static_cast<uint8_t>(value);
        auto& characters = byteIntegerStrings<IntegerType>.characters[index];
        unsigned length = byteIntegerStrings<IntegerType>.lengths[index];
        if (data.size() >= characters.size()) [[likely]]
            memcpySpan(data.first(characters.size()), std::span { characters });
        else
            memcpySpan(data, std::span { characters }.first(length));
        skip(data, length);
    } else if constexpr (std::is_same_v<CharacterType, Latin1Character>) {
        auto result = std::to_chars(std::bit_cast<char*>(data.data()), std::bit_cast<char*>(std::to_address(data.end())), value);
        ASSERT(result.ec != std::errc::value_too_large);
        skip(data, result.ptr - std::bit_cast<char*>(data.data()));
    } else {
        WTF::StringTypeAdapter<IntegerType> adapter { value };
        adapter.writeTo(data);
        skip(data, adapter.length());
    }
}

template<std::integral IntegerType>
static constexpr size_t maxIntegerStringLength = std::max(lengthOfIntegerAsString(std::numeric_limits<IntegerType>::min()), lengthOfIntegerAsString(std::numeric_limits<IntegerType>::max()));

// Requires numberToShortestStringBufferSize characters of room even though fewer are consumed.
template<typename CharacterType>
static inline void appendStringToData(std::span<CharacterType>& data, double value)
{
    if (auto int32Value = tryConvertToStrictInt32(value)) {
        appendStringToData(data, *int32Value);
        return;
    }
    if (isSafeInteger(value)) {
        appendStringToData(data, static_cast<int64_t>(value));
        return;
    }
    if constexpr (std::is_same_v<CharacterType, Latin1Character>) {
        size_t length = numberToShortestString(value, std::span<char, numberToShortestStringBufferSize> { std::bit_cast<char*>(data.data()), numberToShortestStringBufferSize });
        skip(data, length);
    } else {
        std::array<char, numberToShortestStringBufferSize> buffer;
        size_t length = numberToShortestString(value, buffer);
        StringImpl::copyCharacters(data, byteCast<Latin1Character>(std::span { buffer }.first(length)));
        skip(data, length);
    }
}

template<typename CharacterType>
static inline void appendStringToDataWithOneCharacterSeparatorRepeatedly(std::span<CharacterType>& data, char16_t separatorCharacter, StringView string, unsigned count)
{
#if OS(DARWIN)
    if constexpr (std::is_same_v<CharacterType, Latin1Character>) {
        ASSERT(string.is8Bit());
        if (count > 4) {
            switch (string.length() + 1) {
            case 16: {
                alignas(16) Latin1Character pattern[16];
                pattern[0] = separatorCharacter;
                string.getCharacters8(std::span { pattern }.subspan(1));
                size_t fillLength = count * 16;
                memset_pattern16(data.data(), pattern, fillLength);
                skip(data, fillLength);
                return;
            }
            case 8: {
                alignas(8) Latin1Character pattern[8];
                pattern[0] = separatorCharacter;
                string.getCharacters8(std::span { pattern }.subspan(1));
                size_t fillLength = count * 8;
                memset_pattern8(data.data(), pattern, fillLength);
                skip(data, fillLength);
                return;
            }
            case 4: {
                alignas(4) Latin1Character pattern[4];
                pattern[0] = separatorCharacter;
                string.getCharacters8(std::span { pattern }.subspan(1));
                size_t fillLength = count * 4;
                memset_pattern4(data.data(), pattern, fillLength);
                skip(data, fillLength);
                return;
            }
            default:
                break;
            }
        }
    }
#endif

    while (count--) {
        consume(data) = separatorCharacter;
        appendStringToData(data, string);
    }
}

template<typename OutputCharacterType, typename SeparatorCharacterType>
static inline RefPtr<StringImpl> joinStrings(const JSStringJoiner::Entries& strings, std::span<const SeparatorCharacterType> separator, unsigned joinedLength)
{
    ASSERT(joinedLength);

    std::span<OutputCharacterType> data;
    RefPtr<StringImpl> result = StringImpl::tryCreateUninitialized(joinedLength, data);
    if (!result) [[unlikely]]
        return result;

    unsigned size = strings.size();

    switch (separator.size()) {
    case 0: {
        for (unsigned i = 0; i < size; ++i) {
            const auto& entry = strings[i];
            unsigned count = entry.m_additional;
            do {
                appendStringToData(data, entry.m_view.view);
            } while (count--);
        }
        break;
    }
    case 1: {
        OutputCharacterType separatorCharacter = separator.data()[0];
        {
            const auto& entry = strings[0];
            unsigned count = entry.m_additional;
            appendStringToData(data, entry.m_view.view);
            appendStringToDataWithOneCharacterSeparatorRepeatedly(data, separatorCharacter, entry.m_view.view, count);
        }
        for (unsigned i = 1; i < size; ++i) {
            const auto& entry = strings[i];
            unsigned count = entry.m_additional;
            appendStringToDataWithOneCharacterSeparatorRepeatedly(data, separatorCharacter, entry.m_view.view, count + 1);
        }
        break;
    }
    default: {
        {
            const auto& entry = strings[0];
            unsigned count = entry.m_additional;
            appendStringToData(data, entry.m_view.view);
            while (count--) {
                appendStringToData(data, separator);
                appendStringToData(data, entry.m_view.view);
            }
        }
        for (unsigned i = 1; i < size; ++i) {
            const auto& entry = strings[i];
            unsigned count = entry.m_additional;
            do {
                appendStringToData(data, separator);
                appendStringToData(data, entry.m_view.view);
            } while (count--);
        }
        break;
    }
    }
    ASSERT(data.data() == span<OutputCharacterType>(result).data() + joinedLength);

    return result;
}

template<IndexingType indexingShape, typename OutputCharacterType, typename SeparatorCharacterType>
static inline RefPtr<StringImpl> joinStrings(JSGlobalObject* globalObject, const WriteBarrier<Unknown>* strings, unsigned size, std::span<const SeparatorCharacterType> separator, unsigned joinedLength)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!joinedLength)
        return emptyString();

    std::span<OutputCharacterType> data;
    RefPtr<StringImpl> result = StringImpl::tryCreateUninitialized(joinedLength, data);
    if (!result) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }

    auto appendValue = [&](JSValue value) ALWAYS_INLINE_LAMBDA {
        if constexpr (indexingShape == ContiguousShape) {
            if (value.isString()) {
                JSString* string = asString(value);
                if (!string->isNonSubstringRope()) [[likely]] {
                    appendStringToData(data, string->tryGetView());
                    return;
                }
                unsigned length = string->length();
                string->resolveToBuffer(data.first(length));
                skip(data, length);
                return;
            }
            if (value.isBoolean()) {
                appendStringToData(data, StringView(value.isTrue() ? "true"_s : "false"_s));
                return;
            }
            if (value.isUndefinedOrNull())
                return;
        }
        ASSERT(value.isInt32());
        appendStringToData(data, value.asInt32());
    };

    switch (separator.size()) {
    case 0: {
        for (unsigned i = 0; i < size; ++i)
            appendValue(strings[i].get());
        break;
    }
    default: {
        appendValue(strings[0].get());
        for (unsigned i = 1; i < size; ++i) {
            appendStringToData(data, separator);
            appendValue(strings[i].get());
        }
        break;
    }
    }
    ASSERT(data.data() == span<OutputCharacterType>(result).data() + joinedLength);

    return result;
}

inline unsigned JSStringJoiner::joinedLength(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!m_stringsCount)
        return 0;

    CheckedInt32 separatorLength = m_separator.length();
    CheckedInt32 totalSeparatorsLength = separatorLength * (m_stringsCount - 1);
    CheckedInt32 totalLength = totalSeparatorsLength + m_accumulatedStringsLength;
    if (totalLength.hasOverflowed()) {
        throwOutOfMemoryError(globalObject, scope);
        return 0;
    }
    return totalLength;
}

JSString* JSStringJoiner::joinImpl(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (m_hasOverflowed) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }
    ASSERT(m_strings.size() <= m_strings.capacity());

    unsigned length = joinedLength(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (!length)
        return jsEmptyString(vm);

    RefPtr<StringImpl> result;
    if (m_isAll8Bit)
        result = joinStrings<Latin1Character>(m_strings, m_separator.span8(), length);
    else {
        if (m_separator.is8Bit())
            result = joinStrings<char16_t>(m_strings, m_separator.span8(), length);
        else
            result = joinStrings<char16_t>(m_strings, m_separator.span16(), length);
    }

    if (!result) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }

    return jsString(vm, WTF::move(result));
}

template<IndexingType indexingShape>
JSString* JSExactLengthJoiner::joinImpl(JSGlobalObject* globalObject, const WriteBarrier<Unknown>* data, unsigned length)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!length)
        return jsEmptyString(vm);

    CheckedInt32 separatorLength = m_separator.length();
    CheckedInt32 totalSeparatorsLength = separatorLength * (CheckedInt32(length) - 1);
    CheckedInt32 totalLength = totalSeparatorsLength + m_accumulatedStringsLength;
    if (totalLength.hasOverflowed()) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }

    RefPtr<StringImpl> result;
    if (m_isAll8Bit)
        result = joinStrings<indexingShape, Latin1Character>(globalObject, data, length, m_separator.span8(), totalLength);
    else {
        if (m_separator.is8Bit())
            result = joinStrings<indexingShape, char16_t>(globalObject, data, length, m_separator.span8(), totalLength);
        else
            result = joinStrings<indexingShape, char16_t>(globalObject, data, length, m_separator.span16(), totalLength);
    }

    RETURN_IF_EXCEPTION(scope, { });

    return jsString(vm, WTF::move(result));
}

template JSString* JSExactLengthJoiner::joinImpl<Int32Shape>(JSGlobalObject*, const WriteBarrier<Unknown>*, unsigned);
template JSString* JSExactLengthJoiner::joinImpl<ContiguousShape>(JSGlobalObject*, const WriteBarrier<Unknown>*, unsigned);

template<std::integral ElementType>
JSString* joinIntegerTypedArray(JSGlobalObject* globalObject, std::span<const ElementType> elements, size_t length, StringView separator)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(elements.size() <= length);

    static_assert(MAX_ARRAY_BUFFER_SIZE / sizeof(ElementType) <= std::numeric_limits<uint64_t>::max() / maxIntegerStringLength<ElementType>);
    uint64_t accumulatedStringsLength = 0;
    for (auto element : elements) {
        if constexpr (sizeof(ElementType) == 1)
            accumulatedStringsLength += byteIntegerStrings<ElementType>.lengths[static_cast<uint8_t>(element)];
        else
            accumulatedStringsLength += lengthOfIntegerAsString(element);
    }

    CheckedInt32 totalSeparatorsLength = CheckedInt32(separator.length()) * (CheckedInt32(std::max<size_t>(length, 1)) - 1);
    CheckedInt32 totalLength = totalSeparatorsLength + CheckedInt32(accumulatedStringsLength);
    if (totalLength.hasOverflowed()) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }

    if (!totalLength)
        return jsEmptyString(vm);

    auto join = [&]<typename CharacterType>(std::span<const CharacterType> separatorCharacters) -> JSString* {
        std::span<CharacterType> data;
        RefPtr<StringImpl> result = StringImpl::tryCreateUninitialized(totalLength, data);
        if (!result) [[unlikely]] {
            throwOutOfMemoryError(globalObject, scope);
            return { };
        }

        auto appendAll = [&](auto appendSeparator) ALWAYS_INLINE_LAMBDA {
            if (!elements.empty()) {
                appendStringToData(data, elements[0]);
                for (auto element : elements.subspan(1)) {
                    appendSeparator();
                    appendStringToData(data, element);
                }
            }
            for (size_t i = std::max<size_t>(elements.size(), 1); i < length; ++i)
                appendSeparator();
        };

        switch (separatorCharacters.size()) {
        case 0:
            for (auto element : elements)
                appendStringToData(data, element);
            break;
        case 1: {
            CharacterType separatorCharacter = separatorCharacters[0];
            appendAll([&] ALWAYS_INLINE_LAMBDA {
                consume(data) = separatorCharacter;
            });
            break;
        }
        default:
            appendAll([&] ALWAYS_INLINE_LAMBDA {
                appendStringToData(data, separatorCharacters);
            });
            break;
        }
        RELEASE_ASSERT(data.empty());

        return jsString(vm, WTF::move(result));
    };

    if (separator.is8Bit())
        return join(separator.span8());
    return join(separator.span16());
}

template JSString* joinIntegerTypedArray<int8_t>(JSGlobalObject*, std::span<const int8_t>, size_t, StringView);
template JSString* joinIntegerTypedArray<uint8_t>(JSGlobalObject*, std::span<const uint8_t>, size_t, StringView);
template JSString* joinIntegerTypedArray<int16_t>(JSGlobalObject*, std::span<const int16_t>, size_t, StringView);
template JSString* joinIntegerTypedArray<uint16_t>(JSGlobalObject*, std::span<const uint16_t>, size_t, StringView);
template JSString* joinIntegerTypedArray<int32_t>(JSGlobalObject*, std::span<const int32_t>, size_t, StringView);
template JSString* joinIntegerTypedArray<uint32_t>(JSGlobalObject*, std::span<const uint32_t>, size_t, StringView);
template JSString* joinIntegerTypedArray<int64_t>(JSGlobalObject*, std::span<const int64_t>, size_t, StringView);
template JSString* joinIntegerTypedArray<uint64_t>(JSGlobalObject*, std::span<const uint64_t>, size_t, StringView);

template<typename ElementType, typename CharacterType>
class JSTypedArrayJoiner {
    WTF_FORBID_HEAP_ALLOCATION;
public:
    JSTypedArrayJoiner(std::span<const CharacterType> separator)
        : m_separator(separator)
    {
    }

    JSString* join(JSGlobalObject*, std::span<const ElementType> elements, size_t length);

private:
    static constexpr size_t maxElementStringLength = [] {
        if constexpr (std::is_integral_v<ElementType>)
            return maxIntegerStringLength<ElementType>;
        else
            return numberToShortestStringBufferSize;
    }();
    static constexpr size_t sampledElementCount = 64;
    static constexpr size_t maxBufferCapacity = (std::numeric_limits<unsigned>::max() >> 1) / sizeof(CharacterType);
    static_assert(WTF::isValidCapacityForVector<CharacterType>(maxBufferCapacity) && !WTF::isValidCapacityForVector<CharacterType>(maxBufferCapacity + 1));

    enum class SeparatorKind : uint8_t { None, OneCharacter, Characters };

    template<SeparatorKind> bool appendElements(std::span<const ElementType>);
    template<SeparatorKind> bool append(ElementType);
    bool appendSeparators(size_t count);

    ALWAYS_INLINE bool ensureRoom(size_t room)
    {
        if (m_buffer.size() - m_length >= room) [[likely]]
            return true;
        return expandBuffer(room);
    }

    NEVER_INLINE bool expandBuffer(size_t room)
    {
        if (room > maxBufferCapacity - m_length)
            return false;
        size_t capacity = m_buffer.capacity();
        size_t newCapacity = std::max(m_length + room, std::min(capacity + capacity / 2, maxBufferCapacity));
        m_buffer.shrink(m_length);
        if (!m_buffer.tryReserveCapacity(newCapacity))
            return false;
        m_buffer.grow(m_buffer.capacity());
        return true;
    }

    template<SeparatorKind kind>
    ALWAYS_INLINE size_t separatorLength() const
    {
        if constexpr (kind == SeparatorKind::None)
            return 0;
        else if constexpr (kind == SeparatorKind::OneCharacter)
            return 1;
        else
            return m_separator.size();
    }

    template<SeparatorKind kind>
    ALWAYS_INLINE void appendSeparator(std::span<CharacterType>& data) const
    {
        if constexpr (kind == SeparatorKind::OneCharacter)
            consume(data) = m_separator[0];
        else if constexpr (kind == SeparatorKind::Characters)
            appendStringToData(data, m_separator);
    }

    std::span<const CharacterType> m_separator;
    Vector<CharacterType, 0, CrashOnOverflow, 16, WTF::StringImplMalloc> m_buffer;
    size_t m_length { 0 };
};

template<typename ElementType, typename CharacterType>
template<typename JSTypedArrayJoiner<ElementType, CharacterType>::SeparatorKind kind>
bool JSTypedArrayJoiner<ElementType, CharacterType>::appendElements(std::span<const ElementType> elements)
{
    ASSERT(!elements.empty());
    auto sampled = elements.first(std::min(elements.size(), sampledElementCount));
    if (!ensureRoom(sampled.size() * (separatorLength<kind>() + maxElementStringLength))) [[unlikely]]
        return false;

    if (!append<SeparatorKind::None>(sampled[0])) [[unlikely]]
        return false;
    for (auto element : sampled.subspan(1)) {
        if (!append<kind>(element)) [[unlikely]]
            return false;
    }

    auto remaining = elements.subspan(sampled.size());
    if (remaining.empty())
        return true;

    CheckedSize estimatedLength = m_length;
    estimatedLength *= elements.size();
    estimatedLength /= sampled.size();
    estimatedLength += estimatedLength / static_cast<size_t>(16);
    m_buffer.shrink(m_length);
    if (!estimatedLength.hasOverflowed())
        m_buffer.tryReserveCapacity(std::min<size_t>(estimatedLength, maxBufferCapacity));
    m_buffer.grow(m_buffer.capacity());

    for (auto element : remaining) {
        if (!append<kind>(element)) [[unlikely]]
            return false;
    }
    return true;
}

template<typename ElementType, typename CharacterType>
template<typename JSTypedArrayJoiner<ElementType, CharacterType>::SeparatorKind kind>
ALWAYS_INLINE bool JSTypedArrayJoiner<ElementType, CharacterType>::append(ElementType element)
{
    if (!ensureRoom(separatorLength<kind>() + maxElementStringLength)) [[unlikely]]
        return false;

    auto data = m_buffer.mutableSpan().subspan(m_length);
    appendSeparator<kind>(data);
    if constexpr (std::is_integral_v<ElementType>)
        appendStringToData(data, element);
    else
        appendStringToData(data, static_cast<double>(element));

    m_length = m_buffer.size() - data.size();
    return m_length <= StringImpl::MaxLength;
}

template<typename ElementType, typename CharacterType>
bool JSTypedArrayJoiner<ElementType, CharacterType>::appendSeparators(size_t count)
{
    if (m_separator.empty())
        return true;

    CheckedSize room = m_separator.size();
    room *= count;
    if (room.hasOverflowed() || room > StringImpl::MaxLength - m_length) [[unlikely]]
        return false;

    if (!ensureRoom(room)) [[unlikely]]
        return false;

    auto data = m_buffer.mutableSpan().subspan(m_length);
    while (count--)
        appendSeparator<SeparatorKind::Characters>(data);
    m_length = m_buffer.size() - data.size();
    return true;
}

template<typename ElementType, typename CharacterType>
JSString* JSTypedArrayJoiner<ElementType, CharacterType>::join(JSGlobalObject* globalObject, std::span<const ElementType> elements, size_t length)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(elements.size() <= length);
    if (!length)
        return jsEmptyString(vm);

    auto outOfMemory = [&] -> JSString* {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    };

    if (!elements.empty()) {
        bool succeeded = true;
        switch (m_separator.size()) {
        case 0:
            succeeded = appendElements<SeparatorKind::None>(elements);
            break;
        case 1:
            succeeded = appendElements<SeparatorKind::OneCharacter>(elements);
            break;
        default:
            succeeded = appendElements<SeparatorKind::Characters>(elements);
            break;
        }
        if (!succeeded) [[unlikely]]
            return outOfMemory();
    }

    if (!appendSeparators(length - std::max<size_t>(elements.size(), 1))) [[unlikely]]
        return outOfMemory();

    if (!m_length)
        return jsEmptyString(vm);

    m_buffer.shrink(m_length);
    m_buffer.shrinkToFit();
    return jsString(vm, StringImpl::adopt(WTF::move(m_buffer)));
}

template<typename ElementType>
JSString* joinTypedArray(JSGlobalObject* globalObject, std::span<const ElementType> elements, size_t length, StringView separator)
{
    if (separator.is8Bit())
        return JSTypedArrayJoiner<ElementType, Latin1Character>(separator.span8()).join(globalObject, elements, length);
    return JSTypedArrayJoiner<ElementType, char16_t>(separator.span16()).join(globalObject, elements, length);
}

template JSString* joinTypedArray<int8_t>(JSGlobalObject*, std::span<const int8_t>, size_t, StringView);
template JSString* joinTypedArray<uint8_t>(JSGlobalObject*, std::span<const uint8_t>, size_t, StringView);
template JSString* joinTypedArray<int16_t>(JSGlobalObject*, std::span<const int16_t>, size_t, StringView);
template JSString* joinTypedArray<uint16_t>(JSGlobalObject*, std::span<const uint16_t>, size_t, StringView);
template JSString* joinTypedArray<int32_t>(JSGlobalObject*, std::span<const int32_t>, size_t, StringView);
template JSString* joinTypedArray<uint32_t>(JSGlobalObject*, std::span<const uint32_t>, size_t, StringView);
template JSString* joinTypedArray<int64_t>(JSGlobalObject*, std::span<const int64_t>, size_t, StringView);
template JSString* joinTypedArray<uint64_t>(JSGlobalObject*, std::span<const uint64_t>, size_t, StringView);
template JSString* joinTypedArray<Float16>(JSGlobalObject*, std::span<const Float16>, size_t, StringView);
template JSString* joinTypedArray<float>(JSGlobalObject*, std::span<const float>, size_t, StringView);
template JSString* joinTypedArray<double>(JSGlobalObject*, std::span<const double>, size_t, StringView);

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
