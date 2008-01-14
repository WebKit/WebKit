/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "DeprecatedString.h"
#include "StringBuffer.h"
#include "TextEncoding.h"
#include <kjs/identifier.h>
#include <wtf/StringExtras.h>
#include <wtf/Vector.h>
#include <stdarg.h>

using KJS::Identifier;
using KJS::UString;

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

String::String(const DeprecatedString& str)
{
    if (str.isNull())
        return;
    m_impl = StringImpl::create(reinterpret_cast<const UChar*>(str.unicode()), str.length());
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
    // FIXME: This is extremely inefficient. So much so that we might want to take this
    // out of String's API. We can make it better by optimizing the case where exactly
    // one String is pointing at this StringImpl, but even then it's going to require a
    // call to fastMalloc every single time.
    if (str.m_impl) {
        if (m_impl) {
            StringBuffer buffer(m_impl->length() + str.length());
            memcpy(buffer.characters(), m_impl->characters(), m_impl->length() * sizeof(UChar));
            memcpy(buffer.characters() + m_impl->length(), str.characters(), str.length() * sizeof(UChar));
            m_impl = StringImpl::adopt(buffer);
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
        StringBuffer buffer(m_impl->length() + 1);
        memcpy(buffer.characters(), m_impl->characters(), m_impl->length() * sizeof(UChar));
        buffer[m_impl->length()] = c;
        m_impl = StringImpl::adopt(buffer);
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
        StringBuffer buffer(m_impl->length() + 1);
        memcpy(buffer.characters(), m_impl->characters(), m_impl->length() * sizeof(UChar));
        buffer[m_impl->length()] = c;
        m_impl = StringImpl::adopt(buffer);
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
    StringBuffer buffer(length() + lengthToAppend);
    memcpy(buffer.characters(), characters(), length() * sizeof(UChar));
    memcpy(buffer.characters() + length(), charactersToAppend, lengthToAppend * sizeof(UChar));
    m_impl = StringImpl::adopt(buffer);
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
    StringBuffer buffer(length() + lengthToInsert);
    memcpy(buffer.characters(), characters(), position * sizeof(UChar));
    memcpy(buffer.characters() + position, charactersToInsert, lengthToInsert * sizeof(UChar));
    memcpy(buffer.characters() + position + lengthToInsert, characters() + position, (length() - position) * sizeof(UChar));
    m_impl = StringImpl::adopt(buffer);
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
    StringBuffer buffer(position);
    memcpy(buffer.characters(), characters(), position * sizeof(UChar));
    m_impl = StringImpl::adopt(buffer);
}

void String::remove(unsigned position, int lengthToRemove)
{
    if (lengthToRemove <= 0)
        return;
    if (position >= length())
        return;
    if (static_cast<unsigned>(lengthToRemove) > length() - position)
        lengthToRemove = length() - position;
    StringBuffer buffer(length() - lengthToRemove);
    memcpy(buffer.characters(), characters(), position * sizeof(UChar));
    memcpy(buffer.characters() + position, characters() + position + lengthToRemove,
        (length() - lengthToRemove - position) * sizeof(UChar));
    m_impl = StringImpl::adopt(buffer);
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

    result = DeprecatedConstString(reinterpret_cast<const DeprecatedChar*>(m_impl->characters()), m_impl->length() - 1).string().toInt();
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

DeprecatedString String::deprecatedString() const
{
    if (!m_impl)
        return DeprecatedString::null;
    if (!m_impl->characters())
        return DeprecatedString("", 0);
    return DeprecatedString(reinterpret_cast<const DeprecatedChar*>(m_impl->characters()), m_impl->length());
}

String String::format(const char *format, ...)
{
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
#if PLATFORM(WIN_OS)
    return String::format("%I64i", n);
#else
    return String::format("%lli", n);
#endif
}

String String::number(unsigned long long n)
{
#if PLATFORM(WIN_OS)
    return String::format("%I64u", n);
#else
    return String::format("%llu", n);
#endif
}
    
String String::number(double n)
{
    return String::format("%.6lg", n);
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

double String::toDouble(bool* ok) const
{
    if (!m_impl) {
        if (ok)
            *ok = false;
        return 0;
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

String String::copy() const
{
    if (!m_impl)
        return String();
    return m_impl->copy();
}

bool String::isEmpty() const
{
    return !m_impl || !m_impl->length();
}

Length* String::toCoordsArray(int& len) const 
{
    return m_impl ? m_impl->toCoordsArray(len) : 0;
}

Length* String::toLengthArray(int& len) const 
{
    return m_impl ? m_impl->toLengthArray(len) : 0;
}

Vector<String> String::split(const String& separator, bool allowEmptyEntries) const
{
    Vector<String> result;
    
    int startPos = 0;
    int endPos;
    while ((endPos = find(separator, startPos)) != -1) {
        if (allowEmptyEntries || startPos != endPos)
            result.append(substring(startPos, endPos - startPos));
        startPos = endPos + separator.length();
    }
    if (allowEmptyEntries || startPos != (int)length())
        result.append(substring(startPos));
    
    return result;
}

Vector<String> String::split(UChar separator, bool allowEmptyEntries) const
{
    Vector<String> result;
  
    return split(String(&separator, 1), allowEmptyEntries);
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
    return Latin1Encoding().encode(characters(), length());
}
    
CString String::utf8() const
{
    return UTF8Encoding().encode(characters(), length());
}

String String::fromUTF8(const char* string, size_t size)
{
    return UTF8Encoding().decode(string, size);
}

String String::fromUTF8(const char* string)
{
    return UTF8Encoding().decode(string, strlen(string));
}


bool operator==(const String& a, const DeprecatedString& b)
{
    unsigned l = a.length();
    if (l != b.length())
        return false;
    if (!memcmp(a.characters(), b.unicode(), l * sizeof(UChar)))
        return true;
    return false;
}

String::String(const Identifier& str)
{
    if (str.isNull())
        return;
    m_impl = StringImpl::create(reinterpret_cast<const UChar*>(str.data()), str.size());
}

String::String(const UString& str)
{
    if (str.isNull())
        return;
    m_impl = StringImpl::create(reinterpret_cast<const UChar*>(str.data()), str.size());
}

String::operator Identifier() const
{
    if (!m_impl)
        return Identifier();
    return Identifier(reinterpret_cast<const KJS::UChar*>(m_impl->characters()), m_impl->length());
}

String::operator UString() const
{
    if (!m_impl)
        return UString();
    return UString(reinterpret_cast<const KJS::UChar*>(m_impl->characters()), m_impl->length());
}

} // namespace WebCore

#ifndef NDEBUG
// For debugging only -- leaks memory
WebCore::String* string(const char* s)
{
    return new WebCore::String(s);
}
#endif
