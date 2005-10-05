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
#include <kdom/css/CSSHelper.h>
#include <kdom/DOMString.h>
#include "CounterImpl.h"

using namespace KDOM;

CounterImpl::CounterImpl() : Shared()
{
    m_listStyle = 0;
    m_separator = 0;
    m_identifier = 0;
}

CounterImpl::~CounterImpl()
{
    if(m_separator)
        m_separator->deref();
    if(m_identifier)
        m_identifier->deref();
}

DOMStringImpl *CounterImpl::identifier() const
{
    return m_identifier;
}

void CounterImpl::setIdentifier(DOMStringImpl *value)
{
    KDOM_SAFE_SET(m_identifier, value);
}

unsigned int CounterImpl::listStyleInt() const
{
    return m_listStyle;
}

DOMStringImpl *CounterImpl::listStyle() const
{
    return CSSHelper::stringForListStyleType((EListStyleType) m_listStyle);
}

void CounterImpl::setListStyle(unsigned int value)
{
    m_listStyle = value;
}

DOMStringImpl *CounterImpl::separator() const
{
    return m_separator;
}

void CounterImpl::setSeparator(DOMStringImpl *value)
{
    KDOM_SAFE_SET(m_separator, value);
}

// vim:ts=4:noet
