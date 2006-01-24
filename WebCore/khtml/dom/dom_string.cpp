/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004 Apple Computer, Inc.
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
#include "dom_string.h"
#include "xml/dom_stringimpl.h"


namespace DOM {

DOMString::DOMString(const QChar *str, uint len)
{
    if (!str)
        return;
    
    if (len == 0)
        m_impl = DOMStringImpl::empty();
    else
        m_impl = new DOMStringImpl(str, len);
}

DOMString::DOMString(const QString &str)
{
    if (str.isNull())
        return;
    
    if (str.isEmpty())
        m_impl = DOMStringImpl::empty();
    else 
        m_impl = new DOMStringImpl(str.unicode(), str.length());
}

DOMString::DOMString(const char *str)
{
    if (!str)
        return;

    int l = strlen(str);
    if (l == 0)
        m_impl = DOMStringImpl::empty();
    else
        m_impl = new DOMStringImpl(str, l);
}

DOMString &DOMString::operator += (const DOMString &str)
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

DOMString operator + (const DOMString &a, const DOMString &b)
{
    if (a.isEmpty())
        return b.copy();
    if (b.isEmpty())
        return a.copy();
    DOMString c = a.copy();
    c += b;
    return c;
}

void DOMString::insert(DOMString str, uint pos)
{
    if (!m_impl)
        m_impl = str.m_impl->copy();
    else
        m_impl->insert(str.m_impl.get(), pos);
}


const QChar &DOMString::operator [](unsigned int i) const
{
    static const QChar nullChar = 0;

    if (!m_impl || i >= m_impl->l )
        return nullChar;

    return *(m_impl->s+i);
}

uint DOMString::length() const
{
    if (!m_impl)
        return 0;
    return m_impl->l;
}

void DOMString::truncate( unsigned int len )
{
    if (m_impl)
        m_impl->truncate(len);
}

void DOMString::remove(unsigned int pos, int len)
{
  if (m_impl)
    m_impl->remove(pos, len);
}

DOMString DOMString::substring(unsigned int pos, unsigned int len) const
{
    if (!m_impl) 
        return DOMString();
    return m_impl->substring(pos, len);
}

DOMString DOMString::split(unsigned int pos)
{
    if (!m_impl)
        return DOMString();
    return m_impl->split(pos);
}

DOMString DOMString::lower() const
{
    if (!m_impl)
        return DOMString();
    return m_impl->lower();
}

DOMString DOMString::upper() const
{
    if (!m_impl)
        return DOMString();
    return m_impl->upper();
}

bool DOMString::percentage(int &_percentage) const
{
    if (!m_impl || !m_impl->l)
        return false;

    if ( *(m_impl->s+m_impl->l-1) != QChar('%'))
       return false;

    _percentage = QConstString(m_impl->s, m_impl->l-1).string().toInt();
    return true;
}

QChar *DOMString::unicode() const
{
    if (!m_impl)
        return 0;
    return m_impl->s;
}

QString DOMString::qstring() const
{
    if (!m_impl)
        return QString::null;
    if (!m_impl->s)
        return QString("", 0);

    return QString(m_impl->s, m_impl->l);
}

int DOMString::toInt() const
{
    if (!m_impl)
        return 0;

    return m_impl->toInt();
}

DOMString DOMString::copy() const
{
    if (!m_impl)
        return DOMString();
    return m_impl->copy();
}

// ------------------------------------------------------------------------

bool strcasecmp( const DOMString &as, const DOMString &bs )
{
    if ( as.length() != bs.length() ) return true;

    const QChar *a = as.unicode();
    const QChar *b = bs.unicode();
    if ( a == b )  return false;
    if ( !( a && b ) )  return true;
    int l = as.length();
    while ( l-- ) {
        if ( *a != *b && a->lower() != b->lower() )
            return true;
        a++,b++;
    }
    return false;
}

bool strcasecmp( const DOMString &as, const char* bs )
{
    const QChar *a = as.unicode();
    int l = as.length();
    if ( !bs ) return ( l != 0 );
    while ( l-- ) {
        if ( a->latin1() != *bs ) {
            char cc = ( ( *bs >= 'A' ) && ( *bs <= 'Z' ) ) ? ( ( *bs ) + 'a' - 'A' ) : ( *bs );
            if ( a->lower().latin1() != cc ) return true;
        }
        a++, bs++;
    }
    return ( *bs != '\0' );
}

bool DOMString::isEmpty() const
{
    return (!m_impl || m_impl->l == 0);
}

khtml::Length* DOMString::toCoordsArray(int& len) const 
{
    return m_impl ? m_impl->toCoordsArray(len) : 0;
}

khtml::Length* DOMString::toLengthArray(int& len) const 
{
    return m_impl ? m_impl->toLengthArray(len) : 0;
}

#ifndef NDEBUG
const char *DOMString::ascii() const
{
    return m_impl ? m_impl->ascii() : "(null impl)";
}
#endif

//-----------------------------------------------------------------------------

bool operator==( const DOMString &a, const QString &b )
{
    unsigned int l = a.length();

    if ( l != b.length() ) return false;

    if (!memcmp(a.unicode(), b.unicode(), l*sizeof(QChar)))
        return true;
    return false;
}

}
