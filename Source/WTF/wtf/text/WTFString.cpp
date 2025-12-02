/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <wtf/text/WTFString.h>

#include <wtf/ASCIICType.h>
#include <wtf/DataLog.h>
#include <wtf/HexNumber.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>
#include <wtf/dtoa.h>
#include <wtf/text/CString.h>
#include <wtf/text/IntegerToStringConversion.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/unicode/CharacterNames.h>
#include <wtf/unicode/UTF8Conversion.h>

namespace WTF {

// Construct a string with UTF-16 data.
String::String(std::span<const char16_t> characters)
    : m_impl(characters.data() ? RefPtr { StringImpl::create(characters) } : nullptr)
{
}

// Construct a string with latin1 data.
String::String(std::span<const Latin1Character> characters)
    : m_impl(characters.data() ? RefPtr { StringImpl::create(characters) } : nullptr)
{
}

// Construct a string with UTF-8 data.
String::String(std::span<const char8_t> characters)
    : m_impl(StringImpl::create(characters))
{
}

// Construct a string with Latin-1 data.
String::String(std::span<const char> characters)
    : m_impl(characters.data() ? RefPtr { StringImpl::create(byteCast<Latin1Character>(characters)) } : nullptr)
{
}

// Construct a string with Latin-1 data, from a null-terminated source.
String::String(const char* nullTerminatedString)
    : m_impl(nullTerminatedString ? RefPtr { StringImpl::create(byteCast<Latin1Character>(unsafeSpan(nullTerminatedString))) } : nullptr)
{
}

std::strong_ordering codePointCompare(const String& a, const String& b)
{
    return codePointCompare(a.impl(), b.impl());
}

char32_t String::characterStartingAt(unsigned i) const
{
    if (!m_impl || i >= m_impl->length())
        return 0;
    SUPPRESS_UNCOUNTED_ARG return m_impl->characterStartingAt(i);
}

String makeStringByJoining(std::span<const String> strings, const String& separator)
{
    StringBuilder builder;
    for (const auto& string : strings) {
        if (builder.isEmpty())
            builder.append(string);
        else
            builder.append(separator, string);
    }
    return builder.toString();
}

String makeStringByRemoving(const String& string, unsigned position, unsigned lengthToRemove)
{
    if (!lengthToRemove)
        return string;
    auto length = string.length();
    if (position >= length)
        return string;
    lengthToRemove = std::min(lengthToRemove, length - position);
    StringView view { string };
    return makeString(view.left(position), view.substring(position + lengthToRemove));
}

String String::substringSharingImpl(unsigned offset, unsigned length) const
{
    // FIXME: We used to check against a limit of Heap::minExtraCost / sizeof(char16_t).

    unsigned stringLength = this->length();
    offset = std::min(offset, stringLength);
    length = std::min(length, stringLength - offset);

    if (!offset && length == stringLength)
        return *this;
    SUPPRESS_UNCOUNTED_ARG return StringImpl::createSubstringSharingImpl(*m_impl, offset, length);
}

String String::convertToASCIILowercase() const
{
    // FIXME: Should this function, and the many others like it, be inlined?
    SUPPRESS_UNCOUNTED_ARG return m_impl ? m_impl->convertToASCIILowercase() : String { };
}

String String::convertToASCIIUppercase() const
{
    // FIXME: Should this function, and the many others like it, be inlined?
    SUPPRESS_UNCOUNTED_ARG return m_impl ? m_impl->convertToASCIIUppercase() : String { };
}

String String::convertToLowercaseWithoutLocale() const
{
    // FIXME: Should this function, and the many others like it, be inlined?
    SUPPRESS_UNCOUNTED_ARG return m_impl ? m_impl->convertToLowercaseWithoutLocale() : String { };
}

String String::convertToLowercaseWithoutLocaleStartingAtFailingIndex8Bit(unsigned failingIndex) const
{
    // FIXME: Should this function, and the many others like it, be inlined?
    SUPPRESS_UNCOUNTED_ARG return m_impl ? m_impl->convertToLowercaseWithoutLocaleStartingAtFailingIndex8Bit(failingIndex) : String { };
}

String String::convertToUppercaseWithoutLocale() const
{
    // FIXME: Should this function, and the many others like it, be inlined?
    SUPPRESS_UNCOUNTED_ARG return m_impl ? m_impl->convertToUppercaseWithoutLocale() : String { };
}

String String::convertToLowercaseWithLocale(const AtomString& localeIdentifier) const
{
    // FIXME: Should this function, and the many others like it, be inlined?
    SUPPRESS_UNCOUNTED_ARG return m_impl ? m_impl->convertToLowercaseWithLocale(localeIdentifier) : String { };
}

String String::convertToUppercaseWithLocale(const AtomString& localeIdentifier) const
{
    // FIXME: Should this function, and the many others like it, be inlined?
    SUPPRESS_UNCOUNTED_ARG return m_impl ? m_impl->convertToUppercaseWithLocale(localeIdentifier) : String { };
}

String String::trim(CodeUnitMatchFunction predicate) const
{
    // FIXME: Should this function, and the many others like it, be inlined?
    SUPPRESS_UNCOUNTED_ARG return m_impl ? m_impl->trim(predicate) : String { };
}

String String::simplifyWhiteSpace(CodeUnitMatchFunction isWhiteSpace) const
{
    // FIXME: Should this function, and the many others like it, be inlined?
    SUPPRESS_UNCOUNTED_ARG return m_impl ? m_impl->simplifyWhiteSpace(isWhiteSpace) : String { };
}

String String::foldCase() const
{
    // FIXME: Should this function, and the many others like it, be inlined?
    SUPPRESS_UNCOUNTED_ARG return m_impl ? m_impl->foldCase() : String { };
}

Expected<Vector<char16_t>, UTF8ConversionError> String::charactersWithoutNullTermination() const
{
    Vector<char16_t> result;
    if (!m_impl)
        return result;

    if (!result.tryReserveInitialCapacity(length() + 1))
        return makeUnexpected(UTF8ConversionError::OutOfMemory);

    if (is8Bit())
        result.append(span8());
    else
        result.append(span16());

    return result;
}

Expected<Vector<char16_t>, UTF8ConversionError> String::charactersWithNullTermination() const
{
    auto result = charactersWithoutNullTermination();
    if (result)
        result.value().append(0);
    return result;
}

String String::number(int number)
{
    return numberToStringSigned<String>(number);
}

String String::number(unsigned number)
{
    return numberToStringUnsigned<String>(number);
}

String String::number(long number)
{
    return numberToStringSigned<String>(number);
}

String String::number(unsigned long number)
{
    return numberToStringUnsigned<String>(number);
}

String String::number(long long number)
{
    return numberToStringSigned<String>(number);
}

String String::number(unsigned long long number)
{
    return numberToStringUnsigned<String>(number);
}

String String::numberToStringFixedPrecision(float number, unsigned precision, TrailingZerosPolicy trailingZerosTruncatingPolicy)
{
    NumberToStringBuffer buffer;
    return String { numberToFixedPrecisionString(number, precision, buffer, trailingZerosTruncatingPolicy == TrailingZerosPolicy::Truncate) };
}

String String::numberToStringFixedPrecision(double number, unsigned precision, TrailingZerosPolicy trailingZerosTruncatingPolicy)
{
    NumberToStringBuffer buffer;
    return String { numberToFixedPrecisionString(number, precision, buffer, trailingZerosTruncatingPolicy == TrailingZerosPolicy::Truncate) };
}

String String::number(float number)
{
    NumberToStringBuffer buffer;
    return String { numberToStringAndSize(number, buffer) };
}

String String::number(double number)
{
    NumberToStringBuffer buffer;
    return String { numberToStringAndSize(number, buffer) };
}

String String::numberToStringFixedWidth(double number, unsigned decimalPlaces)
{
    NumberToStringBuffer buffer;
    return String { numberToFixedWidthString(number, decimalPlaces, buffer) };
}

double String::toDouble(bool* ok) const
{
    if (!m_impl) {
        if (ok)
            *ok = false;
        return 0.0;
    }
    SUPPRESS_UNCOUNTED_ARG return m_impl->toDouble(ok);
}

float String::toFloat(bool* ok) const
{
    if (!m_impl) {
        if (ok)
            *ok = false;
        return 0.0f;
    }
    SUPPRESS_UNCOUNTED_ARG return m_impl->toFloat(ok);
}

String String::isolatedCopy() const &
{
    // FIXME: Should this function, and the many others like it, be inlined?
    SUPPRESS_UNCOUNTED_ARG return m_impl ? m_impl->isolatedCopy() : String { };
}

String String::isolatedCopy() &&
{
    if (isSafeToSendToAnotherThread()) {
        // Since we know that our string is a temporary that will be destroyed
        // we can just steal the m_impl from it, thus avoiding a copy.
        return { WTFMove(*this) };
    }

    SUPPRESS_UNCOUNTED_ARG return m_impl ? m_impl->isolatedCopy() : String { };
}

bool String::isSafeToSendToAnotherThread() const
{
    // AtomStrings are not safe to send between threads, as ~StringImpl()
    // will try to remove them from the wrong AtomStringTable.
    return isEmpty() || (m_impl->hasOneRef() && !m_impl->isAtom());
}

template<bool allowEmptyEntries>
inline Vector<String> String::splitInternal(StringView separator) const
{
    Vector<String> result;

    unsigned startPos = 0;
    size_t endPos;
    while ((endPos = find(separator, startPos)) != notFound) {
        if (allowEmptyEntries || startPos != endPos)
            result.append(substring(startPos, endPos - startPos));
        startPos = endPos + separator.length();
    }
    if (allowEmptyEntries || startPos != length())
        result.append(substring(startPos));

    return result;
}

template<bool allowEmptyEntries>
inline void String::splitInternal(char16_t separator, NOESCAPE const SplitFunctor& functor) const
{
    StringView view(*this);

    unsigned startPos = 0;
    size_t endPos;
    while ((endPos = find(separator, startPos)) != notFound) {
        if (allowEmptyEntries || startPos != endPos)
            functor(view.substring(startPos, endPos - startPos));
        startPos = endPos + 1;
    }
    if (allowEmptyEntries || startPos != length())
        functor(view.substring(startPos));
}

template<bool allowEmptyEntries>
inline Vector<String> String::splitInternal(char16_t separator) const
{
    Vector<String> result;
    splitInternal<allowEmptyEntries>(separator, [&result](StringView item) {
        result.append(item.toString());
    });

    return result;
}

void String::split(char16_t separator, NOESCAPE const SplitFunctor& functor) const
{
    splitInternal<false>(separator, functor);
}

Vector<String> String::split(char16_t separator) const
{
    return splitInternal<false>(separator);
}

Vector<String> String::split(StringView separator) const
{
    return splitInternal<false>(separator);
}

void String::splitAllowingEmptyEntries(char16_t separator, NOESCAPE const SplitFunctor& functor) const
{
    splitInternal<true>(separator, functor);
}

Vector<String> String::splitAllowingEmptyEntries(char16_t separator) const
{
    return splitInternal<true>(separator);
}

Vector<String> String::splitAllowingEmptyEntries(StringView separator) const
{
    return splitInternal<true>(separator);
}

CString String::ascii() const
{
    // Printable ASCII characters 32..127 and the null character are
    // preserved, characters outside of this range are converted to '?'.

    if (isEmpty()) {
        std::span<char> characterBuffer;
        return CString::newUninitialized(0, characterBuffer);
    }

    if (this->is8Bit()) {
        auto characters = this->span8();

        std::span<char> characterBuffer;
        CString result = CString::newUninitialized(characters.size(), characterBuffer);

        size_t characterBufferIndex = 0;
        for (auto character : characters)
            characterBuffer[characterBufferIndex++] = character && (character < 0x20 || character > 0x7f) ? '?' : byteCast<char>(character);

        return result;        
    }

    auto characters = span16();
    std::span<char> characterBuffer;
    CString result = CString::newUninitialized(characters.size(), characterBuffer);

    size_t characterBufferIndex = 0;
    for (auto character : characters)
        characterBuffer[characterBufferIndex++] = character && (character < 0x20 || character > 0x7f) ? '?' : character;

    return result;
}

CString String::latin1() const
{
    // Basic Latin1 (ISO) encoding - Unicode characters 0..255 are
    // preserved, characters outside of this range are converted to '?'.

    if (isEmpty())
        return ""_span;

    if (is8Bit())
        return CString(this->span8());

    auto characters = this->span16();
    std::span<char> characterBuffer;
    CString result = CString::newUninitialized(characters.size(), characterBuffer);

    size_t characterBufferIndex = 0;
    for (auto character : characters)
        characterBuffer[characterBufferIndex++] = !isLatin1(character) ? '?' : character;

    return result;
}

Expected<CString, UTF8ConversionError> String::tryGetUTF8(ConversionMode mode) const
{
    SUPPRESS_UNCOUNTED_ARG return m_impl ? m_impl->tryGetUTF8(mode) : CString { ""_span };
}

Expected<CString, UTF8ConversionError> String::tryGetUTF8() const
{
    return tryGetUTF8(LenientConversion);
}

CString String::utf8(ConversionMode mode) const
{
    Expected<CString, UTF8ConversionError> expectedString = tryGetUTF8(mode);
    RELEASE_ASSERT(expectedString);
    return expectedString.value();
}

String String::make8Bit(std::span<const char16_t> source)
{
    std::span<Latin1Character> destination;
    String result = String::createUninitialized(source.size(), destination);
    StringImpl::copyCharacters(destination, source);
    return result;
}

void String::convertTo16Bit()
{
    if (isNull() || !is8Bit())
        return;
    std::span<char16_t> destination;
    auto convertedString = String::createUninitialized(length(), destination);
    StringImpl::copyCharacters(destination, span8());
    *this = WTFMove(convertedString);
}

String String::fromUTF8(std::span<const char8_t> codeUnits)
{
    return codeUnits;
}

String String::fromUTF8ReplacingInvalidSequences(std::span<const char8_t> string)
{
    if (!string.data())
        return { };

    RELEASE_ASSERT(string.size() <= String::MaxLength);

    if (string.empty())
        return emptyString();

    if (charactersAreAllASCII(string))
        return StringImpl::create(byteCast<Latin1Character>(string));

    Vector<char16_t, 1024> buffer(string.size());

    auto result = Unicode::convertReplacingInvalidSequences(string, buffer.mutableSpan());
    if (result.code != Unicode::ConversionResultCode::Success)
        return { };

    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(result.buffer.size() <= string.size());
    return StringImpl::create(result.buffer);
}

String String::fromUTF8WithLatin1Fallback(std::span<const char8_t> string)
{
    String utf8 { string };
    if (!utf8) {
        // Do this assertion before chopping the size_t down to unsigned.
        RELEASE_ASSERT(string.size() <= String::MaxLength);
        return byteCast<Latin1Character>(string);
    }
    return utf8;
}

String String::fromCodePoint(char32_t codePoint)
{
    std::array<char16_t, 2> buffer;
    uint8_t length = 0;
    UBool error = false;
    U16_APPEND(buffer, length, 2, codePoint, error);
    return error ? String() : String(std::span { buffer }.first(length));
}

// String Operations

template<typename CharacterType, TrailingJunkPolicy policy>
static inline double toDoubleType(std::span<const CharacterType> data, bool* ok, size_t& parsedLength)
{
    size_t leadingSpacesLength = 0;
    while (leadingSpacesLength < data.size() && isUnicodeCompatibleASCIIWhitespace(data[leadingSpacesLength]))
        ++leadingSpacesLength;

    double number = parseDouble(data.subspan(leadingSpacesLength), parsedLength);
    if (!parsedLength) {
        if (ok)
            *ok = false;
        return 0.0;
    }

    parsedLength += leadingSpacesLength;
    if (ok)
        *ok = policy == TrailingJunkPolicy::Allow || parsedLength == data.size();
    return number;
}

double charactersToDouble(std::span<const Latin1Character> data, bool* ok)
{
    size_t parsedLength;
    return toDoubleType<Latin1Character, TrailingJunkPolicy::Disallow>(data, ok, parsedLength);
}

double charactersToDouble(std::span<const char16_t> data, bool* ok)
{
    size_t parsedLength;
    return toDoubleType<char16_t, TrailingJunkPolicy::Disallow>(data, ok, parsedLength);
}

float charactersToFloat(std::span<const Latin1Character> data, bool* ok)
{
    // FIXME: This will return ok even when the string fits into a double but not a float.
    size_t parsedLength;
    return static_cast<float>(toDoubleType<Latin1Character, TrailingJunkPolicy::Disallow>(data, ok, parsedLength));
}

float charactersToFloat(std::span<const char16_t> data, bool* ok)
{
    // FIXME: This will return ok even when the string fits into a double but not a float.
    size_t parsedLength;
    return static_cast<float>(toDoubleType<char16_t, TrailingJunkPolicy::Disallow>(data, ok, parsedLength));
}

float charactersToFloat(std::span<const Latin1Character> data, size_t& parsedLength)
{
    // FIXME: This will return ok even when the string fits into a double but not a float.
    return static_cast<float>(toDoubleType<Latin1Character, TrailingJunkPolicy::Allow>(data, nullptr, parsedLength));
}

float charactersToFloat(std::span<const char16_t> data, size_t& parsedLength)
{
    // FIXME: This will return ok even when the string fits into a double but not a float.
    return static_cast<float>(toDoubleType<char16_t, TrailingJunkPolicy::Allow>(data, nullptr, parsedLength));
}

const StaticString nullStringData { nullptr };
const StaticString emptyStringData { &StringImpl::s_emptyAtomString };

} // namespace WTF

#ifndef NDEBUG

// For use in the debugger.
String* string(const char*);
Vector<char> asciiDebug(StringImpl* impl);
Vector<char> asciiDebug(String& string);

void String::show() const
{
    dataLogF("%s\n", asciiDebug(impl()).span().data());
}

String* string(const char* s)
{
    // Intentionally leaks memory!
    return new String(String::fromLatin1(s));
}

Vector<char> asciiDebug(StringImpl* impl)
{
    if (!impl)
        return asciiDebug(String("[null]"_s).impl());

    StringBuilder buffer;
    for (unsigned i = 0; i < impl->length(); ++i) {
        char16_t ch = (*impl)[i];
        if (isASCIIPrintable(ch)) {
            if (ch == '\\')
                buffer.append(ch);
            buffer.append(ch);
        } else {
            buffer.append('\\', 'u', hex(ch, 4));
        }
    }
    CString narrowString = buffer.toString().ascii();
    return { narrowString.spanIncludingNullTerminator() };
}

Vector<char> asciiDebug(String& string)
{
    return asciiDebug(string.impl());
}

#endif
