/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2004 Apple Computer, Inc.
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

#include "xml/dom2_traversalimpl.h"
#include "dom/dom_exception.h"
#include "xml/dom_docimpl.h"

#include "htmltags.h"

namespace DOM {

NodeFilterImpl::NodeFilterImpl(NodeFilterCondition *condition)
    : m_condition(condition)
{
    if (m_condition)
        m_condition->ref();
}

NodeFilterImpl::~NodeFilterImpl()
{
    if (m_condition)
        m_condition->deref();
}

short NodeFilterImpl::acceptNode(const Node &node) const
{
    // cast to short silences "enumeral and non-enumeral types in return" warning
    return m_condition ? m_condition->acceptNode(node) : static_cast<short>(NodeFilter::FILTER_ACCEPT);
}

// --------------------------------------------------------------

TraversalImpl::TraversalImpl(NodeImpl *rootNode, long whatToShow, NodeFilterImpl *nodeFilter, bool expandEntityReferences)
    : m_root(rootNode), m_whatToShow(whatToShow), m_filter(nodeFilter), m_expandEntityReferences(expandEntityReferences)
{
    if (root())
        root()->ref();
    if (filter())
        filter()->ref();
}

TraversalImpl::~TraversalImpl()
{
    if (root())
        root()->deref();
    if (filter())
        filter()->deref();
}

short TraversalImpl::acceptNode(NodeImpl *node) const
{
    // FIXME: If XML is implemented we have to check expandEntityRerefences in this function.
    // The bid twiddling here is done to map DOM node types, which are given as integers from
    // 1 through 12, to whatToShow bit masks.
    if (node && ((1 << (node->nodeType()-1)) & m_whatToShow) != 0)
        // cast to short silences "enumeral and non-enumeral types in return" warning
        return m_filter ? m_filter->acceptNode(node) : static_cast<short>(NodeFilter::FILTER_ACCEPT);
    return NodeFilter::FILTER_SKIP;
}

NodeImpl *TraversalImpl::findParentNode(NodeImpl *node, short accept) const
{
    if (!node || node == root())
        return 0;
    NodeImpl *n = node->parentNode();
    while (n) {
        if (acceptNode(n) & accept)
            return n;
        if (n == root())
            return 0;
        n = n->parentNode();
    }
    return 0;
}

NodeImpl *TraversalImpl::findFirstChild(NodeImpl *node) const
{
    if (!node || acceptNode(node) == NodeFilter::FILTER_REJECT)
        return 0;
    NodeImpl *n = node->firstChild();
    while (n) {
        if (acceptNode(n) == NodeFilter::FILTER_ACCEPT)
            return n;
        n = n->nextSibling();
    }
    return 0;
}

NodeImpl *TraversalImpl::findLastChild(NodeImpl *node) const
{
    if (!node || acceptNode(node) == NodeFilter::FILTER_REJECT)
        return 0;
    NodeImpl *n = node->lastChild();
    while (n) {
        if (acceptNode(n) == NodeFilter::FILTER_ACCEPT)
            return n;
        n = n->previousSibling();
    }
    return 0;
}

NodeImpl *TraversalImpl::findPreviousSibling(NodeImpl *node) const
{
    if (!node)
        return 0;
    NodeImpl *n = node->previousSibling();
    while (n) {
        if (acceptNode(n) == NodeFilter::FILTER_ACCEPT)
            return n;
        n = n->previousSibling();
    }
    return 0;
}

NodeImpl *TraversalImpl::findNextSibling(NodeImpl *node) const
{
    if (!node)
        return 0;
    NodeImpl *n = node->nextSibling();
    while (n) {
        if (acceptNode(n) == NodeFilter::FILTER_ACCEPT)
            return n;
        n = n->nextSibling();
    }
    return 0;
}

NodeImpl *TraversalImpl::findLastDescendant(NodeImpl *node) const
{
    NodeImpl *n = node;
    NodeImpl *r = node;
    while (n) {
        short accepted = acceptNode(n);
        if (accepted != NodeFilter::FILTER_REJECT) {
            if (accepted == NodeFilter::FILTER_ACCEPT)
                r = n;
            if (n->lastChild())
                n = n->lastChild();
            else if (n != node && n->previousSibling())
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
    while (n) {
        short accepted = acceptNode(n);
        if (accepted != NodeFilter::FILTER_REJECT) {
            NodeImpl *d = findLastDescendant(n);
            if (acceptNode(d) == NodeFilter::FILTER_ACCEPT)
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
    while (n) {
        switch (acceptNode(n)) {
            case NodeFilter::FILTER_ACCEPT:
                return n;
            case NodeFilter::FILTER_SKIP:
                if (n->firstChild())
                    n = n->firstChild();
                else
                    n = n->nextSibling();
                break;
            case NodeFilter::FILTER_REJECT:
                n = n->nextSibling();
                break;
        }
    }

    n = node->nextSibling();
    while (n) {
        switch (acceptNode(n)) {
            case NodeFilter::FILTER_ACCEPT:
                return n;
            case NodeFilter::FILTER_SKIP:
                return findNextNode(n);
            case NodeFilter::FILTER_REJECT:
                n = n->nextSibling();
                break;
        }
    }

    NodeImpl *parent = findParentNode(node, NodeFilter::FILTER_ACCEPT | NodeFilter::FILTER_SKIP);
    while (parent) { 
        n = parent->nextSibling();
        while (n) {
            switch (acceptNode(n)) {
                case NodeFilter::FILTER_ACCEPT:
                    return n;
                case NodeFilter::FILTER_SKIP:
                    return findNextNode(n);
                case NodeFilter::FILTER_REJECT:
                    n = n->nextSibling();
                    break;
            }
        }
        parent = findParentNode(parent, NodeFilter::FILTER_ACCEPT | NodeFilter::FILTER_SKIP);
    }

    return 0;
}

// --------------------------------------------------------------

NodeIteratorImpl::NodeIteratorImpl(NodeImpl *rootNode, long whatToShow, NodeFilterImpl *filter, bool expandEntityReferences)
    :TraversalImpl(rootNode, whatToShow, filter, expandEntityReferences), m_referenceNode(0), m_beforeReferenceNode(true), m_detached(false), m_doc(0)
{
    if (root()) {
        setDocument(root()->getDocument());
        if (document()) {
            document()->attachNodeIterator(this);
            document()->ref();
        }
    }
}

NodeIteratorImpl::~NodeIteratorImpl()
{
    if (referenceNode())
        referenceNode()->deref();
    if (document()) {
        document()->detachNodeIterator(this);
        document()->deref();
    }
}

NodeImpl *NodeIteratorImpl::nextNode(int &exceptioncode)
{
    if (detached()) {
        exceptioncode = DOMException::INVALID_STATE_ERR;
        return 0;
    }

    NodeImpl *result = 0;
    NodeImpl *refNode = referenceNode() ? referenceNode() : root();

    if (pointerBeforeReferenceNode() && acceptNode(refNode) == NodeFilter::FILTER_ACCEPT)
        result = refNode;
    else
        result = findNextNode(refNode);

    if (result)
        setReferenceNode(result);
    setPointerBeforeReferenceNode(false);

    return result;
}

NodeImpl *NodeIteratorImpl::previousNode(int &exceptioncode)
{
    if (detached()) {
        exceptioncode = DOMException::INVALID_STATE_ERR;
        return 0;
    }

    NodeImpl *result = 0;
    NodeImpl *refNode = referenceNode() ? referenceNode() : root();

    if (!pointerBeforeReferenceNode() && acceptNode(refNode) == NodeFilter::FILTER_ACCEPT)
        result = refNode;
    else
        result = findPreviousNode(refNode);

    if (result)
        setReferenceNode(result);
    setPointerBeforeReferenceNode();

    return result;
}

void NodeIteratorImpl::detach(int &/*exceptioncode*/)
{
    if (!detached() && document())
        document()->detachNodeIterator(this);
    setDetached();
}

void NodeIteratorImpl::setReferenceNode(NodeImpl *node)
{
    if (node == m_referenceNode)
        return;
    
    NodeImpl *old = m_referenceNode;
    m_referenceNode = node;
    if (m_referenceNode)
        m_referenceNode->ref();
    if (old)
        old->deref();
}

void NodeIteratorImpl::setDocument(DocumentImpl *doc)
{
    if (doc == m_doc)
        return;
    
    DocumentImpl *old = m_doc;
    m_doc = doc;
    if (m_doc)
        m_doc->ref();
    if (old)
        old->deref();
}

void NodeIteratorImpl::notifyBeforeNodeRemoval(NodeImpl *willRemove)
{
    // Iterator is not affected if the removed node is the reference node and is the root.
    // or if removed node is not the reference node, or the ancestor of the reference node.
    if (!willRemove || willRemove == root())
        return;
    bool willRemoveReferenceNode = willRemove == referenceNode();
    bool willRemoveReferenceNodeAncestor = willRemove->isAncestor(referenceNode());
    if (!willRemoveReferenceNode && !willRemoveReferenceNodeAncestor)
        return;

    if (pointerBeforeReferenceNode()) {
        NodeImpl *node = findNextNode(willRemove);
        if (node) {
            // Move out from under the node being removed if the reference node is
            // a descendant of the node being removed.
            if (willRemoveReferenceNodeAncestor) {
                while (node && willRemove->isAncestor(node))
                    node = findNextNode(node);
            }
            if (node)
                setReferenceNode(node);
        }
        else {
            node = findPreviousNode(willRemove);
            if (node) {
                // Move out from under the node being removed if the reference node is
                // a descendant of the node being removed.
                if (willRemoveReferenceNodeAncestor) {
                    while (node && willRemove->isAncestor(node))
                        node = findPreviousNode(node);
                }
                if (node) {
                    // Removing last node.
                    // Need to move the pointer after the node preceding the 
                    // new reference node.
                    setReferenceNode(node);
                    setPointerBeforeReferenceNode(false);
                }
            }
        }
    }
    else {
        NodeImpl *node = findPreviousNode(willRemove);
        if (node) {
            // Move out from under the node being removed if the reference node is
            // a descendant of the node being removed.
            if (willRemoveReferenceNodeAncestor) {
                while (node && willRemove->isAncestor(node))
                    node = findPreviousNode(node);
            }
            if (node)
                setReferenceNode(node);
        }
        else {
            node = findNextNode(willRemove);
                // Move out from under the node being removed if the reference node is
                // a descendant of the node being removed.
                if (willRemoveReferenceNodeAncestor) {
                    while (node && willRemove->isAncestor(node))
                        node = findPreviousNode(node);
                }
                if (node)
                    setReferenceNode(node);
        }
    }
}

// --------------------------------------------------------------

TreeWalkerImpl::TreeWalkerImpl(NodeImpl *rootNode, long whatToShow, NodeFilterImpl *filter, bool expandEntityReferences)
    : TraversalImpl(rootNode, whatToShow, filter, expandEntityReferences), m_current(rootNode)
{
    if (currentNode())
        currentNode()->ref();
}

TreeWalkerImpl::~TreeWalkerImpl()
{
    if (currentNode())
        currentNode()->deref();
}

void TreeWalkerImpl::setCurrentNode(NodeImpl *node, int &exceptioncode)
{
    if (!node) {
        exceptioncode = DOMException::NOT_SUPPORTED_ERR;
        return;
    }

    if (node == m_current)
        return;
    
    NodeImpl *old = m_current;
    m_current = node;
    if (m_current)
        m_current->ref();
    if (old)
        old->deref();
}

void TreeWalkerImpl::setCurrentNode(NodeImpl *node)
{
    assert(node);
    int dummy;
    setCurrentNode(node, dummy);
}

NodeImpl *TreeWalkerImpl::parentNode()
{
    NodeImpl *node = findParentNode(currentNode());
    if (node)
        setCurrentNode(node);
    return node;
}

NodeImpl *TreeWalkerImpl::firstChild()
{
    NodeImpl *node = findFirstChild(currentNode());
    if (node)
        setCurrentNode(node);
    return node;
}

NodeImpl *TreeWalkerImpl::lastChild()
{
    NodeImpl *node = findLastChild(currentNode());
    if (node)
        setCurrentNode(node);
    return node;
}

NodeImpl *TreeWalkerImpl::previousSibling()
{
    NodeImpl *node = findPreviousSibling(currentNode());
    if (node)
        setCurrentNode(node);
    return node;
}

NodeImpl *TreeWalkerImpl::nextSibling()
{
    NodeImpl *node = findNextSibling(currentNode());
    if (node)
        setCurrentNode(node);
    return node;
}

NodeImpl *TreeWalkerImpl::previousNode()
{
    NodeImpl *node = findPreviousNode(currentNode());
    if (node)
        setCurrentNode(node);
    return node;
}

NodeImpl *TreeWalkerImpl::nextNode()
{
    NodeImpl *node = findNextNode(currentNode());
    if (node)
        setCurrentNode(node);
    return node;
}

} // namespace DOM
