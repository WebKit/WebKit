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

#include "HTMLFrameOwnerElement.h"
#include "HTMLTextAreaElement.h"
#include "InspectorInstrumentation.h"
#include "ScriptDisallowedScope.h"
#include "ShadowRoot.h"
#include "TypedElementDescendantIterator.h"

namespace WebCore {

#if ASSERT_ENABLED
ContainerChildRemovalScope* ContainerChildRemovalScope::s_scope = nullptr;
#endif

enum class TreeScopeChange { Changed, DidNotChange };

static void notifyNodeInsertedIntoDocument(ContainerNode& parentOfInsertedTree, Node& node, TreeScopeChange treeScopeChange, NodeVector& postInsertionNotificationTargets)
{
    ASSERT(parentOfInsertedTree.isConnected());
    ASSERT(!node.isConnected());
    if (node.insertedIntoAncestor(Node::InsertionType { /* connectedToDocument */ true, treeScopeChange == TreeScopeChange::Changed }, parentOfInsertedTree) == Node::InsertedIntoAncestorResult::NeedsPostInsertionCallback)
        postInsertionNotificationTargets.append(node);

    if (!is<ContainerNode>(node))
        return;

    for (RefPtr<Node> child = downcast<ContainerNode>(node).firstChild(); child; child = child->nextSibling()) {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(node.isConnected() && child->parentNode() == &node);
        notifyNodeInsertedIntoDocument(parentOfInsertedTree, *child, treeScopeChange, postInsertionNotificationTargets);
    }

    if (!is<Element>(node))
        return;

    if (RefPtr<ShadowRoot> root = downcast<Element>(node).shadowRoot()) {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(node.isConnected() && root->host() == &node);
        notifyNodeInsertedIntoDocument(parentOfInsertedTree, *root, TreeScopeChange::DidNotChange, postInsertionNotificationTargets);
    }
}

static void notifyNodeInsertedIntoTree(ContainerNode& parentOfInsertedTree, Node& node, TreeScopeChange treeScopeChange, NodeVector& postInsertionNotificationTargets)
{
    ASSERT(!parentOfInsertedTree.isConnected());
    ASSERT(!node.isConnected());

    if (node.insertedIntoAncestor(Node::InsertionType { /* connectedToDocument */ false, treeScopeChange == TreeScopeChange::Changed }, parentOfInsertedTree) == Node::InsertedIntoAncestorResult::NeedsPostInsertionCallback)
        postInsertionNotificationTargets.append(node);

    if (!is<ContainerNode>(node))
        return;

    for (RefPtr<Node> child = downcast<ContainerNode>(node).firstChild(); child; child = child->nextSibling())
        notifyNodeInsertedIntoTree(parentOfInsertedTree, *child, treeScopeChange, postInsertionNotificationTargets);

    if (!is<Element>(node))
        return;

    if (RefPtr<ShadowRoot> root = downcast<Element>(node).shadowRoot())
        notifyNodeInsertedIntoTree(parentOfInsertedTree, *root, TreeScopeChange::DidNotChange, postInsertionNotificationTargets);
}

NodeVector notifyChildNodeInserted(ContainerNode& parentOfInsertedTree, Node& node)
{
    ASSERT(ScriptDisallowedScope::InMainThread::hasDisallowedScope());

    InspectorInstrumentation::didInsertDOMNode(node.document(), node);

    Ref<Document> protectDocument(node.document());
    Ref<Node> protectNode(node);

    NodeVector postInsertionNotificationTargets;

    // Tree scope has changed if the container node into which "node" is inserted is in a document or a shadow root.
    auto treeScopeChange = parentOfInsertedTree.isInTreeScope() ? TreeScopeChange::Changed : TreeScopeChange::DidNotChange;
    if (parentOfInsertedTree.isConnected())
        notifyNodeInsertedIntoDocument(parentOfInsertedTree, node, treeScopeChange, postInsertionNotificationTargets);
    else
        notifyNodeInsertedIntoTree(parentOfInsertedTree, node, treeScopeChange, postInsertionNotificationTargets);

    return postInsertionNotificationTargets;
}

static void notifyNodeRemovedFromDocument(ContainerNode& oldParentOfRemovedTree, TreeScopeChange treeScopeChange, Node& node)
{
    ASSERT(oldParentOfRemovedTree.isConnected());
    ASSERT(node.isConnected());
    node.removedFromAncestor(Node::RemovalType { /* disconnectedFromDocument */ true, treeScopeChange == TreeScopeChange::Changed }, oldParentOfRemovedTree);

    if (!is<ContainerNode>(node))
        return;

    for (RefPtr<Node> child = downcast<ContainerNode>(node).firstChild(); child; child = child->nextSibling()) {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!node.isConnected() && child->parentNode() == &node);
        notifyNodeRemovedFromDocument(oldParentOfRemovedTree, treeScopeChange, *child.get());
    }

