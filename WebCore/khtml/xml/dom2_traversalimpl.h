/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * (C) 2001 Peter Kelly (pmk@post.com)
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

#ifndef _dom2_traversal_h
#define _dom2_traversal_h

#include "dom/dom2_traversal.h"
#include "shared.h"

namespace DOM {

class NodeImpl;
class DocumentImpl;

class NodeFilterImpl : public khtml::Shared<NodeFilterImpl>
{
public:
    NodeFilterImpl(NodeFilterCondition *);
    ~NodeFilterImpl();
    
    short acceptNode(NodeImpl *) const;
    
private:
    NodeFilterImpl(const NodeFilterImpl &);
    NodeFilterImpl &operator=(const NodeFilterImpl &);

    NodeFilterCondition *m_condition;
};

class TraversalImpl : public khtml::Shared<TraversalImpl>
{
public:
    TraversalImpl(NodeImpl *, int whatToShow, NodeFilterImpl *, bool expandEntityReferences);
    ~TraversalImpl();

    NodeImpl *root() const { return m_root; }
    unsigned whatToShow() const { return m_whatToShow; }
    NodeFilterImpl *filter() const { return m_filter; }
    bool expandEntityReferences() const { return m_expandEntityReferences; }

    short acceptNode(NodeImpl *) const;

private:
    TraversalImpl(const TraversalImpl &);
    TraversalImpl &operator=(const TraversalImpl &);
    
    NodeImpl *m_root;
    int m_whatToShow;
    NodeFilterImpl *m_filter;
    bool m_expandEntityReferences;
};

class NodeIteratorImpl : public TraversalImpl
{
public:
    NodeIteratorImpl(NodeImpl *, int whatToShow, NodeFilterImpl *, bool expandEntityReferences);
    ~NodeIteratorImpl();

    NodeImpl *nextNode(int &exceptioncode);
    NodeImpl *previousNode(int &exceptioncode);
    void detach(int &exceptioncode);

    NodeImpl *referenceNode() const { return m_referenceNode; }
    bool pointerBeforeReferenceNode() const { return m_beforeReferenceNode; }

    /**
     * This function has to be called if you delete a node from the
     * document tree and you want the Iterator to react if there
     * are any changes concerning it.
     */
    void notifyBeforeNodeRemoval(NodeImpl *removed);

private:
    NodeIteratorImpl(const NodeIteratorImpl &);
    NodeIteratorImpl &operator=(const NodeIteratorImpl &);
    
    void setReferenceNode(NodeImpl *);
    void setPointerBeforeReferenceNode(bool flag=true) { m_beforeReferenceNode = flag; }
    bool detached() const { return m_detached; }
    void setDetached(bool flag=true) { m_detached = flag; }
    DocumentImpl * document() const { return m_doc; }
    void setDocument(DocumentImpl *);
    NodeImpl *findNextNode(NodeImpl *) const;
    NodeImpl *findPreviousNode(NodeImpl *) const;

    NodeImpl *m_referenceNode;
    bool m_beforeReferenceNode;
    bool m_detached;
    DocumentImpl *m_doc;
};

class TreeWalkerImpl : public TraversalImpl
{
public:
    TreeWalkerImpl(NodeImpl *, int whatToShow, NodeFilterImpl *, bool expandEntityReferences);
    ~TreeWalkerImpl();

    NodeImpl *currentNode() const { return m_current; }
    void setCurrentNode(NodeImpl *, int &exceptioncode);

    NodeImpl *parentNode();
    NodeImpl *firstChild();
    NodeImpl *lastChild();
    NodeImpl *previousSibling();
    NodeImpl *nextSibling();
    NodeImpl *previousNode();
    NodeImpl *nextNode();

private:
    TreeWalkerImpl(const TreeWalkerImpl &);
    TreeWalkerImpl &operator=(const TreeWalkerImpl &);

    // convenience for when it is known there will be no exception
    void setCurrentNode(NodeImpl *);
    
    bool ancestorRejected(const NodeImpl *) const;
    
    NodeImpl *m_current;
};

} // namespace

#endif

