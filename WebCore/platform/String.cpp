/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "PlatformString.h"

#include <kjs/identifier.h>
#include <kxmlcore/Vector.h>
#include <stdarg.h>

using namespace KJS;

namespace WebCore {

String::String(const QChar* str, unsigned len)
{
    if (!str)
        return;
    
    if (len == 0)
        m_impl = StringImpl::empty();
    else
        m_impl = new StringImpl(str, len);
}

String::String(const DeprecatedString& str)
{
    if (str.isNull())
        return;
    
    if (str.isEmpty())
        m_impl = StringImpl::empty();
    else 
        m_impl = new StringImpl(str.unicode(), str.length());
}

String::String(const char* str)
{
    if (!str)
        return;

    int l = strlen(str);
    if (l == 0)
        m_impl = StringImpl::empty();
    else
        m_impl = new StringImpl(str, l);
}

String::String(const char* str, unsigned length)
{
    if (!str)
        return;

    if (length == 0)
        m_impl = StringImpl::empty();
    else
        m_impl = new StringImpl(str, length);
}

String& String::operator+=(const String &str)
{
    if (str.m_impl) {
        if (!m_impl) {
            // ### FIXME!!!
            m_impl = str.m_impl;
            return *this;
        }
        m_impl = m_impl->copy();
        m_impl->append(str.m_impl.get());
    }
    return *this;
}

String operator+(const String& a, const String& b)
{
    if (a.isEmpty())
        return b.copy();
    if (b.isEmpty())
        return a.copy();
    String c = a.copy();
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

String operator+(const String& s, QChar c)
{
    return s + String(c);
}

String operator+(QChar c, const String& s)
{
    return String(c) + s;
}

String operator+(const String& s, char c)
{
    return s + String(c);
}

String operator+(char c, const String& s)
{
    return String(c) + s;
}

void String::insert(const String& str, unsigned pos)
{
    if (!m_impl)
        m_impl = str.m_impl->copy();
    else
        m_impl->insert(str.m_impl.get(), pos);
}

const QChar& String::operator[](unsigned i) const
{
    static const QChar nullChar = 0;
    if (!m_impl || i >= m_impl->length() )
        return nullChar;
    return *(m_impl->unicode()+i);
}

unsigned String::length() const
{
    if (!m_impl)
        return 0;
    return m_impl->length();
}

void String::truncate(unsigned len)
{
    if (m_impl)
        m_impl->truncate(len);
}

void String::remove(unsigned pos, int len)
{
    if (m_impl)
        m_impl->remove(pos, len);
}

String String::substring(unsigned pos, unsigned len) const
{
    if (!m_impl) 
        return String();
    return m_impl->substring(pos, len);
}

String String::split(unsigned pos)
{
    if (!m_impl)
        return String();
    return m_impl->split(pos);
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

bool String::percentage(int& result) const
{
    if (!m_impl || !m_impl->length())
        return false;

    if (*(m_impl->unicode()+m_impl->length()-1) != '%')
       return false;

    result = QConstString(m_impl->unicode(), m_impl->length()-1).string().toInt();
    return true;
}

const QChar* String::unicode() const
{
    if (!m_impl)
        return 0;
    return m_impl->unicode();
}

DeprecatedString String::deprecatedString() const
{
    if (!m_impl)
        return DeprecatedString::null;
    if (!m_impl->unicode())
        return DeprecatedString("", 0);
    return DeprecatedString(m_impl->unicode(), m_impl->length());
}

String String::sprintf(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    Vector<char, 256> buffer;
#if PLATFORM(WIN_OS)
    // Windows vnsprintf does not return the expected size on overflow
    // So we just have to keep looping until our vsprintf call works!
    int result = 0;
    do {
        if (result < 0)
            buffer.resize(buffer.capacity() * 2);
        result = vsnprintf(buffer.data(), buffer.capacity(), format, args);
        // Windows vnsprintf returns -1 both for all errors.  There is no
        // way to distinguish between "not enough room" and "invalid format"
        // we just keep trying until we hit an arbitrary size and then stop.
    } while (result < 0 && (buffer.capacity() * 2) < 2048);
    if (result == 0)
        return String("");
    else if (result < 0)
        return String();
    unsigned len = result;
#else
    // Do the format once to get the length.
    char ch;
    int result = vsnprintf(&ch, 1, format, args);
    if (result == 0)
        return String("");
    else if (result < 0)
        return String();
    unsigned len = result;
    buffer.resize(len + 1);
    
    // Now do the formatting again, guaranteed to fit.
    vsnprintf(buffer.data(), buffer.size(), format, args);
#endif
    va_end(args);
    
    return new StringImpl(buffer.data(), len);
}

String String::number(int n)
{
    return String::sprintf("%d", n);
}

String String::number(unsigned n)
{
    return String::sprintf("%u", n);
}

String String::number(long n)
{
    return String::sprintf("%ld", n);
}

String String::number(unsigned long n)
{
    return String::sprintf("%lu", n);
}

String String::number(double n)
{
    return String::sprintf("%.6lg", n);
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

String String::copy() const
{
    if (!m_impl)
        return String();
    return m_impl->copy();
}

bool String::isEmpty() const
{
    return (!m_impl || m_impl->length() == 0);
}

Length* String::toCoordsArray(int& len) const 
{
    return m_impl ? m_impl->toCoordsArray(len) : 0;
}

Length* String::toLengthArray(int& len) const 
{
    return m_impl ? m_impl->toLengthArray(len) : 0;
}

#ifndef NDEBUG
const char *String::ascii() const
{
    return m_impl ? m_impl->ascii() : "(null impl)";
}
#endif

bool operator==(const String& a, const DeprecatedString& b)
{
    unsigned l = a.length();
    if (l != b.length())
        return false;
    if (!memcmp(a.unicode(), b.unicode(), l * sizeof(QChar)))
        return true;
    return false;
}

String::String(const Identifier& str)
{
    if (str.isNull())
        return;
    
    if (str.isEmpty())
        m_impl = StringImpl::empty();
    else 
        m_impl = new StringImpl(reinterpret_cast<const QChar*>(str.data()), str.size());
}

String::String(const UString& str)
{
    if (str.isNull())
        return;
    
    if (str.isEmpty())
        m_impl = StringImpl::empty();
    else 
        m_impl = new StringImpl(reinterpret_cast<const QChar*>(str.data()), str.size());
}

String::operator Identifier() const
{
    if (!m_impl)
        return Identifier();
    return Identifier(reinterpret_cast<const KJS::UChar*>(m_impl->unicode()), m_impl->length());
}

String::operator UString() const
{
    if (!m_impl)
        return UString();
    return UString(reinterpret_cast<const KJS::UChar*>(m_impl->unicode()), m_impl->length());
}

}
