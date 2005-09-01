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

#include "DOMString.h"
#include "LSOutputImpl.h"
#include "DOMStringImpl.h"

using namespace KDOM;

LSOutputImpl::LSOutputImpl() : Shared(), m_systemId(0), m_encoding(0)
{
}

LSOutputImpl::~LSOutputImpl()
{
    if(m_systemId)
        m_systemId->deref();
    if(m_encoding)
        m_encoding->deref();
}

DOMStringImpl *LSOutputImpl::systemId() const
{
    return m_systemId;
}

void LSOutputImpl::setSystemId(DOMStringImpl *systemId)
{
    KDOM_SAFE_SET(m_systemId, systemId);
}

DOMStringImpl *LSOutputImpl::encoding() const
{
    return m_encoding;
}

void LSOutputImpl::setEncoding(DOMStringImpl *encoding)
{
    KDOM_SAFE_SET(m_encoding, encoding);
}

// vim:ts=4:noet
