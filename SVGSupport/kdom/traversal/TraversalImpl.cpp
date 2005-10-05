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
#include "TraversalImpl.h"
#include <kdom/traversal/kdomtraversal.h>
#include "NodeFilterImpl.h"
#include "NodeImpl.h"
#include "kdom.h"

using namespace KDOM;

TraversalImpl::TraversalImpl(NodeImpl *n, short show,
                             NodeFilterImpl *nodeFilter,
                             bool expandEntityReferences)
: Shared(), m_root(n), m_whatToShow(show),
m_filter(nodeFilter), m_expandEntityReferences(expandEntityReferences)
{
    if(root())
        root()->ref();
    if(filter())
        filter()->ref();
}

TraversalImpl::~TraversalImpl()
{
    if(root())
        root()->deref();
    if(filter())
        filter()->deref();
}

short TraversalImpl::acceptNode(NodeImpl *node) const
{
    // FIXME: If XML is implemented we have to check expandEntityRerefences
    // in this function.  The bit twiddling here is done to map DOM node types,
    // which are given as integers from 1 through 12, to whatToShow bit masks.
    if(node && ((1 << (node->nodeType() - 1)) & m_whatToShow) != 0)
    // cast to short silences "enumeral and non-enumeral types in return" warning
        return m_filter ? m_filter->acceptNode(node) : static_cast<short>(FILTER_ACCEPT);

       return FILTER_SKIP;
}


NodeImpl *TraversalImpl::findParentNode(NodeImpl *node, short accept) const
{
    if(!node || node == root())
        return 0;
    NodeImpl *n = node->parentNode();
    while(n)
    {
        if(acceptNode(n) & accept)
            return n;
        if(n == root())
            return 0;
        n = n->parentNode();
    }

    return 0;
}

NodeImpl *TraversalImpl::findFirstChild(NodeImpl *node) const
{
    if(!node || acceptNode(node) == FILTER_REJECT)
        return 0;
    NodeImpl *n = node->firstChild();
    while(n)
    {
        if(acceptNode(n) == FILTER_ACCEPT)
            return n;
        n = n->nextSibling();
    }

    return 0;
}

NodeImpl *TraversalImpl::findLastChild(NodeImpl *node) const
{
    if(!node || acceptNode(node) == FILTER_REJECT)
    return 0;
    NodeImpl *n = node->lastChild();
    while(n)
    {
        if(acceptNode(n) == FILTER_ACCEPT)
            return n;
        n = n->previousSibling();
    }
    return 0;
}

NodeImpl *TraversalImpl::findPreviousSibling(NodeImpl *node) const
{
    if(!node)
        return 0;
    NodeImpl *n = node->previousSibling();
    while(n)
    {
        if(acceptNode(n) == FILTER_ACCEPT)
            return n;
        n = n->previousSibling();
    }
    return 0;
}

NodeImpl *TraversalImpl::findNextSibling(NodeImpl *node) const
{
    if(!node)
    return 0;
    NodeImpl *n = node->nextSibling();
    while(n)
    {
        if(acceptNode(n) == FILTER_ACCEPT)
            return n;
        n = n->nextSibling();
    }
    return 0;
}

NodeImpl *TraversalImpl::findLastDescendant(NodeImpl *node) const
{
    NodeImpl *n = node;
    NodeImpl *r = node;
    while(n)
    {
        short accepted = acceptNode(n);
        if(accepted != FILTER_REJECT)
        {
            if(accepted == FILTER_ACCEPT)
                r = n;
            if(n->lastChild())
                n = n->lastChild();
            else if(n != node && n->previousSibling())
                n = n->previousSibling();
            else
                break;
        }
        else
            break;
    }

    return r;
}

NodeImpl *TraversalImpl::findPreviousNode(NodeImpl *node) const
{
    NodeImpl *n = node->previousSibling();
    while(n)
    {
        short accepted = acceptNode(n);
        if(accepted != FILTER_REJECT)
        {
            NodeImpl *d = findLastDescendant(n);
            if(acceptNode(d) == FILTER_ACCEPT)
                return d;
            // else FILTER_SKIP
        }
        n = n->previousSibling();
    }

    return findParentNode(node);
}

NodeImpl *TraversalImpl::findNextNode(NodeImpl *node) const
{
    NodeImpl *n = node->firstChild();
    while(n)
    {
        switch(acceptNode(n))
        {
        case FILTER_ACCEPT:
            return n;
        case FILTER_SKIP:
            if(n->firstChild())
                n = n->firstChild();
            else
                n = n->nextSibling();
            break;
        case FILTER_REJECT:
            n = n->nextSibling();
            break;
        }
    }

    n = node->nextSibling();
    while(n)
    {
        switch(acceptNode(n))
        {
        case FILTER_ACCEPT:
            return n;
        case FILTER_SKIP:
            return findNextNode(n);
        case FILTER_REJECT:
            n = n->nextSibling();
        break;
        }
    }

    NodeImpl *parent = findParentNode(node, FILTER_ACCEPT | FILTER_SKIP);
    while(parent)
    {
        n = parent->nextSibling();
        while(n)
        {
            switch(acceptNode(n))
            {
            case FILTER_ACCEPT:
                return n;
            case FILTER_SKIP:
                return findNextNode(n);
            case FILTER_REJECT:
                n = n->nextSibling();
                break;
            }
        }
        parent = findParentNode(parent, FILTER_ACCEPT | FILTER_SKIP);
    }

    return 0;
}

