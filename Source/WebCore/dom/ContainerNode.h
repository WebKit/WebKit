/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#include "ExceptionCodePlaceholder.h"
#include "Node.h"

#include <wtf/OwnPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class FloatPoint;
    
typedef void (*NodeCallback)(Node*, unsigned);

namespace Private { 
    template<class GenericNode, class GenericNodeContainer>
    void addChildNodesToDeletionQueue(GenericNode*& head, GenericNode*& tail, GenericNodeContainer*);
};

class ContainerNode : public Node {
public:
    virtual ~ContainerNode();

    Node* firstChild() const { return m_firstChild; }
    Node* lastChild() const { return m_lastChild; }
    bool hasChildNodes() const { return m_firstChild; }

    unsigned childNodeCount() const;
    Node* childNode(unsigned index) const;

    bool insertBefore(PassRefPtr<Node> newChild, Node* refChild, ExceptionCode& = ASSERT_NO_EXCEPTION, bool shouldLazyAttach = false);
    bool replaceChild(PassRefPtr<Node> newChild, Node* oldChild, ExceptionCode& = ASSERT_NO_EXCEPTION, bool shouldLazyAttach = false);
    bool removeChild(Node* child, ExceptionCode& = ASSERT_NO_EXCEPTION);
    bool appendChild(PassRefPtr<Node> newChild, ExceptionCode& = ASSERT_NO_EXCEPTION, bool shouldLazyAttach = false);

    // These methods are only used during parsing.
    // They don't send DOM mutation events or handle reparenting.
    // However, arbitrary code may be run by beforeload handlers.
    void parserAddChild(PassRefPtr<Node>);
    void parserRemoveChild(Node*);
    void parserInsertBefore(PassRefPtr<Node> newChild, Node* refChild);

    // FIXME: It's not good to have two functions with such similar names, especially public functions.
    // How do removeChildren and removeAllChildren differ?
    void removeChildren();
    void removeAllChildren();

    void takeAllChildrenFrom(ContainerNode*);

    void cloneChildNodes(ContainerNode* clone);

    virtual void attach() OVERRIDE;
    virtual void detach() OVERRIDE;
    virtual LayoutRect getRect() const OVERRIDE;
    virtual void setFocus(bool = true) OVERRIDE;
    virtual void setActive(bool active = true, bool pause = false) OVERRIDE;
    virtual void setHovered(bool = true) OVERRIDE;
    virtual void scheduleSetNeedsStyleRecalc(StyleChangeType = FullStyleChange) OVERRIDE;

    // -----------------------------------------------------------------------------
    // Notification of document structure changes (see Node.h for more notification methods)

    // Notifies the node that it's list of children have changed (either by adding or removing child nodes), or a child
    // node that is of the type CDATA_SECTION_NODE, TEXT_NODE or COMMENT_NODE has changed its value.
    virtual void childrenChanged(bool createdByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

    void attachAsNode();
    void attachChildren();
    void attachChildrenIfNeeded();
    void attachChildrenLazily();
    void detachChildren();
    void detachChildrenIfNeeded();

    void disconnectDescendantFrames();

    // More efficient versions of these two functions for the case where we are starting with a ContainerNode.
    Node* traverseNextNode() const;
    Node* traverseNextNode(const Node* stayWithin) const;

    virtual void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        MemoryClassInfo info(memoryObjectInfo, this, MemoryInstrumentation::DOM);
        Node::reportMemoryUsage(memoryObjectInfo);
        info.addInstrumentedMember(m_firstChild);
        info.addInstrumentedMember(m_lastChild);
    }

protected:
    ContainerNode(Document*, ConstructionType = CreateContainer);

    static void queuePostAttachCallback(NodeCallback, Node*, unsigned = 0);
    static bool postAttachCallbacksAreSuspended();
    void suspendPostAttachCallbacks();
    void resumePostAttachCallbacks();

    template<class GenericNode, class GenericNodeContainer>
    friend void appendChildToContainer(GenericNode* child, GenericNodeContainer*);

    template<class GenericNode, class GenericNodeContainer>
    friend void Private::addChildNodesToDeletionQueue(GenericNode*& head, GenericNode*& tail, GenericNodeContainer*);

    void setFirstChild(Node* child) { m_firstChild = child; }
    void setLastChild(Node* child) { m_lastChild = child; }

private:
    void removeBetween(Node* previousChild, Node* nextChild, Node* oldChild);
    void insertBeforeCommon(Node* nextChild, Node* oldChild);

    static void dispatchPostAttachCallbacks();

    bool getUpperLeftCorner(FloatPoint&) const;
    bool getLowerRightCorner(FloatPoint&) const;

    Node* m_firstChild;
    Node* m_lastChild;
};

inline ContainerNode* toContainerNode(Node* node)
{
    ASSERT(!node || node->isContainerNode());
    return static_cast<ContainerNode*>(node);
}

inline const ContainerNode* toContainerNode(const Node* node)
{
    ASSERT(!node || node->isContainerNode());
    return static_cast<const ContainerNode*>(node);
}

// This will catch anyone doing an unnecessary cast.
void toContainerNode(const ContainerNode*);

inline ContainerNode::ContainerNode(Document* document, ConstructionType type)
    : Node(document, type)
    , m_firstChild(0)
    , m_lastChild(0)
{
}

