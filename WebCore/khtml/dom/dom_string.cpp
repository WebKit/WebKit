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

#include "dom/dom_string.h"
#include "xml/dom_stringimpl.h"


namespace DOM {


DOMString::DOMString(const QChar *str, uint len)
{
    if (!str) {
        impl = 0;
        return;
    }
    
    if (len == 0)
        impl = DOMStringImpl::empty();
    else
        impl = new DOMStringImpl(str, len);
    impl->ref();
}

DOMString::DOMString(const QString &str)
{
    if (str.isNull()) {
	impl = 0;
	return;
    }
    
    if (str.isEmpty())
        impl = DOMStringImpl::empty();
    else 
        impl = new DOMStringImpl(str.unicode(), str.length());
    impl->ref();
}

DOMString::DOMString(const char *str)
{
    if (!str) {
	impl = 0;
	return;
    }

    int l = strlen(str);
    if (l == 0)
        impl = DOMStringImpl::empty();
    else
        impl = new DOMStringImpl(str, l);
    impl->ref();
}

DOMString::DOMString(DOMStringImpl *i)
{
    impl = i;
    if(impl) impl->ref();
}

DOMString::DOMString(const DOMString &other)
{
    impl = other.impl;
    if(impl) impl->ref();
}

DOMString &DOMString::operator =(const DOMString &other)
{
    if ( impl != other.impl ) {
    if(impl) impl->deref();
    impl = other.impl;
    if(impl) impl->ref();
    }
    return *this;
}

DOMString &DOMString::operator += (const DOMString &str)
{
    if(str.impl)
    {
        if(!impl)
        {
            // ### FIXME!!!
            impl = str.impl;
            impl->ref();
            return *this;
        }
	DOMStringImpl *i = impl->copy();
	impl->deref();
	impl = i;
	impl->ref();
	impl->append(str.impl);
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
    if(!impl)
    {
	impl = str.impl->copy();
	impl->ref();
    }
    else
	impl->insert(str.impl, pos);
}


const QChar &DOMString::operator [](unsigned int i) const
{
    static const QChar nullChar = 0;

    if(!impl || i >= impl->l ) return nullChar;

    return *(impl->s+i);
}

int DOMString::find(const QChar c, int start) const
{
    unsigned int l = start;
    if(!impl || l >= impl->l ) return -1;
    while( l < impl->l )
    {
	if( *(impl->s+l) == c ) return l;
	l++;
    }
    return -1;
}

uint DOMString::length() const
{
    if(!impl) return 0;
    return impl->l;
}

void DOMString::truncate( unsigned int len )
{
    if(impl) impl->truncate(len);
}

void DOMString::remove(unsigned int pos, int len)
{
  if(impl) impl->remove(pos, len);
}

DOMString DOMString::substring(unsigned int pos, unsigned int len) const
{
    if (!impl) 
        return DOMString();
    return impl->substring(pos, len);
}

DOMString DOMString::split(unsigned int pos)
{
  if(!impl) return DOMString();
  return impl->split(pos);
}

DOMString DOMString::lower() const
{
  if(!impl) return DOMString();
  return impl->lower();
}

DOMString DOMString::upper() const
{
  if(!impl) return DOMString();
  return impl->upper();
}

bool DOMString::percentage(int &_percentage) const
{
    if(!impl || !impl->l) return false;

    if ( *(impl->s+impl->l-1) != QChar('%'))
       return false;

    _percentage = QConstString(impl->s, impl->l-1).qstring().toInt();
    return true;
}

QChar *DOMString::unicode() const
{
    if(!impl) return 0;
    return impl->s;
}

QString DOMString::qstring() const
{
    if(!impl) return QString::null;

    return QString(impl->s, impl->l);
}

int DOMString::toInt() const
{
    if(!impl) return 0;

    return impl->toInt();
}

DOMString DOMString::copy() const
{
    if(!impl) return DOMString();
    return impl->copy();
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
        if ( *a != *b && a->lower() != b->lower() ) return true;
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
    return (!impl || impl->l == 0);
}

khtml::Length* DOMString::toCoordsArray(int& len) const 
{ 
    return impl ? impl->toCoordsArray(len) : 0;
}

khtml::Length* DOMString::toLengthArray(int& len) const 
{ 
    return impl ? impl->toLengthArray(len) : 0;
}

#ifndef NDEBUG
const char *DOMString::ascii() const
{
    return impl ? impl->ascii() : "(null impl)";
}
#endif

//-----------------------------------------------------------------------------

bool operator==( const DOMString &a, const DOMString &b )
{
    if (a.impl == b.impl)
        return true;
    
    unsigned int l = a.length();

    if( l != b.length() ) return false;

    if(!memcmp(a.unicode(), b.unicode(), l*sizeof(QChar)))
	return true;
    return false;
}

bool operator==( const DOMString &a, const QString &b )
{
    unsigned int l = a.length();

    if( l != b.length() ) return false;

    if(!memcmp(a.unicode(), b.unicode(), l*sizeof(QChar)))
	return true;
    return false;
}

bool operator==( const DOMString &a, const char *b )
{
    DOMStringImpl *aimpl = a.impl;
    
    if (!b)
        return !aimpl;
    
    if (aimpl) {
        int alen = aimpl->l;
        const QChar *aptr = aimpl->s;
        while (alen--) {
            unsigned char c = *b++;
            if (!c || (*aptr++).unicode() != c)
                return false;
        }
    }
    return *b == 0;
}

}
