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

// Construct a string with UTF-16 data, from a null-terminated source.
String::String(const UChar* nullTerminatedString)
{
    if (nullTerminatedString)
        m_impl = StringImpl::create(nullTerminatedString, lengthOfNullTerminatedString(nullTerminatedString));
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
String::String(const LChar* nullTerminatedString)
{
    if (nullTerminatedString)
        m_impl = StringImpl::create(nullTerminatedString);
}

String::String(const char* nullTerminatedString)
{
    if (nullTerminatedString)
        m_impl = StringImpl::create(reinterpret_cast<const LChar*>(nullTerminatedString));
}

String::String(ASCIILiteral characters)
    : m_impl(StringImpl::createFromLiteral(characters))
{
}

void String::append(const String& otherString)
{
    // FIXME: This is extremely inefficient. So much so that we might want to take this out of String's API.

    if (!m_impl) {
        m_impl = otherString.m_impl;
        return;
    }

    if (otherString.isEmpty())
        return;

    auto length = m_impl->length();
    auto otherLength = otherString.m_impl->length();
    if (otherLength > MaxLength - length)
        CRASH();

    if (m_impl->is8Bit() && otherString.m_impl->is8Bit()) {
        LChar* data;
        auto newImpl = StringImpl::createUninitialized(length + otherLength, data);
        StringImpl::copyCharacters(data, m_impl->characters8(), length);
        StringImpl::copyCharacters(data + length, otherString.m_impl->characters8(), otherLength);
        m_impl = WTFMove(newImpl);
        return;
    }
    UChar* data;
    auto newImpl = StringImpl::createUninitialized(length + otherLength, data);
    StringView(*m_impl).getCharactersWithUpconvert(data);
    StringView(*otherString.m_impl).getCharactersWithUpconvert(data + length);
    m_impl = WTFMove(newImpl);
}

void String::append(LChar character)
{
    // FIXME: This is extremely inefficient. So much so that we might want to take this out of String's API.

    if (!m_impl) {
        m_impl = StringImpl::create(&character, 1);
        return;
    }
    if (!is8Bit()) {
        append(static_cast<UChar>(character));
        return;
    }
    if (m_impl->length() >= MaxLength)
        CRASH();
    LChar* data;
    auto newImpl = StringImpl::createUninitialized(m_impl->length() + 1, data);
    StringImpl::copyCharacters(data, m_impl->characters8(), m_impl->length());
    data[m_impl->length()] = character;
    m_impl = WTFMove(newImpl);
}

void String::append(UChar character)
{
    // FIXME: This is extremely inefficient. So much so that we might want to take this out of String's API.

    if (!m_impl) {
        m_impl = StringImpl::create(&character, 1);
        return;
    }
    if (isLatin1(character) && is8Bit()) {
        append(static_cast<LChar>(character));
        return;
    }
    if (m_impl->length() >= MaxLength)
        CRASH();
    UChar* data;
    auto newImpl = StringImpl::createUninitialized(m_impl->length() + 1, data);
    StringView(*m_impl).getCharactersWithUpconvert(data);
    data[m_impl->length()] = character;
    m_impl = WTFMove(newImpl);
}

int codePointCompare(const String& a, const String& b)
{
    return codePointCompare(a.impl(), b.impl());
}

void String::insert(const String& string, unsigned position)
{
    // FIXME: This is extremely inefficient. So much so that we might want to take this out of String's API.

    unsigned lengthToInsert = string.length();

    if (!lengthToInsert) {
        if (string.isNull())
            return;
        if (isNull())
            m_impl = string.impl();
        return;
    }

    if (position >= length()) {
        append(string);
        return;
    }

    if (lengthToInsert > MaxLength - length())
        CRASH();

    if (is8Bit() && string.is8Bit()) {
        LChar* data;
        auto newString = StringImpl::createUninitialized(length() + lengthToInsert, data);
        StringView(*m_impl).substring(0, position).getCharactersWithUpconvert(data);
        StringView(string).getCharactersWithUpconvert(data + position);
        StringView(*m_impl).substring(position).getCharactersWithUpconvert(data + position + lengthToInsert);
        m_impl = WTFMove(newString);
    } else {
        UChar* data;
        auto newString = StringImpl::createUninitialized(length() + lengthToInsert, data);
        StringView(*m_impl).substring(0, position).getCharactersWithUpconvert(data);
        StringView(string).getCharactersWithUpconvert(data + position);
        StringView(*m_impl).substring(position).getCharactersWithUpconvert(data + position + lengthToInsert);
        m_impl = WTFMove(newString);
    }
}

void String::append(const LChar* charactersToAppend, unsigned lengthToAppend)
{
    // FIXME: This is extremely inefficient. So much so that we might want to take this out of String's API.

    if (!m_impl) {
        if (!charactersToAppend)
            return;
        m_impl = StringImpl::create(charactersToAppend, lengthToAppend);
        return;
    }

    if (!lengthToAppend)
        return;

    ASSERT(charactersToAppend);

    unsigned strLength = m_impl->length();

    if (m_impl->is8Bit()) {
        if (lengthToAppend > MaxLength - strLength)
            CRASH();
        LChar* data;
        auto newImpl = StringImpl::createUninitialized(strLength + lengthToAppend, data);
        StringImpl::copyCharacters(data, m_impl->characters8(), strLength);
        StringImpl::copyCharacters(data + strLength, charactersToAppend, lengthToAppend);
        m_impl = WTFMove(newImpl);
        return;
    }

    if (lengthToAppend > MaxLength - strLength)
        CRASH();
    UChar* data;
    auto newImpl = StringImpl::createUninitialized(length() + lengthToAppend, data);
    StringImpl::copyCharacters(data, m_impl->characters16(), strLength);
    StringImpl::copyCharacters(data + strLength, charactersToAppend, lengthToAppend);
    m_impl = WTFMove(newImpl);
}

void String::append(const UChar* charactersToAppend, unsigned lengthToAppend)
{
    // FIXME: This is extremely inefficient. So much so that we might want to take this out of String's API.

    if (!m_impl) {
        if (!charactersToAppend)
            return;
        m_impl = StringImpl::create(charactersToAppend, lengthToAppend);
        return;
    }

    if (!lengthToAppend)
        return;

    unsigned strLength = m_impl->length();
    
    ASSERT(charactersToAppend);
    if (lengthToAppend > MaxLength - strLength)
        CRASH();
    UChar* data;
    auto newImpl = StringImpl::createUninitialized(strLength + lengthToAppend, data);
    if (m_impl->is8Bit())
        StringImpl::copyCharacters(data, characters8(), strLength);
    else
        StringImpl::copyCharacters(data, characters16(), strLength);
    StringImpl::copyCharacters(data + strLength, charactersToAppend, lengthToAppend);
    m_impl = WTFMove(newImpl);
}


UChar32 String::characterStartingAt(unsigned i) const
{
    if (!m_impl || i >= m_impl->length())
        return 0;
    return m_impl->characterStartingAt(i);
}

void String::truncate(unsigned position)
{
    if (m_impl)
        m_impl = m_impl->substring(0, position);
}

template<typename CharacterType> inline void String::removeInternal(const CharacterType* characters, unsigned position, unsigned lengthToRemove)
{
    CharacterType* data;
    auto newImpl = StringImpl::createUninitialized(length() - lengthToRemove, data);
    StringImpl::copyCharacters(data, characters, position);
    StringImpl::copyCharacters(data + position, characters + position + lengthToRemove, length() - lengthToRemove - position);
    m_impl = WTFMove(newImpl);
}

void String::remove(unsigned position, unsigned lengthToRemove)
{
    if (!lengthToRemove)
        return;
    auto length = this->length();
    if (position >= length)
        return;
    lengthToRemove = std::min(lengthToRemove, length - position);
    if (is8Bit())
        removeInternal(characters8(), position, lengthToRemove);
    else
        removeInternal(characters16(), position, lengthToRemove);
}

String String::substring(unsigned position, unsigned length) const
{
    // FIXME: Should this function, and the many others like it, be inlined?
    return m_impl ? m_impl->substring(position, length) : String { };
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

String String::removeCharacters(CodeUnitMatchFunction findMatch) const
{
    // FIXME: Should this function, and the many others like it, be inlined?
    return m_impl ? m_impl->removeCharacters(findMatch) : String { };
}

String String::foldCase() const
{
    // FIXME: Should this function, and the many others like it, be inlined?
    return m_impl ? m_impl->foldCase() : String { };
}

bool String::percentage(int& result) const
{
    if (!m_impl || !m_impl->length())
        return false;

    if ((*m_impl)[m_impl->length() - 1] != '%')
       return false;

    if (m_impl->is8Bit())
        result = charactersToIntStrict(m_impl->characters8(), m_impl->length() - 1);
    else
        result = charactersToIntStrict(m_impl->characters16(), m_impl->length() - 1);
    return true;
}

Vector<UChar> String::charactersWithoutNullTermination() const
{
    Vector<UChar> result;

    if (m_impl) {
        result.reserveInitialCapacity(length() + 1);

        if (is8Bit()) {
            const LChar* characters8 = m_impl->characters8();
            for (size_t i = 0; i < length(); ++i)
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
    return numberToFixedPrecisionString(number, precision, buffer, trailingZerosTruncatingPolicy == TruncateTrailingZeros);
}

String String::numberToStringFixedPrecision(double number, unsigned precision, TrailingZerosTruncatingPolicy trailingZerosTruncatingPolicy)
{
    NumberToStringBuffer buffer;
    return numberToFixedPrecisionString(number, precision, buffer, trailingZerosTruncatingPolicy == TruncateTrailingZeros);
}

String String::number(float number)
{
    NumberToStringBuffer buffer;
    return numberToString(number, buffer);
}

String String::number(double number)
{
    NumberToStringBuffer buffer;
    return numberToString(number, buffer);
}

String String::numberToStringFixedWidth(double number, unsigned decimalPlaces)
{
    NumberToStringBuffer buffer;
    return numberToFixedWidthString(number, decimalPlaces, buffer);
}

int String::toIntStrict(bool* ok, int base) const
{
    if (!m_impl) {
        if (ok)
            *ok = false;
        return 0;
    }
    return m_impl->toIntStrict(ok, base);
}

unsigned String::toUIntStrict(bool* ok, int base) const
{
    if (!m_impl) {
        if (ok)
            *ok = false;
        return 0;
    }
    return m_impl->toUIntStrict(ok, base);
}

int64_t String::toInt64Strict(bool* ok, int base) const
{
    if (!m_impl) {
        if (ok)
            *ok = false;
        return 0;
    }
    return m_impl->toInt64Strict(ok, base);
}

uint64_t String::toUInt64Strict(bool* ok, int base) const
{
    if (!m_impl) {
        if (ok)
            *ok = false;
        return 0;
    }
    return m_impl->toUInt64Strict(ok, base);
}

intptr_t String::toIntPtrStrict(bool* ok, int base) const
{
    if (!m_impl) {
        if (ok)
            *ok = false;
        return 0;
    }
    return m_impl->toIntPtrStrict(ok, base);
}

int String::toInt(bool* ok) const
{
    if (!m_impl) {
        if (ok)
            *ok = false;
        return 0;
    }
    return m_impl->toInt(ok);
}

unsigned String::toUInt(bool* ok) const
{
    if (!m_impl) {
        if (ok)
            *ok = false;
        return 0;
    }
    return m_impl->toUInt(ok);
}

int64_t String::toInt64(bool* ok) const
{
    if (!m_impl) {
        if (ok)
            *ok = false;
        return 0;
    }
    return m_impl->toInt64(ok);
}

uint64_t String::toUInt64(bool* ok) const
{
    if (!m_impl) {
        if (ok)
            *ok = false;
        return 0;
    }
    return m_impl->toUInt64(ok);
}

intptr_t String::toIntPtr(bool* ok) const
{
    if (!m_impl) {
        if (ok)
            *ok = false;
        return 0;
    }
    return m_impl->toIntPtr(ok);
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
inline Vector<String> String::splitInternal(const String& separator) const
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

Vector<String> String::split(const String& separator) const
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

Vector<String> String::splitAllowingEmptyEntries(const String& separator) const
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
        return CString(reinterpret_cast<const char*>(this->characters8()), length);

    const UChar* characters = this->characters16();

    char* characterBuffer;
    CString result = CString::newUninitialized(length, characterBuffer);

    for (unsigned i = 0; i < length; ++i) {
        UChar ch = characters[i];
        characterBuffer[i] = !isLatin1(ch) ? '?' : ch;
    }

    return result;
}

Expected<CString, UTF8ConversionError> String::tryGetUtf8(ConversionMode mode) const
{
    return m_impl ? m_impl->tryGetUtf8(mode) : CString { "", 0 };
}

Expected<CString, UTF8ConversionError> String::tryGetUtf8() const
{
    return tryGetUtf8(LenientConversion);
}

CString String::utf8(ConversionMode mode) const
{
    Expected<CString, UTF8ConversionError> expectedString = tryGetUtf8(mode);
    RELEASE_ASSERT(expectedString);
    return expectedString.value();
}

CString String::utf8() const
{
    return utf8(LenientConversion);
}

String String::make8BitFrom16BitSource(const UChar* source, size_t length)
{
    if (!length)
        return String();

    LChar* destination;
    String result = String::createUninitialized(length, destination);

    copyLCharsFromUCharSource(destination, source, length);

    return result;
}

String String::make16BitFrom8BitSource(const LChar* source, size_t length)
{
    if (!length)
        return String();
    
    UChar* destination;
    String result = String::createUninitialized(length, destination);
    
    StringImpl::copyCharacters(destination, source, length);
    
    return result;
}

String String::fromUTF8(const LChar* stringStart, size_t length)
{
    if (length > MaxLength)
        CRASH();

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
    if (!convertUTF8ToUTF16(stringCurrent, reinterpret_cast<const char *>(stringStart + length), &bufferCurrent, bufferCurrent + buffer.size()))
        return String();

    unsigned utf16Length = bufferCurrent - bufferStart;
    ASSERT_WITH_SECURITY_IMPLICATION(utf16Length < length);
    return StringImpl::create(bufferStart, utf16Length);
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
    if (!utf8)
        return String(string, size);
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
template<typename CharacterType>
static unsigned lengthOfCharactersAsInteger(const CharacterType* data, size_t length)
{
    size_t i = 0;

    // Allow leading spaces.
    for (; i != length; ++i) {
        if (!isSpaceOrNewline(data[i]))
            break;
    }
    
    // Allow sign.
    if (i != length && (data[i] == '+' || data[i] == '-'))
        ++i;
    
    // Allow digits.
    for (; i != length; ++i) {
        if (!isASCIIDigit(data[i]))
            break;
    }

    return i;
}

int charactersToIntStrict(const LChar* data, size_t length, bool* ok, int base)
{
    return toIntegralType<int>(data, length, ok, base);
}

int charactersToIntStrict(const UChar* data, size_t length, bool* ok, int base)
{
    return toIntegralType<int>(data, length, ok, base);
}

unsigned charactersToUIntStrict(const LChar* data, size_t length, bool* ok, int base)
{
    return toIntegralType<unsigned>(data, length, ok, base);
}

unsigned charactersToUIntStrict(const UChar* data, size_t length, bool* ok, int base)
{
    return toIntegralType<unsigned>(data, length, ok, base);
}

int64_t charactersToInt64Strict(const LChar* data, size_t length, bool* ok, int base)
{
    return toIntegralType<int64_t>(data, length, ok, base);
}

int64_t charactersToInt64Strict(const UChar* data, size_t length, bool* ok, int base)
{
    return toIntegralType<int64_t>(data, length, ok, base);
}

uint64_t charactersToUInt64Strict(const LChar* data, size_t length, bool* ok, int base)
{
    return toIntegralType<uint64_t>(data, length, ok, base);
}

uint64_t charactersToUInt64Strict(const UChar* data, size_t length, bool* ok, int base)
{
    return toIntegralType<uint64_t>(data, length, ok, base);
}

intptr_t charactersToIntPtrStrict(const LChar* data, size_t length, bool* ok, int base)
{
    return toIntegralType<intptr_t>(data, length, ok, base);
}

intptr_t charactersToIntPtrStrict(const UChar* data, size_t length, bool* ok, int base)
{
    return toIntegralType<intptr_t>(data, length, ok, base);
}

int charactersToInt(const LChar* data, size_t length, bool* ok)
{
    return toIntegralType<int>(data, lengthOfCharactersAsInteger<LChar>(data, length), ok, 10);
}

int charactersToInt(const UChar* data, size_t length, bool* ok)
{
    return toIntegralType<int>(data, lengthOfCharactersAsInteger(data, length), ok, 10);
}

unsigned charactersToUInt(const LChar* data, size_t length, bool* ok)
{
    return toIntegralType<unsigned>(data, lengthOfCharactersAsInteger<LChar>(data, length), ok, 10);
}

unsigned charactersToUInt(const UChar* data, size_t length, bool* ok)
{
    return toIntegralType<unsigned>(data, lengthOfCharactersAsInteger<UChar>(data, length), ok, 10);
}

int64_t charactersToInt64(const LChar* data, size_t length, bool* ok)
{
    return toIntegralType<int64_t>(data, lengthOfCharactersAsInteger<LChar>(data, length), ok, 10);
}

int64_t charactersToInt64(const UChar* data, size_t length, bool* ok)
{
    return toIntegralType<int64_t>(data, lengthOfCharactersAsInteger<UChar>(data, length), ok, 10);
}

uint64_t charactersToUInt64(const LChar* data, size_t length, bool* ok)
{
    return toIntegralType<uint64_t>(data, lengthOfCharactersAsInteger<LChar>(data, length), ok, 10);
}

uint64_t charactersToUInt64(const UChar* data, size_t length, bool* ok)
{
    return toIntegralType<uint64_t>(data, lengthOfCharactersAsInteger<UChar>(data, length), ok, 10);
}

intptr_t charactersToIntPtr(const LChar* data, size_t length, bool* ok)
{
    return toIntegralType<intptr_t>(data, lengthOfCharactersAsInteger<LChar>(data, length), ok, 10);
}

intptr_t charactersToIntPtr(const UChar* data, size_t length, bool* ok)
{
    return toIntegralType<intptr_t>(data, lengthOfCharactersAsInteger<UChar>(data, length), ok, 10);
}

enum TrailingJunkPolicy { DisallowTrailingJunk, AllowTrailingJunk };

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
        *ok = policy == AllowTrailingJunk || parsedLength == length;
    return number;
}

double charactersToDouble(const LChar* data, size_t length, bool* ok)
{
    size_t parsedLength;
    return toDoubleType<LChar, DisallowTrailingJunk>(data, length, ok, parsedLength);
}

double charactersToDouble(const UChar* data, size_t length, bool* ok)
{
    size_t parsedLength;
    return toDoubleType<UChar, DisallowTrailingJunk>(data, length, ok, parsedLength);
}

float charactersToFloat(const LChar* data, size_t length, bool* ok)
{
    // FIXME: This will return ok even when the string fits into a double but not a float.
    size_t parsedLength;
    return static_cast<float>(toDoubleType<LChar, DisallowTrailingJunk>(data, length, ok, parsedLength));
}

float charactersToFloat(const UChar* data, size_t length, bool* ok)
{
    // FIXME: This will return ok even when the string fits into a double but not a float.
    size_t parsedLength;
    return static_cast<float>(toDoubleType<UChar, DisallowTrailingJunk>(data, length, ok, parsedLength));
}

float charactersToFloat(const LChar* data, size_t length, size_t& parsedLength)
{
    // FIXME: This will return ok even when the string fits into a double but not a float.
    return static_cast<float>(toDoubleType<LChar, AllowTrailingJunk>(data, length, nullptr, parsedLength));
}

float charactersToFloat(const UChar* data, size_t length, size_t& parsedLength)
{
    // FIXME: This will return ok even when the string fits into a double but not a float.
    return static_cast<float>(toDoubleType<UChar, AllowTrailingJunk>(data, length, nullptr, parsedLength));
}

const String& emptyString()
{
    static NeverDestroyed<String> emptyString(StringImpl::empty());
    return emptyString;
}

const String& nullString()
{
    static NeverDestroyed<String> nullString;
    return nullString;
}

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
    return new String(s);
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
    Vector<char> result;
    result.append(reinterpret_cast<const char*>(narrowString.data()), narrowString.length() + 1);
    return result;
}

Vector<char> asciiDebug(String& string)
{
    return asciiDebug(string.impl());
}

#endif
