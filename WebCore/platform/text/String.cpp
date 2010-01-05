/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "PlatformString.h"

#include "CString.h"
#include "FloatConversion.h"
#include "StringBuffer.h"
#include "TextBreakIterator.h"
#include "TextEncoding.h"
#include <wtf/dtoa.h>
#include <limits>
#include <stdarg.h>
#include <wtf/ASCIICType.h>
#include <wtf/StringExtras.h>
#include <wtf/Vector.h>
#include <wtf/unicode/Unicode.h>
#include <wtf/unicode/UTF8.h>

#if USE(JSC)
#include <runtime/Identifier.h>

using JSC::Identifier;
using JSC::UString;
#endif

using namespace WTF;
using namespace WTF::Unicode;

namespace WebCore {

String::String(const UChar* str, unsigned len)
{
    if (!str)
        return;
    m_impl = StringImpl::create(str, len);
}

String::String(const UChar* str)
{
    if (!str)
        return;
        
    int len = 0;
    while (str[len] != UChar(0))
        len++;
    
    m_impl = StringImpl::create(str, len);
}

String::String(const char* str)
{
    if (!str)
        return;
    m_impl = StringImpl::create(str);
}

String::String(const char* str, unsigned length)
{
    if (!str)
        return;
    m_impl = StringImpl::create(str, length);
}

void String::append(const String& str)
{
    if (str.isEmpty())
       return;

    // FIXME: This is extremely inefficient. So much so that we might want to take this
    // out of String's API. We can make it better by optimizing the case where exactly
    // one String is pointing at this StringImpl, but even then it's going to require a
    // call to fastMalloc every single time.
    if (str.m_impl) {
        if (m_impl) {
            UChar* data;
            RefPtr<StringImpl> newImpl =
                StringImpl::createUninitialized(m_impl->length() + str.length(), data);
            memcpy(data, m_impl->characters(), m_impl->length() * sizeof(UChar));
            memcpy(data + m_impl->length(), str.characters(), str.length() * sizeof(UChar));
            m_impl = newImpl.release();
        } else
            m_impl = str.m_impl;
    }
}

void String::append(char c)
{
    // FIXME: This is extremely inefficient. So much so that we might want to take this
    // out of String's API. We can make it better by optimizing the case where exactly
    // one String is pointing at this StringImpl, but even then it's going to require a
    // call to fastMalloc every single time.
    if (m_impl) {
        UChar* data;
        RefPtr<StringImpl> newImpl =
            StringImpl::createUninitialized(m_impl->length() + 1, data);
        memcpy(data, m_impl->characters(), m_impl->length() * sizeof(UChar));
        data[m_impl->length()] = c;
        m_impl = newImpl.release();
    } else
        m_impl = StringImpl::create(&c, 1);
}

void String::append(UChar c)
{
    // FIXME: This is extremely inefficient. So much so that we might want to take this
    // out of String's API. We can make it better by optimizing the case where exactly
    // one String is pointing at this StringImpl, but even then it's going to require a
    // call to fastMalloc every single time.
    if (m_impl) {
        UChar* data;
        RefPtr<StringImpl> newImpl =
            StringImpl::createUninitialized(m_impl->length() + 1, data);
        memcpy(data, m_impl->characters(), m_impl->length() * sizeof(UChar));
        data[m_impl->length()] = c;
        m_impl = newImpl.release();
    } else
        m_impl = StringImpl::create(&c, 1);
}

String operator+(const String& a, const String& b)
{
    if (a.isEmpty())
        return b;
    if (b.isEmpty())
        return a;
    String c = a;
    c += b;
    return c;
}

String operator+(const String& s, const char* cs)
{
    return s + String(cs);
}

String operator+(const char* cs, const String& s)
{
    return String(cs) + s;
}

void String::insert(const String& str, unsigned pos)
{
    if (str.isEmpty()) {
        if (str.isNull())
            return;
        if (isNull())
            m_impl = str.impl();
        return;
    }
    insert(str.characters(), str.length(), pos);
}

void String::append(const UChar* charactersToAppend, unsigned lengthToAppend)
{
    if (!m_impl) {
        if (!charactersToAppend)
            return;
        m_impl = StringImpl::create(charactersToAppend, lengthToAppend);
        return;
    }

    if (!lengthToAppend)
        return;

    ASSERT(charactersToAppend);
    UChar* data;
    RefPtr<StringImpl> newImpl =
        StringImpl::createUninitialized(length() + lengthToAppend, data);
    memcpy(data, characters(), length() * sizeof(UChar));
    memcpy(data + length(), charactersToAppend, lengthToAppend * sizeof(UChar));
    m_impl = newImpl.release();
}

void String::insert(const UChar* charactersToInsert, unsigned lengthToInsert, unsigned position)
{
    if (position >= length()) {
        append(charactersToInsert, lengthToInsert);
        return;
    }

    ASSERT(m_impl);

    if (!lengthToInsert)
        return;

    ASSERT(charactersToInsert);
    UChar* data;
    RefPtr<StringImpl> newImpl =
      StringImpl::createUninitialized(length() + lengthToInsert, data);
    memcpy(data, characters(), position * sizeof(UChar));
    memcpy(data + position, charactersToInsert, lengthToInsert * sizeof(UChar));
    memcpy(data + position + lengthToInsert, characters() + position, (length() - position) * sizeof(UChar));
    m_impl = newImpl.release();
}

UChar String::operator[](unsigned i) const
{
    if (!m_impl || i >= m_impl->length())
        return 0;
    return m_impl->characters()[i];
}

UChar32 String::characterStartingAt(unsigned i) const
{
    if (!m_impl || i >= m_impl->length())
        return 0;
    return m_impl->characterStartingAt(i);
}

unsigned String::length() const
{
    if (!m_impl)
        return 0;
    return m_impl->length();
}

void String::truncate(unsigned position)
{
    if (position >= length())
        return;
    UChar* data;
    RefPtr<StringImpl> newImpl = StringImpl::createUninitialized(position, data);
    memcpy(data, characters(), position * sizeof(UChar));
    m_impl = newImpl.release();
}

void String::remove(unsigned position, int lengthToRemove)
{
    if (lengthToRemove <= 0)
        return;
    if (position >= length())
        return;
    if (static_cast<unsigned>(lengthToRemove) > length() - position)
        lengthToRemove = length() - position;
    UChar* data;
    RefPtr<StringImpl> newImpl =
        StringImpl::createUninitialized(length() - lengthToRemove, data);
    memcpy(data, characters(), position * sizeof(UChar));
    memcpy(data + position, characters() + position + lengthToRemove,
        (length() - lengthToRemove - position) * sizeof(UChar));
    m_impl = newImpl.release();
}

String String::substring(unsigned pos, unsigned len) const
{
    if (!m_impl) 
        return String();
    return m_impl->substring(pos, len);
}

String String::lower() const
{
    if (!m_impl)
        return String();
    return m_impl->lower();
}

String String::upper() const
{
    if (!m_impl)
        return String();
    return m_impl->upper();
}

String String::stripWhiteSpace() const
{
    if (!m_impl)
        return String();
    return m_impl->stripWhiteSpace();
}

String String::simplifyWhiteSpace() const
{
    if (!m_impl)
        return String();
    return m_impl->simplifyWhiteSpace();
}

String String::removeCharacters(CharacterMatchFunctionPtr findMatch) const
{
    if (!m_impl)
        return String();
    return m_impl->removeCharacters(findMatch);
}

String String::foldCase() const
{
    if (!m_impl)
        return String();
    return m_impl->foldCase();
}

bool String::percentage(int& result) const
{
    if (!m_impl || !m_impl->length())
        return false;

    if ((*m_impl)[m_impl->length() - 1] != '%')
       return false;

    result = charactersToIntStrict(m_impl->characters(), m_impl->length() - 1);
    return true;
}

const UChar* String::characters() const
{
    if (!m_impl)
        return 0;
    return m_impl->characters();
}

const UChar* String::charactersWithNullTermination()
{
    if (!m_impl)
        return 0;
    if (m_impl->hasTerminatingNullCharacter())
        return m_impl->characters();
    m_impl = StringImpl::createWithTerminatingNullCharacter(*m_impl);
    return m_impl->characters();
}

String String::format(const char *format, ...)
{
#if PLATFORM(QT)
    // Use QString::vsprintf to avoid the locale dependent formatting of vsnprintf.
    // https://bugs.webkit.org/show_bug.cgi?id=18994
    va_list args;
    va_start(args, format);

    QString buffer;
    buffer.vsprintf(format, args);

    va_end(args);

    return buffer;

#elif OS(WINCE)
    va_list args;
    va_start(args, format);

    Vector<char, 256> buffer;

    int bufferSize = 256;
    buffer.resize(bufferSize);
    for (;;) {
        int written = vsnprintf(buffer.data(), bufferSize, format, args);
        va_end(args);

        if (written == 0)
            return String("");
        if (written > 0)
            return StringImpl::create(buffer.data(), written);
        
        bufferSize <<= 1;
        buffer.resize(bufferSize);
        va_start(args, format);
    }

#else
    va_list args;
    va_start(args, format);

    Vector<char, 256> buffer;

    // Do the format once to get the length.
#if COMPILER(MSVC)
    int result = _vscprintf(format, args);
#else
    char ch;
    int result = vsnprintf(&ch, 1, format, args);
    // We need to call va_end() and then va_start() again here, as the
    // contents of args is undefined after the call to vsnprintf
    // according to http://man.cx/snprintf(3)
    //
    // Not calling va_end/va_start here happens to work on lots of
    // systems, but fails e.g. on 64bit Linux.
    va_end(args);
    va_start(args, format);
#endif

    if (result == 0)
        return String("");
    if (result < 0)
        return String();
    unsigned len = result;
    buffer.grow(len + 1);
    
    // Now do the formatting again, guaranteed to fit.
    vsnprintf(buffer.data(), buffer.size(), format, args);

    va_end(args);
    
    return StringImpl::create(buffer.data(), len);
#endif
}

String String::number(short n)
{
    return String::format("%hd", n);
}

String String::number(unsigned short n)
{
    return String::format("%hu", n);
}

String String::number(int n)
{
    return String::format("%d", n);
}

String String::number(unsigned n)
{
    return String::format("%u", n);
}

String String::number(long n)
{
    return String::format("%ld", n);
}

String String::number(unsigned long n)
{
    return String::format("%lu", n);
}

String String::number(long long n)
{
#if OS(WINDOWS) && !PLATFORM(QT)
    return String::format("%I64i", n);
#else
    return String::format("%lli", n);
#endif
}

String String::number(unsigned long long n)
{
#if OS(WINDOWS) && !PLATFORM(QT)
    return String::format("%I64u", n);
#else
    return String::format("%llu", n);
#endif
}
    
String String::number(double n)
{
    return String::format("%.6lg", n);
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

String String::threadsafeCopy() const
{
    if (!m_impl)
        return String();
    return m_impl->threadsafeCopy();
}

String String::crossThreadString() const
{
    if (!m_impl)
        return String();
    return m_impl->crossThreadString();
}

bool String::isEmpty() const
{
    return !m_impl || !m_impl->length();
}

void String::split(const String& separator, bool allowEmptyEntries, Vector<String>& result) const
{
    result.clear();

    int startPos = 0;
    int endPos;
    while ((endPos = find(separator, startPos)) != -1) {
        if (allowEmptyEntries || startPos != endPos)
            result.append(substring(startPos, endPos - startPos));
        startPos = endPos + separator.length();
    }
    if (allowEmptyEntries || startPos != static_cast<int>(length()))
        result.append(substring(startPos));
}

void String::split(const String& separator, Vector<String>& result) const
{
    return split(separator, false, result);
}

void String::split(UChar separator, bool allowEmptyEntries, Vector<String>& result) const
{
    result.clear();

    int startPos = 0;
    int endPos;
    while ((endPos = find(separator, startPos)) != -1) {
        if (allowEmptyEntries || startPos != endPos)
            result.append(substring(startPos, endPos - startPos));
        startPos = endPos + 1;
    }
    if (allowEmptyEntries || startPos != static_cast<int>(length()))
        result.append(substring(startPos));
}

void String::split(UChar separator, Vector<String>& result) const
{
    return split(String(&separator, 1), false, result);
}

#ifndef NDEBUG
Vector<char> String::ascii() const
{
    if (m_impl) 
        return m_impl->ascii();
    
    const char* nullMsg = "(null impl)";
    Vector<char, 2048> buffer;
    for (int i = 0; nullMsg[i]; ++i)
        buffer.append(nullMsg[i]);
    
    buffer.append('\0');
    return buffer;
}
#endif

CString String::latin1() const
{
    return Latin1Encoding().encode(characters(), length(), QuestionMarksForUnencodables);
}
    
CString String::utf8() const
{
    return UTF8Encoding().encode(characters(), length(), QuestionMarksForUnencodables);
}

String String::fromUTF8(const char* string, size_t size)
{
    if (!string)
        return String();
    return UTF8Encoding().decode(string, size);
}

String String::fromUTF8(const char* string)
{
    if (!string)
        return String();
    return UTF8Encoding().decode(string, strlen(string));
}

String String::fromUTF8WithLatin1Fallback(const char* string, size_t size)
{
    String result = fromUTF8(string, size);
    if (!result)
        result = String(string, size);
    
    return result;
}

#if USE(JSC)
String::String(const Identifier& str)
{
    if (str.isNull())
        return;
    m_impl = StringImpl::create(str.ustring());
}

String::String(const UString& str)
{
    if (str.isNull())
        return;
    m_impl = StringImpl::create(str);
}

String::operator UString() const
{
    if (!m_impl)
        return UString();
    return m_impl->ustring();
}
#endif

// String Operations

static bool isCharacterAllowedInBase(UChar c, int base)
{
    if (c > 0x7F)
        return false;
    if (isASCIIDigit(c))
        return c - '0' < base;
    if (isASCIIAlpha(c)) {
        if (base > 36)
            base = 36;
        return (c >= 'a' && c < 'a' + base - 10)
            || (c >= 'A' && c < 'A' + base - 10);
    }
    return false;
}

template <typename IntegralType>
static inline IntegralType toIntegralType(const UChar* data, size_t length, bool* ok, int base)
{
    static const IntegralType integralMax = std::numeric_limits<IntegralType>::max();
    static const bool isSigned = std::numeric_limits<IntegralType>::is_signed;
    const IntegralType maxMultiplier = integralMax / base;

    IntegralType value = 0;
    bool isOk = false;
    bool isNegative = false;

    if (!data)
        goto bye;

    // skip leading whitespace
    while (length && isSpaceOrNewline(*data)) {
        length--;
        data++;
    }

    if (isSigned && length && *data == '-') {
        length--;
        data++;
        isNegative = true;
    } else if (length && *data == '+') {
        length--;
        data++;
    }

    if (!length || !isCharacterAllowedInBase(*data, base))
        goto bye;

    while (length && isCharacterAllowedInBase(*data, base)) {
        length--;
        IntegralType digitValue;
        UChar c = *data;
        if (isASCIIDigit(c))
            digitValue = c - '0';
        else if (c >= 'a')
            digitValue = c - 'a' + 10;
        else
            digitValue = c - 'A' + 10;

        if (value > maxMultiplier || (value == maxMultiplier && digitValue > (integralMax % base) + isNegative))
            goto bye;

        value = base * value + digitValue;
        data++;
    }

#if COMPILER(MSVC)
#pragma warning(push, 0)
#pragma warning(disable:4146)
#endif

    if (isNegative)
        value = -value;

#if COMPILER(MSVC)
#pragma warning(pop)
#endif

    // skip trailing space
    while (length && isSpaceOrNewline(*data)) {
        length--;
        data++;
    }

    if (!length)
        isOk = true;
bye:
    if (ok)
        *ok = isOk;
    return isOk ? value : 0;
}

static unsigned lengthOfCharactersAsInteger(const UChar* data, size_t length)
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

int charactersToIntStrict(const UChar* data, size_t length, bool* ok, int base)
{
    return toIntegralType<int>(data, length, ok, base);
}

unsigned charactersToUIntStrict(const UChar* data, size_t length, bool* ok, int base)
{
    return toIntegralType<unsigned>(data, length, ok, base);
}

int64_t charactersToInt64Strict(const UChar* data, size_t length, bool* ok, int base)
{
    return toIntegralType<int64_t>(data, length, ok, base);
}

uint64_t charactersToUInt64Strict(const UChar* data, size_t length, bool* ok, int base)
{
    return toIntegralType<uint64_t>(data, length, ok, base);
}

intptr_t charactersToIntPtrStrict(const UChar* data, size_t length, bool* ok, int base)
{
    return toIntegralType<intptr_t>(data, length, ok, base);
}

int charactersToInt(const UChar* data, size_t length, bool* ok)
{
    return toIntegralType<int>(data, lengthOfCharactersAsInteger(data, length), ok, 10);
}

unsigned charactersToUInt(const UChar* data, size_t length, bool* ok)
{
    return toIntegralType<unsigned>(data, lengthOfCharactersAsInteger(data, length), ok, 10);
}

int64_t charactersToInt64(const UChar* data, size_t length, bool* ok)
{
    return toIntegralType<int64_t>(data, lengthOfCharactersAsInteger(data, length), ok, 10);
}

uint64_t charactersToUInt64(const UChar* data, size_t length, bool* ok)
{
    return toIntegralType<uint64_t>(data, lengthOfCharactersAsInteger(data, length), ok, 10);
}

intptr_t charactersToIntPtr(const UChar* data, size_t length, bool* ok)
{
    return toIntegralType<intptr_t>(data, lengthOfCharactersAsInteger(data, length), ok, 10);
}

double charactersToDouble(const UChar* data, size_t length, bool* ok)
{
    if (!length) {
        if (ok)
            *ok = false;
        return 0.0;
    }

    Vector<char, 256> bytes(length + 1);
    for (unsigned i = 0; i < length; ++i)
        bytes[i] = data[i] < 0x7F ? data[i] : '?';
    bytes[length] = '\0';
    char* end;
    double val = WTF::strtod(bytes.data(), &end);
    if (ok)
        *ok = (end == 0 || *end == '\0');
    return val;
}

float charactersToFloat(const UChar* data, size_t length, bool* ok)
{
    // FIXME: This will return ok even when the string fits into a double but not a float.
    return narrowPrecisionToFloat(charactersToDouble(data, length, ok));
}

PassRefPtr<SharedBuffer> utf8Buffer(const String& string)
{
    // Allocate a buffer big enough to hold all the characters.
    const int length = string.length();
    Vector<char> buffer(length * 3);

    // Convert to runs of 8-bit characters.
    char* p = buffer.data();
    const UChar* d = string.characters();
    ConversionResult result = convertUTF16ToUTF8(&d, d + length, &p, p + buffer.size(), true);
    if (result != conversionOK)
        return 0;

    buffer.shrink(p - buffer.data());
    return SharedBuffer::adoptVector(buffer);
}

unsigned String::numGraphemeClusters() const
{
    TextBreakIterator* it = characterBreakIterator(characters(), length());
    if (!it)
        return length();

    unsigned num = 0;
    while (textBreakNext(it) != TextBreakDone)
        ++num;
    return num;
}

unsigned String::numCharactersInGraphemeClusters(unsigned numGraphemeClusters) const
{
    TextBreakIterator* it = characterBreakIterator(characters(), length());
    if (!it)
        return min(length(), numGraphemeClusters);

    for (unsigned i = 0; i < numGraphemeClusters; ++i) {
        if (textBreakNext(it) == TextBreakDone)
            return length();
    }
    return textBreakCurrent(it);
}

} // namespace WebCore

#ifndef NDEBUG
// For use in the debugger - leaks memory
WebCore::String* string(const char*);

WebCore::String* string(const char* s)
{
    return new WebCore::String(s);
}
#endif
