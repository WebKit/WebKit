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
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Node;
class Document;

typedef int ExceptionCode;

class NodeFilterCondition : public Shared<NodeFilterCondition> {
public:
    virtual ~NodeFilterCondition() { }
    virtual short acceptNode(Node*) const;
    virtual void mark() { }
};

class NodeFilter : public Shared<NodeFilter>, Noncopyable {
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

    NodeFilter(NodeFilterCondition*);
    short acceptNode(Node*) const;
    void mark() { m_condition->mark(); };

private:
    RefPtr<NodeFilterCondition> m_condition;
};

class Traversal : public Shared<Traversal>
{
public:
    Traversal(Node*, unsigned whatToShow, PassRefPtr<NodeFilter>, bool expandEntityReferences);
    virtual ~Traversal();

    Node* root() const { return m_root.get(); }
    unsigned whatToShow() const { return m_whatToShow; }
    NodeFilter* filter() const { return m_filter.get(); }
    bool expandEntityReferences() const { return m_expandEntityReferences; }

    short acceptNode(Node*) const;

private:
    Traversal(const Traversal &);
    Traversal &operator=(const Traversal &);
    
    RefPtr<Node> m_root;
    unsigned m_whatToShow;
    RefPtr<NodeFilter> m_filter;
    bool m_expandEntityReferences;
};

class NodeIterator : public Traversal
{
public:
    NodeIterator(Node*, unsigned whatToShow, PassRefPtr<NodeFilter>, bool expandEntityReferences);
    ~NodeIterator();

    Node* nextNode(ExceptionCode&);
    Node* previousNode(ExceptionCode&);
    void detach(ExceptionCode&);

    Node* referenceNode() const { return m_referenceNode.get(); }
    bool pointerBeforeReferenceNode() const { return m_beforeReferenceNode; }

    /**
     * This function has to be called if you delete a node from the
     * document tree and you want the Iterator to react if there
     * are any changes concerning it.
     */
    void notifyBeforeNodeRemoval(Node* removed);

private:
    void setReferenceNode(Node*);
    void setPointerBeforeReferenceNode(bool flag = true) { m_beforeReferenceNode = flag; }
    bool detached() const { return m_detached; }
    void setDetached(bool flag = true) { m_detached = flag; }
    Document* document() const { return m_doc.get(); }
    Node* findNextNode(Node*) const;
    Node* findPreviousNode(Node*) const;

    RefPtr<Node> m_referenceNode;
    bool m_beforeReferenceNode;
    bool m_detached;
    RefPtr<Document> m_doc;
};

class TreeWalker : public Traversal {
public:
    TreeWalker(Node*, unsigned whatToShow, PassRefPtr<NodeFilter>, bool expandEntityReferences);

    Node* currentNode() const { return m_current.get(); }
    void setCurrentNode(Node*, ExceptionCode&);

    Node* parentNode();
    Node* firstChild();
    Node* lastChild();
    Node* previousSibling();
    Node* nextSibling();
    Node* previousNode();
    Node* nextNode();

private:
    // convenience for when it is known there will be no exception
    void setCurrentNode(Node*);
    bool ancestorRejected(const Node*) const;

    RefPtr<Node> m_current;
};

} // namespace

#endif
