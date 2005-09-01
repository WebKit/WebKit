/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 1999 Lars Knoll (knoll@kde.org)
              (C) 1999 Antti Koivisto (koivisto@kde.org)
              (C) 2001 Dirk Mueller (mueller@kde.org)
              (C) 2003 Apple Computer, Inc.

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
#include "NodeListImpl.h"

using namespace KDOM;

NodeListImpl::NodeListImpl(NodeImpl *refNode) : Shared(), m_refNode(refNode)
{
    if(m_refNode)
        m_refNode->ref();
}

NodeListImpl::~NodeListImpl()
{
    if(m_refNode)
        m_refNode->deref();
}

NodeImpl *NodeListImpl::item(unsigned long index) const
{
    if(!m_refNode)
        return 0;

    unsigned int pos = 0;
    NodeImpl *n = m_refNode->firstChild();
    while(n != 0 && pos < index)
    {
        n = n->nextSibling();
        pos++;
    }

    return n;
}

unsigned long NodeListImpl::length() const
{
    if(!m_refNode)
        return 0;
        
    unsigned long len = 0;
    for(NodeImpl *n = m_refNode->firstChild(); n != 0; n = n->nextSibling())
        len++;

    return len;
}

int NodeListImpl::index(NodeImpl *_item) const
{
    if(m_refNode)
    {
        unsigned int pos = 0;
        NodeImpl *n = m_refNode->firstChild();
        while(n)
        {
            if(n == _item)
                return pos;

            n = n->nextSibling();
            pos++;
        }
    }

    return -1;
}

// vim:ts=4:noet
