/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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


namespace WebCore {

void ChildNodeInsertionNotifier::notifyDescendantInsertedIntoDocument(ContainerNode& node, NodeVector& postInsertionNotificationTargets)
{
    ChildNodesLazySnapshot snapshot(node);
    while (RefPtr<Node> child = snapshot.nextNode()) {
        // If we have been removed from the document during this loop, then
        // we don't want to tell the rest of our children that they've been
        // inserted into the document because they haven't.
        if (node.inDocument() && child->parentNode() == &node)
            notifyNodeInsertedIntoDocument(*child, postInsertionNotificationTargets);
    }

    if (!is<Element>(node))
        return;

    if (RefPtr<ShadowRoot> root = downcast<Element>(node).shadowRoot()) {
        if (node.inDocument() && root->hostElement() == &node)
            notifyNodeInsertedIntoDocument(*root, postInsertionNotificationTargets);
    }
}

void ChildNodeInsertionNotifier::notifyDescendantInsertedIntoTree(ContainerNode& node, NodeVector& postInsertionNotificationTargets)
{
    for (Node* child = node.firstChild(); child; child = child->nextSibling()) {
        if (is<ContainerNode>(*child))
            notifyNodeInsertedIntoTree(downcast<ContainerNode>(*child), postInsertionNotificationTargets);
    }

    if (ShadowRoot* root = node.shadowRoot())
        notifyNodeInsertedIntoTree(*root, postInsertionNotificationTargets);
}

void ChildNodeRemovalNotifier::notifyDescendantRemovedFromDocument(ContainerNode& node)
{
    ChildNodesLazySnapshot snapshot(node);
    while (RefPtr<Node> child = snapshot.nextNode()) {
        // If we have been added to the document during this loop, then we
        // don't want to tell the rest of our children that they've been
        // removed from the document because they haven't.
        if (!node.inDocument() && child->parentNode() == &node)
            notifyNodeRemovedFromDocument(*child.get());
    }

    if (!is<Element>(node))
        return;

    if (node.document().cssTarget() == &node)
        node.document().setCSSTarget(0);

    if (RefPtr<ShadowRoot> root = downcast<Element>(node).shadowRoot()) {
        if (!node.inDocument() && root->hostElement() == &node)
            notifyNodeRemovedFromDocument(*root.get());
    }
}

void ChildNodeRemovalNotifier::notifyDescendantRemovedFromTree(ContainerNode& node)
{
    for (Node* child = node.firstChild(); child; child = child->nextSibling()) {
        if (is<ContainerNode>(*child))
            notifyNodeRemovedFromTree(downcast<ContainerNode>(*child));
    }

    if (!is<Element>(node))
        return;

    if (RefPtr<ShadowRoot> root = downcast<Element>(node).shadowRoot())
        notifyNodeRemovedFromTree(*root.get());
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
