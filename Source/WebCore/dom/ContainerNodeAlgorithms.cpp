/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 */

#include "config.h"
#include "ContainerNodeAlgorithms.h"

#include "NoEventDispatchAssertion.h"

namespace WebCore {

void notifyNodeInsertedIntoTree(ContainerNode& insertionPoint, ContainerNode&, NodeVector& postInsertionNotificationTargets);
void notifyNodeInsertedIntoDocument(ContainerNode& insertionPoint, Node&, NodeVector& postInsertionNotificationTargets);
void notifyNodeRemovedFromTree(ContainerNode& insertionPoint, ContainerNode&);
void notifyNodeRemovedFromDocument(ContainerNode& insertionPoint, Node&);

static void notifyDescendantInsertedIntoDocument(ContainerNode& insertionPoint, ContainerNode& node, NodeVector& postInsertionNotificationTargets)
{
    ChildNodesLazySnapshot snapshot(node);
    while (RefPtr<Node> child = snapshot.nextNode()) {
        // If we have been removed from the document during this loop, then
        // we don't want to tell the rest of our children that they've been
        // inserted into the document because they haven't.
        if (node.inDocument() && child->parentNode() == &node)
            notifyNodeInsertedIntoDocument(insertionPoint, *child, postInsertionNotificationTargets);
    }

    if (!is<Element>(node))
        return;

    if (RefPtr<ShadowRoot> root = downcast<Element>(node).shadowRoot()) {
        if (node.inDocument() && root->host() == &node)
            notifyNodeInsertedIntoDocument(insertionPoint, *root, postInsertionNotificationTargets);
    }
}

static void notifyDescendantInsertedIntoTree(ContainerNode& insertionPoint, ContainerNode& node, NodeVector& postInsertionNotificationTargets)
{
    for (Node* child = node.firstChild(); child; child = child->nextSibling()) {
        if (is<ContainerNode>(*child))
            notifyNodeInsertedIntoTree(insertionPoint, downcast<ContainerNode>(*child), postInsertionNotificationTargets);
    }

    if (ShadowRoot* root = node.shadowRoot())
        notifyNodeInsertedIntoTree(insertionPoint, *root, postInsertionNotificationTargets);
}

void notifyNodeInsertedIntoDocument(ContainerNode& insertionPoint, Node& node, NodeVector& postInsertionNotificationTargets)
{
    ASSERT(insertionPoint.inDocument());
    if (node.insertedInto(insertionPoint) == Node::InsertionShouldCallFinishedInsertingSubtree)
        postInsertionNotificationTargets.append(node);
    if (is<ContainerNode>(node))
        notifyDescendantInsertedIntoDocument(insertionPoint, downcast<ContainerNode>(node), postInsertionNotificationTargets);
}

void notifyNodeInsertedIntoTree(ContainerNode& insertionPoint, ContainerNode& node, NodeVector& postInsertionNotificationTargets)
{
    NoEventDispatchAssertion assertNoEventDispatch;
    ASSERT(!insertionPoint.inDocument());

    if (node.insertedInto(insertionPoint) == Node::InsertionShouldCallFinishedInsertingSubtree)
        postInsertionNotificationTargets.append(node);
    notifyDescendantInsertedIntoTree(insertionPoint, node, postInsertionNotificationTargets);
}

void notifyChildNodeInserted(ContainerNode& insertionPoint, Node& node, NodeVector& postInsertionNotificationTargets)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!NoEventDispatchAssertion::isEventDispatchForbidden());

    InspectorInstrumentation::didInsertDOMNode(node.document(), node);

    Ref<Document> protectDocument(node.document());
    Ref<Node> protectNode(node);

    if (insertionPoint.inDocument())
        notifyNodeInsertedIntoDocument(insertionPoint, node, postInsertionNotificationTargets);
    else if (is<ContainerNode>(node))
        notifyNodeInsertedIntoTree(insertionPoint, downcast<ContainerNode>(node), postInsertionNotificationTargets);
}

void notifyNodeRemovedFromDocument(ContainerNode& insertionPoint, Node& node)
{
    ASSERT(insertionPoint.inDocument());
    node.removedFrom(insertionPoint);

    if (!is<ContainerNode>(node))
        return;
    ChildNodesLazySnapshot snapshot(node);
    while (RefPtr<Node> child = snapshot.nextNode()) {
        // If we have been added to the document during this loop, then we
        // don't want to tell the rest of our children that they've been
        // removed from the document because they haven't.
        if (!node.inDocument() && child->parentNode() == &node)
            notifyNodeRemovedFromDocument(insertionPoint, *child.get());
    }

    if (!is<Element>(node))
        return;

    if (node.document().cssTarget() == &node)
        node.document().setCSSTarget(nullptr);

    if (RefPtr<ShadowRoot> root = downcast<Element>(node).shadowRoot()) {
        if (!node.inDocument() && root->host() == &node)
            notifyNodeRemovedFromDocument(insertionPoint, *root.get());
    }
}

