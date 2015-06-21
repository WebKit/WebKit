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
#include "ElementIterator.h"
#include "Frame.h"
#include "HTMLFrameOwnerElement.h"
#include "InspectorInstrumentation.h"
#include "NodeTraversal.h"
#include "ShadowRoot.h"
#include <wtf/Assertions.h>
#include <wtf/Ref.h>

namespace WebCore {

class ChildNodeInsertionNotifier {
public:
    explicit ChildNodeInsertionNotifier(ContainerNode& insertionPoint)
        : m_insertionPoint(insertionPoint)
    {
    }

    void notify(Node&, NodeVector& postInsertionNotificationTargets);

private:
    void notifyDescendantInsertedIntoDocument(ContainerNode&, NodeVector& postInsertionNotificationTargets);
    void notifyDescendantInsertedIntoTree(ContainerNode&, NodeVector& postInsertionNotificationTargets);
    void notifyNodeInsertedIntoDocument(Node&, NodeVector& postInsertionNotificationTargets);
    void notifyNodeInsertedIntoTree(ContainerNode&, NodeVector& postInsertionNotificationTargets);

    ContainerNode& m_insertionPoint;
};

class ChildNodeRemovalNotifier {
public:
    explicit ChildNodeRemovalNotifier(ContainerNode& removalPoint)
        : m_removalPoint(removalPoint)
    {
    }

    void notify(Node&);

private:
    void notifyDescendantRemovedFromDocument(ContainerNode&);
    void notifyDescendantRemovedFromTree(ContainerNode&);
    void notifyNodeRemovedFromDocument(Node&);
    void notifyNodeRemovedFromTree(ContainerNode&);

    ContainerNode& m_removalPoint;
};

namespace Private {

    template<class GenericNode, class GenericNodeContainer>
    void addChildNodesToDeletionQueue(GenericNode*& head, GenericNode*& tail, GenericNodeContainer&);

}

// This no longer needs to be generic. It's only used for Node and ContainerNode now.
template<class GenericNode, class GenericNodeContainer>
inline void removeDetachedChildrenInContainer(GenericNodeContainer& container)
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
            Private::addChildNodesToDeletionQueue<GenericNode, GenericNodeContainer>(head, tail, *static_cast<GenericNodeContainer*>(n));

        delete n;
    }
}

template<class GenericNode, class GenericNodeContainer>
inline void appendChildToContainer(GenericNode* child, GenericNodeContainer& container)
{
    child->setParentNode(&container);

    GenericNode* lastChild = container.lastChild();
    if (lastChild) {
        child->setPreviousSibling(lastChild);
        lastChild->setNextSibling(child);
    } else
        container.setFirstChild(child);

    container.setLastChild(child);
}

// Helper methods for removeDetachedChildrenInContainer, hidden from WebCore namespace
namespace Private {

    template<class GenericNode, class GenericNodeContainer, bool dispatchRemovalNotification>
    struct NodeRemovalDispatcher {
        static void dispatch(GenericNode&, GenericNodeContainer&)
        {
            // no-op, by default
        }
    };

    template<class GenericNode, class GenericNodeContainer>
    struct NodeRemovalDispatcher<GenericNode, GenericNodeContainer, true> {
        static void dispatch(GenericNode& node, GenericNodeContainer& container)
        {
            // Clean up any TreeScope to a removed tree.
            if (Document* containerDocument = container.ownerDocument())
                containerDocument->adoptIfNeeded(&node);
            if (node.inDocument())
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
    void addChildNodesToDeletionQueue(GenericNode*& head, GenericNode*& tail, GenericNodeContainer& container)
    {
        // We have to tell all children that their parent has died.
        GenericNode* next = 0;
        for (GenericNode* n = container.firstChild(); n != 0; n = next) {
            ASSERT(!n->m_deletionHasBegun);

            next = n->nextSibling();
            n->setNextSibling(0);
            n->setParentNode(0);
            container.setFirstChild(next);
            if (next)
                next->setPreviousSibling(0);

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
                Ref<GenericNode> protect(*n); // removedFromDocument may remove remove all references to this node.
                NodeRemovalDispatcher<GenericNode, GenericNodeContainer, ShouldDispatchRemovalNotification<GenericNode>::value>::dispatch(*n, container);
            }
        }

        container.setLastChild(0);
    }

} // namespace Private

inline void ChildNodeInsertionNotifier::notifyNodeInsertedIntoDocument(Node& node, NodeVector& postInsertionNotificationTargets)
{
    ASSERT(m_insertionPoint.inDocument());
    if (Node::InsertionShouldCallFinishedInsertingSubtree == node.insertedInto(m_insertionPoint))
        postInsertionNotificationTargets.append(node);
    if (is<ContainerNode>(node))
        notifyDescendantInsertedIntoDocument(downcast<ContainerNode>(node), postInsertionNotificationTargets);
}

inline void ChildNodeInsertionNotifier::notifyNodeInsertedIntoTree(ContainerNode& node, NodeVector& postInsertionNotificationTargets)
{
    NoEventDispatchAssertion assertNoEventDispatch;
    ASSERT(!m_insertionPoint.inDocument());

    if (Node::InsertionShouldCallFinishedInsertingSubtree == node.insertedInto(m_insertionPoint))
        postInsertionNotificationTargets.append(node);
    notifyDescendantInsertedIntoTree(node, postInsertionNotificationTargets);
}

inline void ChildNodeInsertionNotifier::notify(Node& node, NodeVector& postInsertionNotificationTargets)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!NoEventDispatchAssertion::isEventDispatchForbidden());

    InspectorInstrumentation::didInsertDOMNode(node.document(), node);

    Ref<Document> protectDocument(node.document());
    Ref<Node> protectNode(node);

    if (m_insertionPoint.inDocument())
        notifyNodeInsertedIntoDocument(node, postInsertionNotificationTargets);
    else if (is<ContainerNode>(node))
        notifyNodeInsertedIntoTree(downcast<ContainerNode>(node), postInsertionNotificationTargets);
}


inline void ChildNodeRemovalNotifier::notifyNodeRemovedFromDocument(Node& node)
{
    ASSERT(m_removalPoint.inDocument());
    node.removedFrom(m_removalPoint);

    if (is<ContainerNode>(node))
        notifyDescendantRemovedFromDocument(downcast<ContainerNode>(node));
}

inline void ChildNodeRemovalNotifier::notifyNodeRemovedFromTree(ContainerNode& node)
{
    NoEventDispatchAssertion assertNoEventDispatch;
    ASSERT(!m_removalPoint.inDocument());

    node.removedFrom(m_removalPoint);
    notifyDescendantRemovedFromTree(node);
}

inline void ChildNodeRemovalNotifier::notify(Node& node)
{
    if (node.inDocument()) {
        notifyNodeRemovedFromDocument(node);
        node.document().notifyRemovePendingSheetIfNeeded();
    } else if (is<ContainerNode>(node))
        notifyNodeRemovedFromTree(downcast<ContainerNode>(node));
}

enum SubframeDisconnectPolicy {
    RootAndDescendants,
    DescendantsOnly
};
void disconnectSubframes(ContainerNode& root, SubframeDisconnectPolicy);

inline void disconnectSubframesIfNeeded(ContainerNode& root, SubframeDisconnectPolicy policy)
{
    if (!root.connectedSubframeCount())
        return;
    disconnectSubframes(root, policy);
}

} // namespace WebCore

#endif // ContainerNodeAlgorithms_h
