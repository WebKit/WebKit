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

#ifndef dom2_traversal_h
#define dom2_traversal_h

#include "Shared.h"
#include <kxmlcore/Noncopyable.h>

namespace KXMLCore {
    template <class T> class PassRefPtr;
}
using KXMLCore::PassRefPtr;

namespace WebCore {

class NodeImpl;
class DocumentImpl;

typedef int ExceptionCode;

class NodeFilterCondition : public Shared<NodeFilterCondition> {
public:
    virtual ~NodeFilterCondition() { }
    virtual short acceptNode(NodeImpl*) const;
    virtual void mark() { }
};

class NodeFilterImpl : public Shared<NodeFilterImpl>, Noncopyable {
public:
    /**
     * The following constants are returned by the acceptNode()
     * method:
     */
    enum {
        FILTER_ACCEPT = 1,
        FILTER_REJECT = 2,
        FILTER_SKIP   = 3
    };

    /**
     * These are the available values for the whatToShow parameter.
     * They are the same as the set of possible types for Node, and
     * their values are derived by using a bit position corresponding
     * to the value of NodeType for the equivalent node type.
     */
    enum {
        SHOW_ALL                       = 0xFFFFFFFF,
        SHOW_ELEMENT                   = 0x00000001,
        SHOW_ATTRIBUTE                 = 0x00000002,
        SHOW_TEXT                      = 0x00000004,
        SHOW_CDATA_SECTION             = 0x00000008,
        SHOW_ENTITY_REFERENCE          = 0x00000010,
        SHOW_ENTITY                    = 0x00000020,
        SHOW_PROCESSING_INSTRUCTION    = 0x00000040,
        SHOW_COMMENT                   = 0x00000080,
        SHOW_DOCUMENT                  = 0x00000100,
        SHOW_DOCUMENT_TYPE             = 0x00000200,
        SHOW_DOCUMENT_FRAGMENT         = 0x00000400,
        SHOW_NOTATION                  = 0x00000800
    };

    NodeFilterImpl(NodeFilterCondition*);
    short acceptNode(NodeImpl*) const;
    void mark() { m_condition->mark(); };

private:
    RefPtr<NodeFilterCondition> m_condition;
};

class TraversalImpl : public Shared<TraversalImpl>
{
public:
    TraversalImpl(NodeImpl*, unsigned whatToShow, PassRefPtr<NodeFilterImpl>, bool expandEntityReferences);
    virtual ~TraversalImpl();

    NodeImpl* root() const { return m_root.get(); }
    unsigned whatToShow() const { return m_whatToShow; }
    NodeFilterImpl* filter() const { return m_filter.get(); }
    bool expandEntityReferences() const { return m_expandEntityReferences; }

    short acceptNode(NodeImpl*) const;

private:
    TraversalImpl(const TraversalImpl &);
    TraversalImpl &operator=(const TraversalImpl &);
    
    RefPtr<NodeImpl> m_root;
    unsigned m_whatToShow;
    RefPtr<NodeFilterImpl> m_filter;
    bool m_expandEntityReferences;
};

class NodeIteratorImpl : public TraversalImpl
{
public:
    NodeIteratorImpl(NodeImpl*, unsigned whatToShow, PassRefPtr<NodeFilterImpl>, bool expandEntityReferences);
    ~NodeIteratorImpl();

    NodeImpl* nextNode(ExceptionCode&);
    NodeImpl* previousNode(ExceptionCode&);
    void detach(ExceptionCode&);

    NodeImpl* referenceNode() const { return m_referenceNode.get(); }
    bool pointerBeforeReferenceNode() const { return m_beforeReferenceNode; }

    /**
     * This function has to be called if you delete a node from the
     * document tree and you want the Iterator to react if there
     * are any changes concerning it.
     */
    void notifyBeforeNodeRemoval(NodeImpl* removed);

private:
    void setReferenceNode(NodeImpl*);
    void setPointerBeforeReferenceNode(bool flag = true) { m_beforeReferenceNode = flag; }
    bool detached() const { return m_detached; }
    void setDetached(bool flag = true) { m_detached = flag; }
    DocumentImpl* document() const { return m_doc.get(); }
    NodeImpl* findNextNode(NodeImpl*) const;
    NodeImpl* findPreviousNode(NodeImpl*) const;

    RefPtr<NodeImpl> m_referenceNode;
    bool m_beforeReferenceNode;
    bool m_detached;
    RefPtr<DocumentImpl> m_doc;
};

class TreeWalkerImpl : public TraversalImpl {
public:
    TreeWalkerImpl(NodeImpl*, unsigned whatToShow, PassRefPtr<NodeFilterImpl>, bool expandEntityReferences);

    NodeImpl* currentNode() const { return m_current.get(); }
    void setCurrentNode(NodeImpl*, ExceptionCode&);

    NodeImpl* parentNode();
    NodeImpl* firstChild();
    NodeImpl* lastChild();
    NodeImpl* previousSibling();
    NodeImpl* nextSibling();
    NodeImpl* previousNode();
    NodeImpl* nextNode();

private:
    // convenience for when it is known there will be no exception
    void setCurrentNode(NodeImpl*);
    bool ancestorRejected(const NodeImpl*) const;

    RefPtr<NodeImpl> m_current;
};

} // namespace

#endif