void notifyNodeRemovedFromTree(ContainerNode& insertionPoint, ContainerNode& node)
{
    NoEventDispatchAssertion assertNoEventDispatch;
    ASSERT(!insertionPoint.inDocument());

    node.removedFrom(insertionPoint);

    for (Node* child = node.firstChild(); child; child = child->nextSibling()) {
        if (is<ContainerNode>(*child))
            notifyNodeRemovedFromTree(insertionPoint, downcast<ContainerNode>(*child));
    }

    if (!is<Element>(node))
        return;

    if (RefPtr<ShadowRoot> root = downcast<Element>(node).shadowRoot())
        notifyNodeRemovedFromTree(insertionPoint, *root.get());
}

void notifyChildNodeRemoved(ContainerNode& insertionPoint, Node& child)
{
    if (!child.inDocument()) {
        if (is<ContainerNode>(child))
            notifyNodeRemovedFromTree(insertionPoint, downcast<ContainerNode>(child));
        return;
    }
    notifyNodeRemovedFromDocument(insertionPoint, child);
    child.document().notifyRemovePendingSheetIfNeeded();
}

void addChildNodesToDeletionQueue(Node*& head, Node*& tail, ContainerNode& container)
{
    // We have to tell all children that their parent has died.
    Node* next = nullptr;
    for (auto* node = container.firstChild(); node; node = next) {
        ASSERT(!node->m_deletionHasBegun);

        next = node->nextSibling();
        node->setNextSibling(nullptr);
        node->setParentNode(nullptr);
        container.setFirstChild(next);
        if (next)
            next->setPreviousSibling(nullptr);

        if (!node->refCount()) {
#ifndef NDEBUG
            node->m_deletionHasBegun = true;
#endif
            // Add the node to the list of nodes to be deleted.
            // Reuse the nextSibling pointer for this purpose.
            if (tail)
                tail->setNextSibling(node);
            else
                head = node;

            tail = node;
        } else {
            Ref<Node> protect(*node); // removedFromDocument may remove remove all references to this node.
            if (Document* containerDocument = container.ownerDocument())
                containerDocument->adoptIfNeeded(node);
            if (node->inDocument())
                notifyChildNodeRemoved(container, *node);
        }
    }

    container.setLastChild(nullptr);
}

void removeDetachedChildrenInContainer(ContainerNode& container)
{
    // List of nodes to be deleted.
    Node* head = nullptr;
    Node* tail = nullptr;

    addChildNodesToDeletionQueue(head, tail, container);

    Node* node;
    Node* next;
    while ((node = head)) {
        ASSERT(node->m_deletionHasBegun);

        next = node->nextSibling();
        node->setNextSibling(nullptr);

        head = next;
        if (!next)
            tail = nullptr;

        if (is<ContainerNode>(node))
            addChildNodesToDeletionQueue(head, tail, downcast<ContainerNode>(*node));
        
        delete node;
    }
}

#ifndef NDEBUG
static unsigned assertConnectedSubrameCountIsConsistent(ContainerNode& node)
{
    unsigned count = 0;

    if (is<Element>(node)) {
        if (is<HTMLFrameOwnerElement>(node) && downcast<HTMLFrameOwnerElement>(node).contentFrame())
            ++count;

        if (ShadowRoot* root = downcast<Element>(node).shadowRoot())
            count += assertConnectedSubrameCountIsConsistent(*root);
    }

    for (auto& child : childrenOfType<Element>(node))
        count += assertConnectedSubrameCountIsConsistent(child);

    // If we undercount there's possibly a security bug since we'd leave frames
    // in subtrees outside the document.
    ASSERT(node.connectedSubframeCount() >= count);

    // If we overcount it's safe, but not optimal because it means we'll traverse
    // through the document in disconnectSubframes looking for frames that have
    // already been disconnected.
    ASSERT(node.connectedSubframeCount() == count);

    return count;
}
#endif

static void collectFrameOwners(Vector<Ref<HTMLFrameOwnerElement>>& frameOwners, ContainerNode& root)
{
    auto elementDescendants = descendantsOfType<Element>(root);
    auto it = elementDescendants.begin();
    auto end = elementDescendants.end();
    while (it != end) {
        Element& element = *it;
        if (!element.connectedSubframeCount()) {
            it.traverseNextSkippingChildren();
            continue;
        }

        if (is<HTMLFrameOwnerElement>(element))
            frameOwners.append(downcast<HTMLFrameOwnerElement>(element));

        if (ShadowRoot* shadowRoot = element.shadowRoot())
            collectFrameOwners(frameOwners, *shadowRoot);
        ++it;
    }
}

void disconnectSubframes(ContainerNode& root, SubframeDisconnectPolicy policy)
{
#ifndef NDEBUG
    assertConnectedSubrameCountIsConsistent(root);
#endif
    ASSERT(root.connectedSubframeCount());

    Vector<Ref<HTMLFrameOwnerElement>> frameOwners;

    if (policy == RootAndDescendants) {
        if (is<HTMLFrameOwnerElement>(root))
            frameOwners.append(downcast<HTMLFrameOwnerElement>(root));
    }

    collectFrameOwners(frameOwners, root);

    // Must disable frame loading in the subtree so an unload handler cannot
    // insert more frames and create loaded frames in detached subtrees.
    SubframeLoadingDisabler disabler(root);

    bool isFirst = true;
    for (auto& owner : frameOwners) {
        // Don't need to traverse up the tree for the first owner since no
        // script could have moved it.
        if (isFirst || root.containsIncludingShadowDOM(&owner.get()))
            owner.get().disconnectContentFrame();
        isFirst = false;
    }
}

}
