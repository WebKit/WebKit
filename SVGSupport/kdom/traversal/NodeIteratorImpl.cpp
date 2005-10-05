/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 1999 Lars Knoll (knoll@kde.org)
              (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
              (C) 2001 Peter Kelly (pmk@post.com)
          
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
#include <NodeIteratorImpl.h>
#include <kdom/traversal/kdomtraversal.h>
#include "DOMExceptionImpl.h"
#include "DocumentImpl.h"
#include "NodeImpl.h"
#include "kdom.h"

using namespace KDOM;

NodeIteratorImpl::NodeIteratorImpl(NodeImpl *n, short show,
                                    NodeFilterImpl *filter,
                                   bool expansion)
: TraversalImpl(n, show, filter, expansion), m_referenceNode(0),
m_beforeReferenceNode(true),  m_detached(false), m_doc(0)
{
    if(root())
    {
        setDocument(root()->ownerDocument());
        if(document())
        {
            document()->attachNodeIterator(this);
            document()->ref();
        }
    }
}

NodeIteratorImpl::~NodeIteratorImpl()
{
    if(referenceNode())
        referenceNode()->deref();
    if(document())
    {
        document()->detachNodeIterator(this);
        document()->deref();
    }
}

NodeImpl *NodeIteratorImpl::nextNode()
{
    if(m_detached)
        throw new DOMExceptionImpl(INVALID_STATE_ERR);

    NodeImpl *result = 0;
    NodeImpl *refNode = referenceNode() ? referenceNode() : root();

    if(pointerBeforeReferenceNode() && acceptNode(refNode) == FILTER_ACCEPT)
        result = refNode;
    else
        result = findNextNode(refNode);

    if(result)
        setReferenceNode(result);
    setPointerBeforeReferenceNode(false);

    return result;
}

NodeImpl *NodeIteratorImpl::previousNode()
{
    if(m_detached)
        throw new DOMExceptionImpl(INVALID_STATE_ERR);

    NodeImpl *result = 0;
    NodeImpl *refNode = referenceNode() ? referenceNode() : root();

    if(!pointerBeforeReferenceNode() && acceptNode(refNode) == FILTER_ACCEPT)
        result = refNode;
    else
        result = findPreviousNode(refNode);

    if(result)
        setReferenceNode(result);
    setPointerBeforeReferenceNode();

    return result;
}

void NodeIteratorImpl::detach()
{
    if(!detached() && document())
        document()->detachNodeIterator(this);
    setDetached();
}

void NodeIteratorImpl::setReferenceNode(NodeImpl *node)
{
    if(node == m_referenceNode)
        return;

    NodeImpl *old = m_referenceNode;
    m_referenceNode = node;
    if(m_referenceNode)
        m_referenceNode->ref();
    if(old)
        old->deref();
}

void NodeIteratorImpl::setDocument(DocumentImpl *doc)
{
    if(doc == m_doc)
        return;

    DocumentImpl *old = m_doc;
    m_doc = doc;
    if(m_doc)
        m_doc->ref();
    if(old)
        old->deref();
}

void NodeIteratorImpl::notifyBeforeNodeRemoval(NodeImpl *)
{
}

// vim:ts=4:noet
