/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#include "CSSValueListImpl.h"

using namespace KDOM;

CSSValueListImpl::CSSValueListImpl() : CSSValueImpl()
{
}

CSSValueListImpl::~CSSValueListImpl()
{
    CSSValueImpl *val = m_values.first();
    while(val)
    {
        val->deref();
        val = m_values.next();
    }
}

unsigned long CSSValueListImpl::length() const
{
    return m_values.count();
}

CSSValueImpl *CSSValueListImpl::item(unsigned long index)
{
    return m_values.at(index);
}

DOMStringImpl *CSSValueListImpl::cssText() const
{
    DOMStringImpl *result = new DOMStringImpl();

    for(Q3PtrListIterator<CSSValueImpl> iterator(m_values); iterator.current(); ++iterator)
        result->append(iterator.current()->cssText());

    return result;
}

void CSSValueListImpl::setCssText(DOMStringImpl *)
{
}

unsigned short CSSValueListImpl::cssValueType() const
{
    return CSS_VALUE_LIST;
}

void CSSValueListImpl::append(CSSValueImpl *val)
{
    m_values.append(val);
    val->ref();
}

// vim:ts=4:noet
