/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2019 Apple Inc. All rights reserved.
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
#include <wtf/Vector.h>
#include <wtf/dtoa.h>
#include <wtf/text/CString.h>
#include <wtf/text/IntegerToStringConversion.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/unicode/CharacterNames.h>
#include <wtf/unicode/UTF8Conversion.h>

namespace WTF {

using namespace Unicode;

// Construct a string with UTF-16 data.
String::String(const UChar* characters, unsigned length)
{
    if (characters)
        m_impl = StringImpl::create(characters, length);
}

// Construct a string with latin1 data.
String::String(const LChar* characters, unsigned length)
{
    if (characters)
        m_impl = StringImpl::create(characters, length);
}

String::String(const char* characters, unsigned length)
{
    if (characters)
        m_impl = StringImpl::create(reinterpret_cast<const LChar*>(characters), length);
}

// Construct a string with Latin-1 data, from a null-terminated source.
String::String(const char* nullTerminatedString)
{
    if (nullTerminatedString)
        m_impl = StringImpl::createFromCString(nullTerminatedString);
}

int codePointCompare(const String& a, const String& b)
{
    return codePointCompare(a.impl(), b.impl());
}

UChar32 String::characterStartingAt(unsigned i) const
{
    if (!m_impl || i >= m_impl->length())
        return 0;
    return m_impl->characterStartingAt(i);
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
    // FIXME: We used to check against a limit of Heap::minExtraCost / sizeof(UChar).

    unsigned stringLength = this->length();
    offset = std::min(offset, stringLength);
    length = std::min(length, stringLength - offset);

    if (!offset && length == stringLength)
        return *this;
    return StringImpl::createSubstringSharingImpl(*m_impl, offset, length);
}

String String::convertToASCIILowercase() const
{
    // FIXME: Should this function, and the many others like it, be inlined?
    return m_impl ? m_impl->convertToASCIILowercase() : String { };
}

String String::convertToASCIIUppercase() const
{
    // FIXME: Should this function, and the many others like it, be inlined?
    return m_impl ? m_impl->convertToASCIIUppercase() : String { };
}

String String::convertToLowercaseWithoutLocale() const
{
    // FIXME: Should this function, and the many others like it, be inlined?
    return m_impl ? m_impl->convertToLowercaseWithoutLocale() : String { };
}

String String::convertToLowercaseWithoutLocaleStartingAtFailingIndex8Bit(unsigned failingIndex) const
{
    // FIXME: Should this function, and the many others like it, be inlined?
    return m_impl ? m_impl->convertToLowercaseWithoutLocaleStartingAtFailingIndex8Bit(failingIndex) : String { };
}

String String::convertToUppercaseWithoutLocale() const
{
    // FIXME: Should this function, and the many others like it, be inlined?
    return m_impl ? m_impl->convertToUppercaseWithoutLocale() : String { };
}

String String::convertToLowercaseWithLocale(const AtomString& localeIdentifier) const
{
    // FIXME: Should this function, and the many others like it, be inlined?
    return m_impl ? m_impl->convertToLowercaseWithLocale(localeIdentifier) : String { };
}

String String::convertToUppercaseWithLocale(const AtomString& localeIdentifier) const
{
    // FIXME: Should this function, and the many others like it, be inlined?
    return m_impl ? m_impl->convertToUppercaseWithLocale(localeIdentifier) : String { };
}

String String::stripWhiteSpace() const
{
    // FIXME: Should this function, and the many others like it, be inlined?
    // FIXME: This function needs a new name. For one thing, "whitespace" is a single
    // word so the "s" should be lowercase. For another, it's not clear from this name
    // that the function uses the Unicode definition of whitespace. Most WebKit callers
    // don't want that and eventually we should consider deleting this.
    return m_impl ? m_impl->stripWhiteSpace() : String { };
}

String String::stripLeadingAndTrailingCharacters(CodeUnitMatchFunction predicate) const
{
    // FIXME: Should this function, and the many others like it, be inlined?
    return m_impl ? m_impl->stripLeadingAndTrailingCharacters(predicate) : String { };
}

String String::simplifyWhiteSpace() const
{
    // FIXME: Should this function, and the many others like it, be inlined?
    // FIXME: This function needs a new name. For one thing, "whitespace" is a single
    // word so the "s" should be lowercase. For another, it's not clear from this name
    // that the function uses the Unicode definition of whitespace. Most WebKit callers
    // don't want that and eventually we should consider deleting this.
    return m_impl ? m_impl->simplifyWhiteSpace() : String { };
}

String String::simplifyWhiteSpace(CodeUnitMatchFunction isWhiteSpace) const
{
    // FIXME: Should this function, and the many others like it, be inlined?
    return m_impl ? m_impl->simplifyWhiteSpace(isWhiteSpace) : String { };
}

String String::foldCase() const
{
    // FIXME: Should this function, and the many others like it, be inlined?
    return m_impl ? m_impl->foldCase() : String { };
}

Vector<UChar> String::charactersWithoutNullTermination() const
{
    Vector<UChar> result;

    if (m_impl) {
        result.reserveInitialCapacity(length() + 1);

        if (is8Bit()) {
            const LChar* characters8 = m_impl->characters8();
            for (unsigned i = 0; i < length(); ++i)
                result.uncheckedAppend(characters8[i]);
        } else {
            const UChar* characters16 = m_impl->characters16();
            result.append(characters16, m_impl->length());
        }
    }

    return result;
}

Vector<UChar> String::charactersWithNullTermination() const
{
    auto result = charactersWithoutNullTermination();
    result.append(0);
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

String String::numberToStringFixedPrecision(float number, unsigned precision, TrailingZerosTruncatingPolicy trailingZerosTruncatingPolicy)
{
    NumberToStringBuffer buffer;
    return String { numberToFixedPrecisionString(number, precision, buffer, trailingZerosTruncatingPolicy == TruncateTrailingZeros) };
}

String String::numberToStringFixedPrecision(double number, unsigned precision, TrailingZerosTruncatingPolicy trailingZerosTruncatingPolicy)
{
    NumberToStringBuffer buffer;
    return String { numberToFixedPrecisionString(number, precision, buffer, trailingZerosTruncatingPolicy == TruncateTrailingZeros) };
}

String String::number(float number)
{
    NumberToStringBuffer buffer;
    return String { numberToString(number, buffer) };
}

String String::number(double number)
{
    NumberToStringBuffer buffer;
    return String { numberToString(number, buffer) };
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
    return m_impl->toDouble(ok);
}

float String::toFloat(bool* ok) const
{
    if (!m_impl) {
        if (ok)
            *ok = false;
        return 0.0f;
    }
    return m_impl->toFloat(ok);
}

String String::isolatedCopy() const &
{
    // FIXME: Should this function, and the many others like it, be inlined?
    return m_impl ? m_impl->isolatedCopy() : String { };
}

String String::isolatedCopy() &&
{
    if (isSafeToSendToAnotherThread()) {
        // Since we know that our string is a temporary that will be destroyed
        // we can just steal the m_impl from it, thus avoiding a copy.
        return { WTFMove(*this) };
    }

    return m_impl ? m_impl->isolatedCopy() : String { };
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
inline void String::splitInternal(UChar separator, const SplitFunctor& functor) const
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
inline Vector<String> String::splitInternal(UChar separator) const
{
    Vector<String> result;
    splitInternal<allowEmptyEntries>(separator, [&result](StringView item) {
        result.append(item.toString());
    });

    return result;
}

void String::split(UChar separator, const SplitFunctor& functor) const
{
    splitInternal<false>(separator, functor);
}

Vector<String> String::split(UChar separator) const
{
    return splitInternal<false>(separator);
}

Vector<String> String::split(StringView separator) const
{
    return splitInternal<false>(separator);
}

void String::splitAllowingEmptyEntries(UChar separator, const SplitFunctor& functor) const
{
    splitInternal<true>(separator, functor);
}

Vector<String> String::splitAllowingEmptyEntries(UChar separator) const
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

    unsigned length = this->length();
    if (!length) { 
        char* characterBuffer;
        return CString::newUninitialized(length, characterBuffer);
    }

    if (this->is8Bit()) {
        const LChar* characters = this->characters8();

        char* characterBuffer;
        CString result = CString::newUninitialized(length, characterBuffer);

        for (unsigned i = 0; i < length; ++i) {
            LChar ch = characters[i];
            characterBuffer[i] = ch && (ch < 0x20 || ch > 0x7f) ? '?' : ch;
        }

        return result;        
    }

    const UChar* characters = this->characters16();

    char* characterBuffer;
    CString result = CString::newUninitialized(length, characterBuffer);

    for (unsigned i = 0; i < length; ++i) {
        UChar ch = characters[i];
        characterBuffer[i] = ch && (ch < 0x20 || ch > 0x7f) ? '?' : ch;
    }

    return result;
}

CString String::latin1() const
{
    // Basic Latin1 (ISO) encoding - Unicode characters 0..255 are
    // preserved, characters outside of this range are converted to '?'.

    unsigned length = this->length();

    if (!length)
        return CString("", 0);

    if (is8Bit())
        return CString(this->characters8(), length);

    const UChar* characters = this->characters16();

    char* characterBuffer;
    CString result = CString::newUninitialized(length, characterBuffer);

    for (unsigned i = 0; i < length; ++i) {
        UChar ch = characters[i];
        characterBuffer[i] = !isLatin1(ch) ? '?' : ch;
    }

    return result;
}

Expected<CString, UTF8ConversionError> String::tryGetUTF8(ConversionMode mode) const
{
    return m_impl ? m_impl->tryGetUTF8(mode) : CString { "", 0 };
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

String String::make8Bit(const UChar* source, unsigned length)
{
    LChar* destination;
    String result = String::createUninitialized(length, destination);
    StringImpl::copyCharacters(destination, source, length);
    return result;
}

void String::convertTo16Bit()
{
    if (isNull() || !is8Bit())
        return;
    auto length = this->length();
    UChar* destination;
    auto convertedString = String::createUninitialized(length, destination);
    StringImpl::copyCharacters(destination, characters8(), length);
    *this = WTFMove(convertedString);
}

template<bool replaceInvalidSequences>
String fromUTF8Impl(const LChar* stringStart, size_t length)
{
    // Do this assertion before chopping the size_t down to unsigned.
    RELEASE_ASSERT(length <= String::MaxLength);

    if (!stringStart)
        return String();

    if (!length)
        return emptyString();

    if (charactersAreAllASCII(stringStart, length))
        return StringImpl::create(stringStart, length);

    Vector<UChar, 1024> buffer(length);
    UChar* bufferStart = buffer.data();
 
    UChar* bufferCurrent = bufferStart;
    const char* stringCurrent = reinterpret_cast<const char*>(stringStart);
    constexpr auto function = replaceInvalidSequences ? convertUTF8ToUTF16ReplacingInvalidSequences : convertUTF8ToUTF16;
    if (!function(stringCurrent, reinterpret_cast<const char*>(stringStart + length), &bufferCurrent, bufferCurrent + buffer.size(), nullptr))
        return String();

    unsigned utf16Length = bufferCurrent - bufferStart;
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(utf16Length <= length);
    return StringImpl::create(bufferStart, utf16Length);
}

String String::fromUTF8(const LChar* stringStart, size_t length)
{
    return fromUTF8Impl<false>(stringStart, length);
}

String String::fromUTF8ReplacingInvalidSequences(const LChar* characters, size_t length)
{
    return fromUTF8Impl<true>(characters, length);
}

String String::fromUTF8(const LChar* string)
{
    if (!string)
        return String();
    return fromUTF8(string, strlen(reinterpret_cast<const char*>(string)));
}

String String::fromUTF8(const CString& s)
{
    return fromUTF8(s.data());
}

String String::fromUTF8WithLatin1Fallback(const LChar* string, size_t size)
{
    String utf8 = fromUTF8(string, size);
    if (!utf8) {
        // Do this assertion before chopping the size_t down to unsigned.
        RELEASE_ASSERT(size <= String::MaxLength);
        return String(string, size);
    }
    return utf8;
}

String String::fromCodePoint(UChar32 codePoint)
{
    UChar buffer[2];
    uint8_t length = 0;
    UBool error = false;
    U16_APPEND(buffer, length, 2, codePoint, error);
    return error ? String() : String(buffer, length);
}

// String Operations

template<typename CharacterType, TrailingJunkPolicy policy>
static inline double toDoubleType(const CharacterType* data, size_t length, bool* ok, size_t& parsedLength)
{
    size_t leadingSpacesLength = 0;
    while (leadingSpacesLength < length && isASCIISpace(data[leadingSpacesLength]))
        ++leadingSpacesLength;

    double number = parseDouble(data + leadingSpacesLength, length - leadingSpacesLength, parsedLength);
    if (!parsedLength) {
        if (ok)
            *ok = false;
        return 0.0;
    }

    parsedLength += leadingSpacesLength;
    if (ok)
        *ok = policy == TrailingJunkPolicy::Allow || parsedLength == length;
    return number;
}

double charactersToDouble(const LChar* data, size_t length, bool* ok)
{
    size_t parsedLength;
    return toDoubleType<LChar, TrailingJunkPolicy::Disallow>(data, length, ok, parsedLength);
}

double charactersToDouble(const UChar* data, size_t length, bool* ok)
{
    size_t parsedLength;
    return toDoubleType<UChar, TrailingJunkPolicy::Disallow>(data, length, ok, parsedLength);
}

float charactersToFloat(const LChar* data, size_t length, bool* ok)
{
    // FIXME: This will return ok even when the string fits into a double but not a float.
    size_t parsedLength;
    return static_cast<float>(toDoubleType<LChar, TrailingJunkPolicy::Disallow>(data, length, ok, parsedLength));
}

float charactersToFloat(const UChar* data, size_t length, bool* ok)
{
    // FIXME: This will return ok even when the string fits into a double but not a float.
    size_t parsedLength;
    return static_cast<float>(toDoubleType<UChar, TrailingJunkPolicy::Disallow>(data, length, ok, parsedLength));
}

float charactersToFloat(const LChar* data, size_t length, size_t& parsedLength)
{
    // FIXME: This will return ok even when the string fits into a double but not a float.
    return static_cast<float>(toDoubleType<LChar, TrailingJunkPolicy::Allow>(data, length, nullptr, parsedLength));
}

float charactersToFloat(const UChar* data, size_t length, size_t& parsedLength)
{
    // FIXME: This will return ok even when the string fits into a double but not a float.
    return static_cast<float>(toDoubleType<UChar, TrailingJunkPolicy::Allow>(data, length, nullptr, parsedLength));
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
    dataLogF("%s\n", asciiDebug(impl()).data());
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
        UChar ch = (*impl)[i];
        if (isASCIIPrintable(ch)) {
            if (ch == '\\')
                buffer.append(ch);
            buffer.append(ch);
        } else {
            buffer.append('\\');
            buffer.append('u');
            buffer.append(hex(ch, 4));
        }
    }
    CString narrowString = buffer.toString().ascii();
    return { reinterpret_cast<const char*>(narrowString.data()), narrowString.length() + 1 };
}

Vector<char> asciiDebug(String& string)
{
    return asciiDebug(string.impl());
}

#endif
