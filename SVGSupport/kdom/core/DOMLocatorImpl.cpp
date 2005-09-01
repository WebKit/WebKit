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

#include "NodeImpl.h"
#include "DOMStringImpl.h"
#include "DOMLocatorImpl.h"

using namespace KDOM;

DOMLocatorImpl::DOMLocatorImpl() : Shared()
{
    m_utf16Offset = -1;
    m_relatedNode = 0;
    m_uri = 0;
}

DOMLocatorImpl::~DOMLocatorImpl()
{
    if(m_uri)
        m_uri->deref();
    if(m_relatedNode)
        m_relatedNode->deref();
}

long DOMLocatorImpl::lineNumber() const
{
    return m_lineNumber;
}

void DOMLocatorImpl::setLineNumber(long lineNumber)
{
    m_lineNumber = lineNumber;
}

long DOMLocatorImpl::columnNumber() const
{
    return m_columnNumber;
}

void DOMLocatorImpl::setColumnNumber(long columnNumber)
{
    m_columnNumber = columnNumber;
}

long DOMLocatorImpl::byteOffset() const
{
    return m_byteOffset;
}

void DOMLocatorImpl::setByteOffset(long byteOffset)
{
    m_byteOffset = byteOffset;
}

long DOMLocatorImpl::utf16Offset() const
{
    return m_utf16Offset;
}

void DOMLocatorImpl::setUtf16Offset(long utf16Offset)
{
    m_utf16Offset = utf16Offset;
}

NodeImpl *DOMLocatorImpl::relatedNode() const
{
    return m_relatedNode;
}

void DOMLocatorImpl::setRelatedNode(NodeImpl *relatedNode)
{
    KDOM_SAFE_SET(m_relatedNode, relatedNode);
}

DOMStringImpl *DOMLocatorImpl::uri() const
{
    return m_uri;
}

void DOMLocatorImpl::setUri(DOMStringImpl *uri)
{
    KDOM_SAFE_SET(m_uri, uri);
}

// vim:ts=4:noet