    if (!is<Element>(node))
        return;

    if (RefPtr<ShadowRoot> root = downcast<Element>(node).shadowRoot()) {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!node.isConnected() && root->host() == &node);
        notifyNodeRemovedFromDocument(oldParentOfRemovedTree, TreeScopeChange::DidNotChange, *root.get());
    }
}

static void notifyNodeRemovedFromTree(ContainerNode& oldParentOfRemovedTree, TreeScopeChange treeScopeChange, Node& node)
{
    ASSERT(!oldParentOfRemovedTree.isConnected());

    node.removedFromAncestor(Node::RemovalType { /* disconnectedFromDocument */ false, treeScopeChange == TreeScopeChange::Changed }, oldParentOfRemovedTree);

    if (!is<ContainerNode>(node))
        return;

    for (RefPtr<Node> child = downcast<ContainerNode>(node).firstChild(); child; child = child->nextSibling())
        notifyNodeRemovedFromTree(oldParentOfRemovedTree, treeScopeChange, *child);

    if (!is<Element>(node))
        return;

    if (RefPtr<ShadowRoot> root = downcast<Element>(node).shadowRoot())
        notifyNodeRemovedFromTree(oldParentOfRemovedTree, TreeScopeChange::DidNotChange, *root);
}

void notifyChildNodeRemoved(ContainerNode& oldParentOfRemovedTree, Node& child)
{
    // Assert that the caller of this function has an instance of ScriptDisallowedScope.
    ASSERT(!isMainThread() || ScriptDisallowedScope::InMainThread::hasDisallowedScope());
    ContainerChildRemovalScope removalScope(oldParentOfRemovedTree, child);

    // Tree scope has changed if the container node from which "node" is removed is in a document or a shadow root.
    auto treeScopeChange = oldParentOfRemovedTree.isInTreeScope() ? TreeScopeChange::Changed : TreeScopeChange::DidNotChange;
    if (child.isConnected())
        notifyNodeRemovedFromDocument(oldParentOfRemovedTree, treeScopeChange, child);
    else
        notifyNodeRemovedFromTree(oldParentOfRemovedTree, treeScopeChange, child);
}

void addChildNodesToDeletionQueue(Node*& head, Node*& tail, ContainerNode& container)
{
    // We have to tell all children that their parent has died.
    RefPtr<Node> next = nullptr;
    for (RefPtr<Node> node = container.firstChild(); node; node = next) {
        ASSERT(!node->m_deletionHasBegun);

        next = node->nextSibling();
        node->setNextSibling(nullptr);
        node->setParentNode(nullptr);
        container.setFirstChild(next.get());
        if (next)
            next->setPreviousSibling(nullptr);

        if (!node->refCount()) {
#ifndef NDEBUG
            node->m_deletionHasBegun = true;
#endif
            // Add the node to the list of nodes to be deleted.
            // Reuse the nextSibling pointer for this purpose.
            if (tail)
                tail->setNextSibling(node.get());
            else
                head = node.get();

            tail = node.get();
        } else {
            node->setTreeScopeRecursively(container.document());
            if (node->isInTreeScope())
                notifyChildNodeRemoved(container, *node);
            ASSERT_WITH_SECURITY_IMPLICATION(!node->isInTreeScope());
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

        if (is<ContainerNode>(*node))
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

    if (auto* shadowRoot = root.shadowRoot())
        collectFrameOwners(frameOwners, *shadowRoot);

    // Must disable frame loading in the subtree so an unload handler cannot
    // insert more frames and create loaded frames in detached subtrees.
    SubframeLoadingDisabler disabler(&root);

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
