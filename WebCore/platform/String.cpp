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
    if (!m_impl || i >= m_impl->l )
        return nullChar;
    return *(m_impl->s+i);
}

unsigned String::length() const
{
    if (!m_impl)
        return 0;
    return m_impl->l;
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
    if (!m_impl || !m_impl->l)
        return false;

    if (*(m_impl->s+m_impl->l-1) != '%')
       return false;

    result = QConstString(m_impl->s, m_impl->l-1).string().toInt();
    return true;
}

QChar* String::unicode() const
{
    if (!m_impl)
        return 0;
    return m_impl->s;
}

DeprecatedString String::deprecatedString() const
{
    if (!m_impl)
        return DeprecatedString::null;
    if (!m_impl->s)
        return DeprecatedString("", 0);
    return DeprecatedString(m_impl->s, m_impl->l);
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
    return (!m_impl || m_impl->l == 0);
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
