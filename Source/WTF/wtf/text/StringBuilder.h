/*
 * Copyright (C) 2009-2024 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include <wtf/SaturatedArithmetic.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WTF {

class StringBuilder {
    // Disallow copying since we don't want to share m_buffer between two builders.
    WTF_MAKE_NONCOPYABLE(StringBuilder);
    WTF_MAKE_FAST_ALLOCATED;

public:
    StringBuilder() = default;
    StringBuilder(StringBuilder&&) = default;
    StringBuilder& operator=(StringBuilder&&) = default;

    enum class OverflowHandler { CrashOnOverflow, RecordOverflow }; // FIXME: Despite its use in Checked<>, "handler" does not seem the correct name for this.
    explicit StringBuilder(OverflowHandler);

    void clear();
    void swap(StringBuilder&);

    void didOverflow();
    bool hasOverflowed() const { return m_length > String::MaxLength; }
    bool crashesOnOverflow() const { return m_shouldCrashOnOverflow; }

    template<StringTypeAdaptable... StringTypes> void append(const StringTypes&...);

    // FIXME: We should keep these overloads only if optimizations make them more efficient than the single-argument form of the variadic append above.
    WTF_EXPORT_PRIVATE void append(std::span<const UChar>);
    WTF_EXPORT_PRIVATE void append(std::span<const LChar>);
    void append(const AtomString& string) { append(string.string()); }
    void append(const String&);
    void append(StringView);
    void append(ASCIILiteral);
    void append(const char*) = delete; // Pass ASCIILiteral or span instead.
    void append(UChar);
    void append(LChar);
    void append(char character) { append(byteCast<LChar>(character)); }

    template<typename... StringTypeAdapters> void appendFromAdapters(const StringTypeAdapters&...);

    // FIXME: Add a StringTypeAdapter so we can append one string builder to another with variadic append.
    void append(const StringBuilder&);

    void appendSubstring(const String&, unsigned offset, unsigned length = String::MaxLength);
    WTF_EXPORT_PRIVATE void appendQuotedJSONString(const String&);

    // FIXME: Unclear why toString returns String and toStringPreserveCapacity returns const String&. Make them consistent.
    String toString();
    const String& toStringPreserveCapacity() const;
    AtomString toAtomString() const;

    bool isEmpty() const { return !m_length; }
    unsigned length() const;

    operator StringView() const;
    UChar operator[](unsigned i) const;

    bool is8Bit() const;
    std::span<const LChar> span8() const { return { characters<LChar>(), length() }; }
    std::span<const UChar> span16() const { return { characters<UChar>(), length() }; }
    template<typename CharacterType> std::span<const CharacterType> span() const { return std::span(characters<CharacterType>(), length()); }
    
    unsigned capacity() const;
    WTF_EXPORT_PRIVATE void reserveCapacity(unsigned newCapacity);

    WTF_EXPORT_PRIVATE void shrink(unsigned newLength);
    WTF_EXPORT_PRIVATE bool shouldShrinkToFit() const;
    WTF_EXPORT_PRIVATE void shrinkToFit();

    WTF_EXPORT_PRIVATE bool containsOnlyASCII() const;

private:
    static unsigned expandedCapacity(unsigned capacity, unsigned requiredCapacity);
    template<typename CharacterType> const CharacterType* characters() const;

    template<typename AllocationCharacterType, typename CurrentCharacterType> void allocateBuffer(const CurrentCharacterType* currentCharacters, unsigned requiredCapacity);
    template<typename CharacterType> void reallocateBuffer(unsigned requiredCapacity);
    void reallocateBuffer(unsigned requiredCapacity);

    template<typename CharacterType> CharacterType* extendBufferForAppending(unsigned requiredLength);
    template<typename CharacterType> CharacterType* extendBufferForAppendingSlowCase(unsigned requiredLength);
    WTF_EXPORT_PRIVATE LChar* extendBufferForAppendingLChar(unsigned requiredLength);
    WTF_EXPORT_PRIVATE UChar* extendBufferForAppendingWithUpconvert(unsigned requiredLength);

    WTF_EXPORT_PRIVATE void reifyString() const;

    void appendFromAdapters() { /* empty base case */ }
    template<typename StringTypeAdapter, typename... StringTypeAdapters> void appendFromAdaptersSlow(const StringTypeAdapter&, const StringTypeAdapters&...);
    template<typename StringTypeAdapter> void appendFromAdapterSlow(const StringTypeAdapter&);

    mutable String m_string;
    RefPtr<StringImpl> m_buffer;
    unsigned m_length { 0 };
    bool m_shouldCrashOnOverflow { true };
};

