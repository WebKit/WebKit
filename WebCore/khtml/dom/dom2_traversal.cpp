/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
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

#include "config.h"
#include "dom/dom2_traversal.h"

#if !KHTML_NO_CPLUSPLUS_DOM

#include "dom/dom_exception.h"
#include "dom/dom_string.h"
#include "xml/dom2_traversalimpl.h"

#endif

namespace DOM {

// --------------------------------------------------------------

short NodeFilterCondition::acceptNode(FilterNode) const
{
    return NodeFilter::FILTER_ACCEPT;
}

// --------------------------------------------------------------

#if !KHTML_NO_CPLUSPLUS_DOM

NodeFilter::NodeFilter() : impl(0)
{
}

NodeFilter::NodeFilter(NodeFilterCondition *condition)
{
    impl = new NodeFilterImpl(condition);
    impl->ref();
}

NodeFilter::NodeFilter(const NodeFilter &other)
{
    impl = other.impl;
    if (impl) 
        impl->ref();
}

NodeFilter::NodeFilter(NodeFilterImpl *i)
{
    impl = i;
    if (impl)
        impl->ref();
}

NodeFilter &NodeFilter::operator=(const NodeFilter &other)
{
    if (impl == other.impl)
        return *this;
    
    NodeFilterImpl *old = impl;
    impl = other.impl;
    if (impl) 
        impl->ref();
    if (old) 
        old->deref();

    return *this;
}

NodeFilter::~NodeFilter()
{
    if (impl) 
        impl->deref();
}

short NodeFilter::acceptNode(const Node &node) const
{
    if (impl)
        return impl->acceptNode(node.handle());
    return FILTER_ACCEPT;
}

// --------------------------------------------------------------

NodeIterator::NodeIterator()
{
    impl = 0;
}

NodeIterator::NodeIterator(const NodeIterator &other)
{
    impl = other.impl;
    if (impl) 
        impl->ref();
}

NodeIterator::NodeIterator(NodeIteratorImpl *i)
{
    impl = i;
    if (impl) 
        impl->ref();
}

NodeIterator &NodeIterator::operator=(const NodeIterator &other)
{
    if (impl == other.impl)
        return *this;
    
    NodeIteratorImpl *old = impl;
    impl = other.impl;
    if (impl) 
        impl->ref();
    if (old) 
        old->deref();

    return *this;
}

NodeIterator::~NodeIterator()
{
    if (impl) 
        impl->deref();
}

Node NodeIterator::root() const
{
    if (impl) 
        return impl->root();
    return 0;
}

unsigned NodeIterator::whatToShow() const
{
    if (impl) 
        return impl->whatToShow();
    return 0;
}

NodeFilter NodeIterator::filter() const
{
    if (impl) 
        return impl->filter();
    return NodeFilter();
}

bool NodeIterator::expandEntityReferences() const
{
    if (impl) 
        return impl->expandEntityReferences();
    return 0;
}

Node NodeIterator::nextNode()
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    NodeImpl *result = impl->nextNode(exceptioncode);
    if (exceptioncode)
        throw DOMException(exceptioncode);
    return result;
}

Node NodeIterator::previousNode()
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    NodeImpl *result = impl->previousNode(exceptioncode);
    if (exceptioncode)
        throw DOMException(exceptioncode);
    return result;
}

void NodeIterator::detach()
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);

    int exceptioncode = 0;
    impl->detach(exceptioncode);
    if (exceptioncode)
        throw DOMException(exceptioncode);
}

Node NodeIterator::referenceNode() const
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);
        
    return impl->referenceNode();
}

bool NodeIterator::pointerBeforeReferenceNode() const
{
    if (!impl)
        throw DOMException(DOMException::INVALID_STATE_ERR);
        
    return impl->pointerBeforeReferenceNode();
}

// --------------------------------------------------------------

TreeWalker::TreeWalker()
{
    impl = 0;
}

TreeWalker::TreeWalker(const TreeWalker &other)
{
    impl = other.impl;
    if (impl) 
        impl->ref();
}

TreeWalker::TreeWalker(TreeWalkerImpl *i)
{
    impl = i;
    if (impl) 
        impl->ref();
}

TreeWalker &TreeWalker::operator=(const TreeWalker &other)
{
    if (impl == other.impl)
        return *this;
    
    TreeWalkerImpl *old = impl;
    impl = other.impl;
    if (impl) 
        impl->ref();
    if (old) 
        old->deref();

    return *this;
}

TreeWalker::~TreeWalker()
{
    if (impl) 
        impl->deref();
}

Node TreeWalker::root() const
{
    if (impl) 
        return impl->root();
    return 0;
}

unsigned TreeWalker::whatToShow() const
{
    if (impl) 
        return impl->whatToShow();
    return 0;
}

NodeFilter TreeWalker::filter() const
{
    if (impl) 
        return impl->filter();
    return NodeFilter();
}

bool TreeWalker::expandEntityReferences() const
{
    if (impl) return impl->expandEntityReferences();
    return false;
}

Node TreeWalker::currentNode() const
{
    if (impl) 
        return impl->currentNode();
    return 0;
}

void TreeWalker::setCurrentNode(const Node &node)
{
    if (impl) {
        int exceptioncode = 0;
        impl->setCurrentNode(node.handle(), exceptioncode);
        if (exceptioncode)
            throw DOMException(exceptioncode);
    }
}

Node TreeWalker::parentNode()
{
    if (impl) 
        return impl->parentNode();
    return 0;
}

Node TreeWalker::firstChild()
{
    if (impl) 
        return impl->firstChild();
    return 0;
}

Node TreeWalker::lastChild()
{
    if (impl) 
        return impl->lastChild();
    return 0;
}

Node TreeWalker::previousSibling()
{
    if (impl) 
        return impl->previousSibling();
    return 0;
}

Node TreeWalker::nextSibling()
{
    if (impl) 
        return impl->nextSibling();
    return 0;
}

Node TreeWalker::previousNode()
{
    if (impl) 
        return impl->previousNode();
    return 0;
}

Node TreeWalker::nextNode()
{
    if (impl) 
        return impl->nextNode();
    return 0;
}

#endif

} // namespace DOM
