/*
 * This file is part of the DOM implementation for KDE.
 *
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
 *
 */

#include "dom/dom_string.h"
#include "dom_nameimpl.h"

namespace DOM {

AtomicStringList* AtomicStringList::clone()
{
    return new AtomicStringList(m_string, m_next ? m_next->clone() : 0);
}

bool operator==(const AtomicString &a, const DOMString &b)
{
    unsigned l = a.size();
    if (l != b.length()) return false;
    
    if (!memcmp(a.ustring().data(), b.unicode(), l*sizeof(unsigned short)))
	return true;
    return false;
    
}
   
bool equalsIgnoreCase(const AtomicString &as, const DOMString &bs)
{
    // returns true when equal, false otherwise (ignoring case)
    unsigned l = as.size();
    if (l != bs.length()) return false;
    
    const QChar *a = reinterpret_cast<const QChar*>(as.ustring().data());
    const QChar *b = bs.unicode();
    if (a == b)  return true;
    if (!(a && b))  return false;
    while (l--) {
        if (*a != *b && a->lower() != b->lower()) return false;
	a++,b++;
    }
    return true;
}

bool equalsIgnoreCase(const AtomicString &as, const AtomicString &bs)
{
    // returns true when equal, false otherwise (ignoring case)
    int l = as.size();
    if (l != bs.size()) return false;
    
    const QChar *a = reinterpret_cast<const QChar*>(as.ustring().data());
    const QChar *b = reinterpret_cast<const QChar*>(bs.ustring().data());
    if ( a == b )  return true;
    if ( !( a && b ) )  return false;
    while ( l-- ) {
        if ( *a != *b && a->lower() != b->lower() ) return false;
	a++,b++;
    }
    return true;    
}

}
