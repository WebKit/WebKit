/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2018 Apple Inc. All rights reserved.
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

#pragma once

#include "CollectionType.h"
#include "Node.h"

namespace WebCore {

class HTMLCollection;
class RadioNodeList;
class RenderElement;

const int initialNodeVectorSize = 11; // Covers 99.5%. See webkit.org/b/80706
typedef Vector<Ref<Node>, initialNodeVectorSize> NodeVector;

class ContainerNode : public Node {
    WTF_MAKE_ISO_ALLOCATED(ContainerNode);
public:
    virtual ~ContainerNode();

    Node* firstChild() const { return m_firstChild; }
    static ptrdiff_t firstChildMemoryOffset() { return OBJECT_OFFSETOF(ContainerNode, m_firstChild); }
    Node* lastChild() const { return m_lastChild; }
    static ptrdiff_t lastChildMemoryOffset() { return OBJECT_OFFSETOF(ContainerNode, m_lastChild); }
    bool hasChildNodes() const { return m_firstChild; }
    bool hasOneChild() const { return m_firstChild && m_firstChild == m_lastChild; }

    bool directChildNeedsStyleRecalc() const { return hasStyleFlag(NodeStyleFlag::DirectChildNeedsStyleResolution); }
    void setDirectChildNeedsStyleRecalc() { setStyleFlag(NodeStyleFlag::DirectChildNeedsStyleResolution); }

    WEBCORE_EXPORT unsigned countChildNodes() const;
    WEBCORE_EXPORT Node* traverseToChildAt(unsigned) const;

    ExceptionOr<void> insertBefore(Node& newChild, Node* refChild);
    ExceptionOr<void> replaceChild(Node& newChild, Node& oldChild);
    WEBCORE_EXPORT ExceptionOr<void> removeChild(Node& child);
    WEBCORE_EXPORT ExceptionOr<void> appendChild(Node& newChild);
    void stringReplaceAll(String&&);
    void replaceAll(Node*);

    ContainerNode& rootNode() const { return downcast<ContainerNode>(Node::rootNode()); }

    // These methods are only used during parsing.
    // They don't send DOM mutation events or handle reparenting.
    // However, arbitrary code may be run by beforeload handlers.
    void parserAppendChild(Node&);
    void parserRemoveChild(Node&);
    void parserInsertBefore(Node& newChild, Node& refChild);

    void removeChildren();

    void takeAllChildrenFrom(ContainerNode*);

    void cloneChildNodes(ContainerNode& clone);

    struct ChildChange {
        enum class Type : uint8_t { ElementInserted, ElementRemoved, TextInserted, TextRemoved, TextChanged, AllChildrenRemoved, NonContentsChildRemoved, NonContentsChildInserted, AllChildrenReplaced };
        enum class Source : bool { Parser, API };

        ChildChange::Type type;
        Element* siblingChanged;
        Element* previousSiblingElement;
        Element* nextSiblingElement;
        ChildChange::Source source;

        bool isInsertion() const
        {
            switch (type) {
            case ChildChange::Type::ElementInserted:
            case ChildChange::Type::TextInserted:
            case ChildChange::Type::NonContentsChildInserted:
            case ChildChange::Type::AllChildrenReplaced:
                return true;
            case ChildChange::Type::ElementRemoved:
            case ChildChange::Type::TextRemoved:
            case ChildChange::Type::TextChanged:
            case ChildChange::Type::AllChildrenRemoved:
            case ChildChange::Type::NonContentsChildRemoved:
                return false;
            }
            ASSERT_NOT_REACHED();
            return false;
        }

        bool affectsElements() const
        {
            switch (type) {
            case ChildChange::Type::ElementInserted:
            case ChildChange::Type::ElementRemoved:
            case ChildChange::Type::AllChildrenRemoved:
            case ChildChange::Type::AllChildrenReplaced:
                return true;
            case ChildChange::Type::TextInserted:
            case ChildChange::Type::TextRemoved:
            case ChildChange::Type::TextChanged:
            case ChildChange::Type::NonContentsChildInserted:
            case ChildChange::Type::NonContentsChildRemoved:
                return false;
            }
            ASSERT_NOT_REACHED();
            return false;
        }
    };
    virtual void childrenChanged(const ChildChange&);

    ExceptionOr<void> appendChild(ChildChange::Source, Node& newChild);

    void disconnectDescendantFrames();

    RenderElement* renderer() const;

    // Return a bounding box in absolute coordinates enclosing this node and all its descendants.
    // This gives the area within which events may get handled by a hander registered on this node.
    virtual LayoutRect absoluteEventHandlerBounds(bool& /* includesFixedPositionElements */) { return LayoutRect(); }

    WEBCORE_EXPORT ExceptionOr<Element*> querySelector(const String& selectors);
    WEBCORE_EXPORT ExceptionOr<Ref<NodeList>> querySelectorAll(const String& selectors);

    WEBCORE_EXPORT Ref<HTMLCollection> getElementsByTagName(const AtomString&);
    WEBCORE_EXPORT Ref<HTMLCollection> getElementsByTagNameNS(const AtomString& namespaceURI, const AtomString& localName);
    WEBCORE_EXPORT Ref<NodeList> getElementsByName(const AtomString& elementName);
    WEBCORE_EXPORT Ref<HTMLCollection> getElementsByClassName(const AtomString& classNames);
    Ref<RadioNodeList> radioNodeList(const AtomString&);