template<> struct IntegerToStringConversionTrait<StringBuilder>;

// FIXME: Move this to StringView and make it take a StringView instead of a StringBuilder?
template<typename CharacterType> bool equal(const StringBuilder&, const CharacterType*, unsigned length);

// Inline function implementations.

inline StringBuilder::StringBuilder(OverflowHandler policy)
    : m_shouldCrashOnOverflow { policy == OverflowHandler::CrashOnOverflow }
{
}

inline void StringBuilder::clear()
{
    m_string = { };
    m_buffer = nullptr;
    m_length = 0;
    // We intentionally do not change m_shouldCrashOnOverflow.
}

inline void StringBuilder::swap(StringBuilder& other)
{
    m_string.swap(other.m_string);
    m_buffer.swap(other.m_buffer);
    std::swap(m_length, other.m_length);
    std::swap(m_shouldCrashOnOverflow, other.m_shouldCrashOnOverflow);
}

inline StringBuilder::operator StringView() const
{
    if (is8Bit())
        return span<LChar>();
    return span<UChar>();
}

inline void StringBuilder::append(UChar character)
{
    if (m_buffer && m_length < m_buffer->length() && m_string.isNull()) {
        if (!m_buffer->is8Bit()) {
            spanConstCast(m_buffer->span16())[m_length++] = character;
            return;
        }
        if (isLatin1(character)) {
            spanConstCast(m_buffer->span8())[m_length++] = static_cast<LChar>(character);
            return;
        }
    }
    append(WTF::span(character));
}

inline void StringBuilder::append(LChar character)
{
    if (m_buffer && m_length < m_buffer->length() && m_string.isNull()) {
        if (m_buffer->is8Bit())
            spanConstCast(m_buffer->span8())[m_length++] = character;
        else
            spanConstCast(m_buffer->span16())[m_length++] = character;
        return;
    }
    append(WTF::span(character));
}

inline void StringBuilder::append(const String& string)
{
    // If we're appending to an empty string, and there is not a buffer (reserveCapacity has not been called)
    // then just retain the string.
    if (!m_length && !m_buffer) {
        m_string = string;
        m_length = string.length();
        return;
    }

    append(StringView { string });
}

inline void StringBuilder::append(const StringBuilder& other)
{
    // If we're appending to an empty string, and there is not a buffer (reserveCapacity has not been called)
    // then just retain the string.
    if (!m_length && !m_buffer && !other.m_string.isNull()) {
        // Use the length function here so we crash on overflow without explicit overflow checks.
        m_string = other.m_string;
        m_length = other.length();
        return;
    }

    append(StringView { other });
}

inline void StringBuilder::append(StringView string)
{
    if (string.is8Bit())
        append(string.span8());
    else
        append(string.span16());
}

inline void StringBuilder::append(ASCIILiteral string)
{
    append(string.span8());
}

inline void StringBuilder::appendSubstring(const String& string, unsigned offset, unsigned length)
{
    append(StringView { string }.substring(offset, length));
}

inline String StringBuilder::toString()
{
    if (m_string.isNull()) {
        shrinkToFit();
        reifyString();
    }
    return m_string;
}

inline const String& StringBuilder::toStringPreserveCapacity() const
{
    if (m_string.isNull())
        reifyString();
    return m_string;
}

