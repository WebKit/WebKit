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
#include "InspectorInstrumentation.h"
#include <wtf/Assertions.h>

namespace WebCore {

class Node;

class ChildNodeInsertionNotifier {
public:
    explicit ChildNodeInsertionNotifier(Node* insertionPoint)
        : m_insertionPoint(insertionPoint)
    { }

    void notifyInsertedIntoDocument(Node*);
    void notify(Node*);

private:
    void notifyDescendantInsertedIntoDocument(ContainerNode*);
    void notifyDescendantInsertedIntoTree(ContainerNode*);
    void notifyNodeInsertedIntoDocument(Node*);
    void notifyNodeInsertedIntoTree(ContainerNode*);

    Node* m_insertionPoint;
};

class ChildNodeRemovalNotifier {
public:
    explicit ChildNodeRemovalNotifier(Node* insertionPoint)
        : m_insertionPoint(insertionPoint)
    { }

    void notify(Node*);

private:
    void notifyDescendantRemovedFromDocument(ContainerNode*);
    void notifyDescendantRemovedFromTree(ContainerNode*);
    void notifyNodeRemovedFromDocument(Node*);
    void notifyNodeRemovedFromTree(ContainerNode*);

    Node* m_insertionPoint;
};

namespace Private {

    template<class GenericNode, class GenericNodeContainer>
    void addChildNodesToDeletionQueue(GenericNode*& head, GenericNode*& tail, GenericNodeContainer* container);

};

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
    child->setParent(container);

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
            n->setParent(0);

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
    Node::InsertionNotificationRequest request = node->insertedInto(m_insertionPoint);

    if (node->isContainerNode())
        notifyDescendantInsertedIntoDocument(toContainerNode(node));

    if (request == Node::InsertionShouldCallDidNotifyDescendantInseretions)
        node->didNotifyDescendantInseretions(m_insertionPoint);
}

inline void ChildNodeInsertionNotifier::notifyNodeInsertedIntoTree(ContainerNode* node)
{
    ASSERT(!m_insertionPoint->inDocument());
    forbidEventDispatch();

    Node::InsertionNotificationRequest request = node->insertedInto(m_insertionPoint);

    notifyDescendantInsertedIntoTree(node);
    if (request == Node::InsertionShouldCallDidNotifyDescendantInseretions)
        node->didNotifyDescendantInseretions(m_insertionPoint);

    allowEventDispatch();
}

inline void ChildNodeInsertionNotifier::notifyInsertedIntoDocument(Node* node)
{
    notifyNodeInsertedIntoDocument(node);
}

inline void ChildNodeInsertionNotifier::notify(Node* node)
{
    ASSERT(!eventDispatchForbidden());

#if ENABLE(INSPECTOR)
    InspectorInstrumentation::didInsertDOMNode(node->document(), node);
#endif

    RefPtr<Document> protectDocument(node->document());
    RefPtr<Node> protectNode(node);

    if (m_insertionPoint->inDocument())
        notifyNodeInsertedIntoDocument(node);
    else if (node->isContainerNode())
        notifyNodeInsertedIntoTree(toContainerNode(node));
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
    ASSERT(!m_insertionPoint->inDocument());
    forbidEventDispatch();

    node->removedFrom(m_insertionPoint);
    notifyDescendantRemovedFromTree(node);

    allowEventDispatch();
}

inline void ChildNodeRemovalNotifier::notify(Node* node)
{
    if (node->inDocument())
        notifyNodeRemovedFromDocument(node);
    else if (node->isContainerNode())
        notifyNodeRemovedFromTree(toContainerNode(node));
}

} // namespace WebCore

#endif // ContainerNodeAlgorithms_h
