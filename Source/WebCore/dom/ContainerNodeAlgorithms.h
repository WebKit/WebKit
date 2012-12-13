/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *           (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef ContainerNodeAlgorithms_h
#define ContainerNodeAlgorithms_h

#include "Document.h"
#include "Frame.h"
#include "HTMLFrameOwnerElement.h"
#include "InspectorInstrumentation.h"
#include "NodeTraversal.h"
#include <wtf/Assertions.h>

namespace WebCore {

class ChildNodeInsertionNotifier {
public:
    explicit ChildNodeInsertionNotifier(ContainerNode* insertionPoint)
        : m_insertionPoint(insertionPoint)
    {
    }

    void notify(Node*);

private:
    void notifyDescendantInsertedIntoDocument(ContainerNode*);
    void notifyDescendantInsertedIntoTree(ContainerNode*);
    void notifyNodeInsertedIntoDocument(Node*);
    void notifyNodeInsertedIntoTree(ContainerNode*);

    ContainerNode* m_insertionPoint;
    Vector< RefPtr<Node> > m_postInsertionNotificationTargets;
};

class ChildNodeRemovalNotifier {
public:
    explicit ChildNodeRemovalNotifier(ContainerNode* insertionPoint)
        : m_insertionPoint(insertionPoint)
    {
    }

    void notify(Node*);

private:
    void notifyDescendantRemovedFromDocument(ContainerNode*);
    void notifyDescendantRemovedFromTree(ContainerNode*);
    void notifyNodeRemovedFromDocument(Node*);
    void notifyNodeRemovedFromTree(ContainerNode*);

    ContainerNode* m_insertionPoint;
};

namespace Private {

    template<class GenericNode, class GenericNodeContainer>
    void addChildNodesToDeletionQueue(GenericNode*& head, GenericNode*& tail, GenericNodeContainer*);

}

// Helper functions for TreeShared-derived classes, which have a 'Node' style interface
// This applies to 'ContainerNode' and 'SVGElementInstance'
template<class GenericNode, class GenericNodeContainer>
inline void removeAllChildrenInContainer(GenericNodeContainer* container)
{
    // List of nodes to be deleted.
    GenericNode* head = 0;
    GenericNode* tail = 0;

    Private::addChildNodesToDeletionQueue<GenericNode, GenericNodeContainer>(head, tail, container);

    GenericNode* n;
    GenericNode* next;
    while ((n = head) != 0) {
        ASSERT(n->m_deletionHasBegun);

        next = n->nextSibling();
        n->setNextSibling(0);

        head = next;
        if (next == 0)
            tail = 0;

        if (n->hasChildNodes())
            Private::addChildNodesToDeletionQueue<GenericNode, GenericNodeContainer>(head, tail, static_cast<GenericNodeContainer*>(n));

        delete n;
    }
}

template<class GenericNode, class GenericNodeContainer>
inline void appendChildToContainer(GenericNode* child, GenericNodeContainer* container)
{
    child->setParentOrHostNode(container);

    GenericNode* lastChild = container->lastChild();
    if (lastChild) {
        child->setPreviousSibling(lastChild);
        lastChild->setNextSibling(child);
    } else
        container->setFirstChild(child);

    container->setLastChild(child);
}

// Helper methods for removeAllChildrenInContainer, hidden from WebCore namespace
namespace Private {

    template<class GenericNode, class GenericNodeContainer, bool dispatchRemovalNotification>
    struct NodeRemovalDispatcher {
        static void dispatch(GenericNode*, GenericNodeContainer*)
        {
            // no-op, by default
        }
    };

    template<class GenericNode, class GenericNodeContainer>
    struct NodeRemovalDispatcher<GenericNode, GenericNodeContainer, true> {
        static void dispatch(GenericNode* node, GenericNodeContainer* container)
        {
            // Clean up any TreeScope to a removed tree.
            if (Document* containerDocument = container->ownerDocument())
                containerDocument->adoptIfNeeded(node);
            if (node->inDocument())
                ChildNodeRemovalNotifier(container).notify(node);
        }
    };

    template<class GenericNode>
    struct ShouldDispatchRemovalNotification {
        static const bool value = false;
    };

    template<>
    struct ShouldDispatchRemovalNotification<Node> {
        static const bool value = true;
    };

    template<class GenericNode, class GenericNodeContainer>
    void addChildNodesToDeletionQueue(GenericNode*& head, GenericNode*& tail, GenericNodeContainer* container)
    {
        // We have to tell all children that their parent has died.
        GenericNode* next = 0;
        for (GenericNode* n = container->firstChild(); n != 0; n = next) {
            ASSERT(!n->m_deletionHasBegun);

            next = n->nextSibling();
            n->setPreviousSibling(0);
            n->setNextSibling(0);
            n->setParentOrHostNode(0);

            if (!n->refCount()) {
#ifndef NDEBUG
                n->m_deletionHasBegun = true;
#endif
                // Add the node to the list of nodes to be deleted.
                // Reuse the nextSibling pointer for this purpose.
                if (tail)
                    tail->setNextSibling(n);
                else
                    head = n;

                tail = n;
            } else {
                RefPtr<GenericNode> protect(n); // removedFromDocument may remove remove all references to this node.
                NodeRemovalDispatcher<GenericNode, GenericNodeContainer, ShouldDispatchRemovalNotification<GenericNode>::value>::dispatch(n, container);
            }
        }

        container->setFirstChild(0);
        container->setLastChild(0);
    }

} // namespace Private

inline void ChildNodeInsertionNotifier::notifyNodeInsertedIntoDocument(Node* node)
{
    ASSERT(m_insertionPoint->inDocument());
    RefPtr<Node> protect(node);
    if (Node::InsertionShouldCallDidNotifySubtreeInsertions == node->insertedInto(m_insertionPoint))
        m_postInsertionNotificationTargets.append(node);
    if (node->isContainerNode())
        notifyDescendantInsertedIntoDocument(toContainerNode(node));
}

inline void ChildNodeInsertionNotifier::notifyNodeInsertedIntoTree(ContainerNode* node)
{
    NoEventDispatchAssertion assertNoEventDispatch;
    ASSERT(!m_insertionPoint->inDocument());

    if (Node::InsertionShouldCallDidNotifySubtreeInsertions == node->insertedInto(m_insertionPoint))
        m_postInsertionNotificationTargets.append(node);
    notifyDescendantInsertedIntoTree(node);
}

inline void ChildNodeInsertionNotifier::notify(Node* node)
{
    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());

#if ENABLE(INSPECTOR)
    InspectorInstrumentation::didInsertDOMNode(node->document(), node);
#endif

    RefPtr<Document> protectDocument(node->document());
    RefPtr<Node> protectNode(node);

    if (m_insertionPoint->inDocument())
        notifyNodeInsertedIntoDocument(node);
    else if (node->isContainerNode())
        notifyNodeInsertedIntoTree(toContainerNode(node));

    for (size_t i = 0; i < m_postInsertionNotificationTargets.size(); ++i)
        m_postInsertionNotificationTargets[i]->didNotifySubtreeInsertions(m_insertionPoint);
}


inline void ChildNodeRemovalNotifier::notifyNodeRemovedFromDocument(Node* node)
{
    ASSERT(m_insertionPoint->inDocument());
    node->removedFrom(m_insertionPoint);

    if (node->isContainerNode())
        notifyDescendantRemovedFromDocument(toContainerNode(node));
}

inline void ChildNodeRemovalNotifier::notifyNodeRemovedFromTree(ContainerNode* node)
{
    NoEventDispatchAssertion assertNoEventDispatch;
    ASSERT(!m_insertionPoint->inDocument());

    node->removedFrom(m_insertionPoint);
    notifyDescendantRemovedFromTree(node);
}

inline void ChildNodeRemovalNotifier::notify(Node* node)
{
    if (node->inDocument()) {
        notifyNodeRemovedFromDocument(node);
        node->document()->notifyRemovePendingSheetIfNeeded();
    } else if (node->isContainerNode())
        notifyNodeRemovedFromTree(toContainerNode(node));
}

class ChildFrameDisconnector {
public:
    enum ShouldIncludeRoot {
        DoNotIncludeRoot,
        IncludeRoot
    };

    explicit ChildFrameDisconnector(Node* root, ShouldIncludeRoot shouldIncludeRoot = IncludeRoot)
        : m_root(root)
    {
        // If we know there's no frames to disconnect then don't bother traversing
        // the tree looking for them.
        Frame* frame = root->document()->frame();
        if (frame && !frame->tree()->firstChild())
            return;
        collectDescendant(m_root, shouldIncludeRoot);
    }

    ~ChildFrameDisconnector()
    {
    }

    void disconnect();

    static bool nodeHasDisconnector(Node*);

private:
    void collectDescendant(Node* root, ShouldIncludeRoot);
    void collectDescendant(ElementShadow*);

    class Target {
    public:
        Target(HTMLFrameOwnerElement* element)
            : m_owner(element)
            , m_ownerParent(element->parentNode())
        {
        }

        bool isValid() const { return m_owner->parentNode() == m_ownerParent; }
        void disconnect();

    private:
        RefPtr<HTMLFrameOwnerElement> m_owner;
        ContainerNode* m_ownerParent;
    };

    Vector<Target, 10> m_list;
    Node* m_root;
};

inline void ChildFrameDisconnector::collectDescendant(Node* root, ShouldIncludeRoot shouldIncludeRoot)
{
    Element* element = (shouldIncludeRoot == IncludeRoot && root->isElementNode()) ? toElement(root) : ElementTraversal::firstWithin(root);
    for (; element; element = ElementTraversal::next(element, root)) {
        if (element->hasCustomCallbacks() && element->isFrameOwnerElement())
            m_list.append(toFrameOwnerElement(element));
        if (ElementShadow* shadow = element->shadow())
            collectDescendant(shadow);
    }
}

inline void ChildFrameDisconnector::disconnect()
{
    // Must disable frame loading in the subtree so an unload handler cannot
    // insert more frames and create loaded frames in detached subtrees.
    SubframeLoadingDisabler disabler(m_root);
    unsigned size = m_list.size();
    for (unsigned i = 0; i < size; ++i) {
        Target& target = m_list[i];
        if (target.isValid())
            target.disconnect();
    }
}

} // namespace WebCore

#endif // ContainerNodeAlgorithms_h