inline AtomString StringBuilder::toAtomString() const
{
    if (isEmpty())
        return emptyAtom();

    // If the buffer is sufficiently over-allocated, make a new AtomString from a copy so its buffer is not so large.
    if (shouldShrinkToFit())
        return StringView { *this }.toAtomString();

    if (!m_string.isNull())
        return AtomString { m_string };

    // Use the length function here so we crash on overflow without explicit overflow checks.
    ASSERT(m_buffer);
    return { m_buffer.get(), 0, length() };
}

inline unsigned StringBuilder::length() const
{
    RELEASE_ASSERT(!hasOverflowed());
    return m_length;
}

inline unsigned StringBuilder::capacity() const
{
    return m_buffer ? m_buffer->length() : length();
}

inline UChar StringBuilder::operator[](unsigned i) const
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(i < length());
    return is8Bit() ? characters<LChar>()[i] : characters<UChar>()[i];
}

inline bool StringBuilder::is8Bit() const
{
    return m_buffer ? m_buffer->is8Bit() : m_string.is8Bit();
}

template<typename CharacterType> inline const CharacterType* StringBuilder::characters() const
{
    if (!m_length)
        return nullptr;
    if (!m_string.isNull())
        return m_string.span<CharacterType>().data();
    return m_buffer->span<CharacterType>().data();
}

template<typename StringTypeAdapter> constexpr bool stringBuilderSlowPathRequiredForAdapter = requires(const StringTypeAdapter& adapter) {
    { adapter.writeUsing(std::declval<StringBuilder&>) } -> std::same_as<void>;
};
template<typename... StringTypeAdapters> constexpr bool stringBuilderSlowPathRequired = (... || stringBuilderSlowPathRequiredForAdapter<StringTypeAdapters>);

template<typename... StringTypeAdapters> void StringBuilder::appendFromAdapters(const StringTypeAdapters&... adapters)
{
    if constexpr (stringBuilderSlowPathRequired<StringTypeAdapters...>) {
        appendFromAdaptersSlow(adapters...);
    } else {
        auto requiredLength = saturatedSum<uint32_t>(m_length, adapters.length()...);
        if (is8Bit() && are8Bit(adapters...)) {
            auto destination = extendBufferForAppendingLChar(requiredLength);
            if (!destination)
                return;
            stringTypeAdapterAccumulator(destination, adapters...);
        } else {
            auto destination = extendBufferForAppendingWithUpconvert(requiredLength);
            if (!destination)
                return;
            stringTypeAdapterAccumulator(destination, adapters...);
        }
    }
}

template<typename StringTypeAdapter> void StringBuilder::appendFromAdapterSlow(const StringTypeAdapter& adapter)
{
    if constexpr (stringBuilderSlowPathRequired<StringTypeAdapter>) {
        adapter.writeUsing(*this);
    } else {
        appendFromAdapters(adapter);
    }
}

template<typename StringTypeAdapter, typename... StringTypeAdapters> void StringBuilder::appendFromAdaptersSlow(const StringTypeAdapter& adapter, const StringTypeAdapters&... adapters)
{
    appendFromAdapterSlow(adapter);
    appendFromAdapters(adapters...);
}

template<StringTypeAdaptable... StringTypes> void StringBuilder::append(const StringTypes&... strings)
{
    appendFromAdapters(StringTypeAdapter<StringTypes>(strings)...);
}

template<typename CharacterType> bool equal(const StringBuilder& builder, const CharacterType* buffer, unsigned length)
{
    return builder == StringView { std::span { buffer, length } };
}

template<> struct IntegerToStringConversionTrait<StringBuilder> {
    using ReturnType = void;
    using AdditionalArgumentType = StringBuilder;
    static void flush(std::span<const LChar> characters, StringBuilder* builder) { builder->append(characters); }
};

// Helper functor useful in generic contexts where both makeString() and StringBuilder are being used.
struct SerializeUsingStringBuilder {
    StringBuilder& builder;

    using Result = void;
    template<typename... T> void operator()(T&&... args)
    {
        return builder.append(std::forward<T>(args)...);
    }
};

} // namespace WTF

using WTF::StringBuilder;
using WTF::SerializeUsingStringBuilder;
