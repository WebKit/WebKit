/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef ContainerNode_h
#define ContainerNode_h

#include "Node.h"
#include "FloatPoint.h"

namespace WebCore {
    
typedef void (*NodeCallback)(Node*);

namespace Private { 
    template<class GenericNode, class GenericNodeContainer>
    void addChildNodesToDeletionQueue(GenericNode*& head, GenericNode*& tail, GenericNodeContainer* container);
};

class ContainerNode : public Node {
public:
    virtual ~ContainerNode();

    Node* firstChild() const { return m_firstChild; }
    Node* lastChild() const { return m_lastChild; }

    virtual bool insertBefore(PassRefPtr<Node> newChild, Node* refChild, ExceptionCode&, bool shouldLazyAttach = false);
    virtual bool replaceChild(PassRefPtr<Node> newChild, Node* oldChild, ExceptionCode&, bool shouldLazyAttach = false);
    virtual bool removeChild(Node* child, ExceptionCode&);
    virtual bool appendChild(PassRefPtr<Node> newChild, ExceptionCode&, bool shouldLazyAttach = false);

    // These methods are only used during parsing.
    // They don't send DOM mutation events or handle reparenting.
    // However, arbitrary code may be run by beforeload handlers.
    void parserAddChild(PassRefPtr<Node>);
    void parserRemoveChild(Node*);
    void parserInsertBefore(PassRefPtr<Node> newChild, Node* refChild);

    bool hasChildNodes() const { return m_firstChild; }
    virtual void attach();
    virtual void detach();
    virtual void willRemove();
    virtual IntRect getRect() const;
    virtual void setFocus(bool = true);
    virtual void setActive(bool active = true, bool pause = false);
    virtual void setHovered(bool = true);
    unsigned childNodeCount() const;
    Node* childNode(unsigned index) const;

    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
    virtual void insertedIntoTree(bool deep);
    virtual void removedFromTree(bool deep);
    virtual void childrenChanged(bool createdByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

    virtual bool removeChildren();

    void removeAllChildren();
    void takeAllChildrenFrom(ContainerNode*);

    void cloneChildNodes(ContainerNode* clone);
    
    bool dispatchBeforeLoadEvent(const String& sourceURL);

    static void queuePostAttachCallback(NodeCallback, Node*);
    static bool postAttachCallbacksAreSuspended();
    
protected:
    ContainerNode(Document*, ConstructionType = CreateContainer);

    void suspendPostAttachCallbacks();
    void resumePostAttachCallbacks();

    template<class GenericNode, class GenericNodeContainer>
    friend void appendChildToContainer(GenericNode* child, GenericNodeContainer* container);

    template<class GenericNode, class GenericNodeContainer>
    friend void Private::addChildNodesToDeletionQueue(GenericNode*& head, GenericNode*& tail, GenericNodeContainer* container);

    void setFirstChild(Node* child) { m_firstChild = child; }
    void setLastChild(Node* child) { m_lastChild = child; }

private:
    // Never call this function directly.  If you're trying to call this
    // function, your code is either wrong or you're supposed to call
    // parserAddChild.  Please do not call parserAddChild unless you are the
    // parser!
    virtual void deprecatedParserAddChild(PassRefPtr<Node>);

    void removeBetween(Node* previousChild, Node* nextChild, Node* oldChild);
    void insertBeforeCommon(Node* nextChild, Node* oldChild);

    static void dispatchPostAttachCallbacks();

    bool getUpperLeftCorner(FloatPoint&) const;
    bool getLowerRightCorner(FloatPoint&) const;

    Node* m_firstChild;
    Node* m_lastChild;
};

inline ContainerNode::ContainerNode(Document* document, ConstructionType type)
    : Node(document, type)
    , m_firstChild(0)
    , m_lastChild(0)
{
}

inline unsigned Node::containerChildNodeCount() const
{
    ASSERT(isContainerNode());
    return static_cast<const ContainerNode*>(this)->childNodeCount();
}

inline Node* Node::containerChildNode(unsigned index) const
{
    ASSERT(isContainerNode());
    return static_cast<const ContainerNode*>(this)->childNode(index);
}

inline Node* Node::containerFirstChild() const
{
    ASSERT(isContainerNode());
    return static_cast<const ContainerNode*>(this)->firstChild();
}

inline Node* Node::containerLastChild() const
{
    ASSERT(isContainerNode());
    return static_cast<const ContainerNode*>(this)->lastChild();
}

} // namespace WebCore

#endif // ContainerNode_h