inline void ContainerNode::attachAsNode()
{
    Node::attach();
}

inline void ContainerNode::attachChildren()
{
    for (Node* child = firstChild(); child; child = child->nextSibling())
        child->attach();
}

inline void ContainerNode::attachChildrenIfNeeded()
{
    for (Node* child = firstChild(); child; child = child->nextSibling()) {
        if (!child->attached())
            child->attach();
    }
}

inline void ContainerNode::attachChildrenLazily()
{
    for (Node* child = firstChild(); child; child = child->nextSibling())
        if (!child->attached())
            child->lazyAttach();
}

inline void ContainerNode::detachChildrenIfNeeded()
{
    for (Node* child = firstChild(); child; child = child->nextSibling()) {
        if (child->attached())
            child->detach();
    }
}

inline void ContainerNode::detachChildren()
{
    for (Node* child = firstChild(); child; child = child->nextSibling())
        child->detach();
}

inline unsigned Node::childNodeCount() const
{
    if (!isContainerNode())
        return 0;
    return toContainerNode(this)->childNodeCount();
}

inline Node* Node::childNode(unsigned index) const
{
    if (!isContainerNode())
        return 0;
    return toContainerNode(this)->childNode(index);
}

inline Node* Node::firstChild() const
{
    if (!isContainerNode())
        return 0;
    return toContainerNode(this)->firstChild();
}

inline Node* Node::lastChild() const
{
    if (!isContainerNode())
        return 0;
    return toContainerNode(this)->lastChild();
}

inline Node* Node::highestAncestor() const
{
    Node* node = const_cast<Node*>(this);
    Node* highest = node;
    for (; node; node = node->parentNode())
        highest = node;
    return highest;
}

inline Node* Node::traverseNextSibling() const
{
    if (nextSibling())
        return nextSibling();
    return traverseNextAncestorSibling();
}

inline Node* Node::traverseNextNode() const
{
    if (firstChild())
        return firstChild();
    return traverseNextSibling();
}

inline Node* ContainerNode::traverseNextNode() const
{
    // More efficient than the Node::traverseNextNode above, because
    // this does not need to do the isContainerNode check inside firstChild.
    if (firstChild())
        return firstChild();
    return traverseNextSibling();
}

inline Node* Node::traverseNextSibling(const Node* stayWithin) const
{
    if (this == stayWithin)
        return 0;
    if (nextSibling())
        return nextSibling();
    return traverseNextAncestorSibling(stayWithin);
}

inline Node* Node::traverseNextNode(const Node* stayWithin) const
{
    if (firstChild())
        return firstChild();
    return traverseNextSibling(stayWithin);
}

inline Node* ContainerNode::traverseNextNode(const Node* stayWithin) const
{
    // More efficient than the Node::traverseNextNode above, because
    // this does not need to do the isContainerNode check inside firstChild.
    if (firstChild())
        return firstChild();
    return traverseNextSibling(stayWithin);
}

typedef Vector<RefPtr<Node>, 11> NodeVector;

inline void getChildNodes(Node* node, NodeVector& nodes)
{
    ASSERT(!nodes.size());
    for (Node* child = node->firstChild(); child; child = child->nextSibling())
        nodes.append(child);
}

class ChildNodesLazySnapshot {
    WTF_MAKE_NONCOPYABLE(ChildNodesLazySnapshot);
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit ChildNodesLazySnapshot(Node* parentNode)
        : m_currentNode(parentNode->firstChild())
        , m_currentIndex(0)
    {
        m_nextSnapshot = latestSnapshot;
        latestSnapshot = this;
    }

    ~ChildNodesLazySnapshot()
    {
        latestSnapshot = m_nextSnapshot;
    }

    // Returns 0 if there is no next Node.
    Node* nextNode()
    {
        if (LIKELY(!hasSnapshot())) {
            Node* node = m_currentNode;
            if (m_currentNode)
                m_currentNode = m_currentNode->nextSibling();
            return node;
        }
        Vector<RefPtr<Node> >* nodeVector = m_childNodes.get();
        if (m_currentIndex >= nodeVector->size())
            return 0;
        return (*nodeVector)[m_currentIndex++].get();
    }

    void takeSnapshot()
    {
        if (hasSnapshot())
            return;
        m_childNodes = adoptPtr(new Vector<RefPtr<Node> >());
        Node* node = m_currentNode;
        while (node) {
            m_childNodes->append(node);
            node = node->nextSibling();
        }
    }

    ChildNodesLazySnapshot* nextSnapshot() { return m_nextSnapshot; }
    bool hasSnapshot() { return !!m_childNodes.get(); }

    static void takeChildNodesLazySnapshot()
    {
        ChildNodesLazySnapshot* snapshot = latestSnapshot;
        while (snapshot && !snapshot->hasSnapshot()) {
            snapshot->takeSnapshot();
            snapshot = snapshot->nextSnapshot();
        }
    }

private:
    static ChildNodesLazySnapshot* latestSnapshot;

    Node* m_currentNode;
    unsigned m_currentIndex;
    OwnPtr<Vector<RefPtr<Node> > > m_childNodes; // Lazily instantiated.
    ChildNodesLazySnapshot* m_nextSnapshot;
};

} // namespace WebCore

#endif // ContainerNode_h