    // From the ParentNode interface - https://dom.spec.whatwg.org/#interface-parentnode
    WEBCORE_EXPORT Ref<HTMLCollection> children();
    WEBCORE_EXPORT Element* firstElementChild() const;
    WEBCORE_EXPORT Element* lastElementChild() const;
    WEBCORE_EXPORT unsigned childElementCount() const;
    ExceptionOr<void> append(FixedVector<NodeOrString>&&);
    ExceptionOr<void> prepend(FixedVector<NodeOrString>&&);

    ExceptionOr<void> replaceChildren(FixedVector<NodeOrString>&&);

    ExceptionOr<void> ensurePreInsertionValidity(Node& newChild, Node* refChild);

protected:
    explicit ContainerNode(Document&, ConstructionType = CreateContainer);

    friend void addChildNodesToDeletionQueue(Node*& head, Node*& tail, ContainerNode&);

    void removeDetachedChildren();
    void setFirstChild(Node* child) { m_firstChild = child; }
    void setLastChild(Node* child) { m_lastChild = child; }

    HTMLCollection* cachedHTMLCollection(CollectionType);

private:
    void executePreparedChildrenRemoval();
    enum class DeferChildrenChanged { Yes, No };
    void removeAllChildrenWithScriptAssertion(ChildChange::Source, NodeVector& children, DeferChildrenChanged = DeferChildrenChanged::No);
    bool removeNodeWithScriptAssertion(Node&, ChildChange::Source);
    ExceptionOr<void> removeSelfOrChildNodesForInsertion(Node&, NodeVector&);

    void removeBetween(Node* previousChild, Node* nextChild, Node& oldChild);
    ExceptionOr<void> appendChildWithoutPreInsertionValidityCheck(Node&);
    void insertBeforeCommon(Node& nextChild, Node& oldChild);
    void appendChildCommon(Node&);

    void rebuildSVGExtensionsElementsIfNecessary();

    bool isContainerNode() const = delete;

    Node* m_firstChild { nullptr };
    Node* m_lastChild { nullptr };
};

inline ContainerNode::ContainerNode(Document& document, ConstructionType type)
    : Node(document, type)
{
}

inline unsigned Node::countChildNodes() const
{
    auto* containerNode = dynamicDowncast<ContainerNode>(*this);
    return containerNode ? containerNode->countChildNodes() : 0;
}

inline Node* Node::traverseToChildAt(unsigned index) const
{
    auto* containerNode = dynamicDowncast<ContainerNode>(*this);
    return containerNode ? containerNode->traverseToChildAt(index) : nullptr;
}

inline Node* Node::firstChild() const
{
    auto* containerNode = dynamicDowncast<ContainerNode>(*this);
    return containerNode ? containerNode->firstChild() : nullptr;
}

inline Node* Node::lastChild() const
{
    auto* containerNode = dynamicDowncast<ContainerNode>(*this);
    return containerNode ? containerNode->lastChild() : nullptr;
}

inline Node& Node::rootNode() const
{
    if (isInTreeScope())
        return treeScope().rootNode();
    return traverseToRootNode();
}

inline void collectChildNodes(Node& node, NodeVector& children)
{
    for (Node* child = node.firstChild(); child; child = child->nextSibling())
        children.append(*child);
}

class ChildNodesLazySnapshot {
    WTF_MAKE_NONCOPYABLE(ChildNodesLazySnapshot);
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit ChildNodesLazySnapshot(Node& parentNode)
        : m_currentNode(parentNode.firstChild())
        , m_currentIndex(0)
        , m_hasSnapshot(false)
    {
        m_nextSnapshot = latestSnapshot;
        latestSnapshot = this;
    }

    ALWAYS_INLINE ~ChildNodesLazySnapshot()
    {
        latestSnapshot = m_nextSnapshot;
    }

    // Returns 0 if there is no next Node.
    RefPtr<Node> nextNode()
    {
        if (LIKELY(!hasSnapshot())) {
            RefPtr<Node> node = WTFMove(m_currentNode);
            if (node)
                m_currentNode = node->nextSibling();
            return node;
        }
        if (m_currentIndex >= m_snapshot.size())
            return nullptr;
        return m_snapshot[m_currentIndex++];
    }

    void takeSnapshot()
    {
        if (hasSnapshot())
            return;
        m_hasSnapshot = true;
        Node* node = m_currentNode.get();
        while (node) {
            m_snapshot.append(node);
            node = node->nextSibling();
        }
    }

    ChildNodesLazySnapshot* nextSnapshot() { return m_nextSnapshot; }
    bool hasSnapshot() { return m_hasSnapshot; }

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

    RefPtr<Node> m_currentNode;
    unsigned m_currentIndex;
    bool m_hasSnapshot;
    Vector<RefPtr<Node>> m_snapshot; // Lazily instantiated.
    ChildNodesLazySnapshot* m_nextSnapshot;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ContainerNode)
    static bool isType(const WebCore::Node& node) { return node.isContainerNode(); }
SPECIALIZE_TYPE_TRAITS_END()
