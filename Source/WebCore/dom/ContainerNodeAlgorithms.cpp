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

#include "Element.h"
#include "ElementShadow.h"

namespace WebCore {

void ChildNodeInsertionNotifier::notifyDescendantInsertedIntoDocument(ContainerNode* node)
{
    NodeVector children;
    getChildNodes(node, children);
    for (size_t i = 0; i < children.size(); ++i) {
        // If we have been removed from the document during this loop, then
        // we don't want to tell the rest of our children that they've been
        // inserted into the document because they haven't.
        if (node->inDocument() && children[i]->parentNode() == node)
            notifyNodeInsertedIntoDocument(children[i].get());
    }
        
    if (!node->isElementNode())
        return;

    if (ElementShadow* shadow = toElement(node)->shadow()) {
        ShadowRootVector roots(shadow);
        for (size_t i = 0; i < roots.size(); ++i)
            notifyNodeInsertedIntoDocument(roots[i].get());
    }
}

void ChildNodeInsertionNotifier::notifyDescendantInsertedIntoTree(ContainerNode* node)
{
    for (Node* child = node->firstChild(); child; child = child->nextSibling()) {
        if (child->isContainerNode())
            notifyNodeInsertedIntoTree(toContainerNode(child));
    }

    if (!node->isElementNode())
        return;

    if (ElementShadow* shadow = toElement(node)->shadow()) {
        for (ShadowRoot* root = shadow->youngestShadowRoot(); root; root = root->olderShadowRoot())
            notifyNodeInsertedIntoTree(root);
    }
}

void ChildNodeRemovalNotifier::notifyDescendantRemovedFromDocument(ContainerNode* node)
{
    NodeVector children;
    getChildNodes(node, children);
    for (size_t i = 0; i < children.size(); ++i) {
        // If we have been added to the document during this loop, then we
        // don't want to tell the rest of our children that they've been
        // removed from the document because they haven't.
        if (!node->inDocument() && children[i]->parentNode() == node)
            notifyNodeRemovedFromDocument(children[i].get());
    }

    if (!node->isElementNode())
        return;

    if (node->document()->cssTarget() == node)
        node->document()->setCSSTarget(0);

    if (ElementShadow* shadow = toElement(node)->shadow()) {
        ShadowRootVector roots(shadow);
        for (size_t i = 0; i < roots.size(); ++i)
            notifyNodeRemovedFromDocument(roots[i].get());
    }
}

void ChildNodeRemovalNotifier::notifyDescendantRemovedFromTree(ContainerNode* node)
{
    for (Node* child = node->firstChild(); child; child = child->nextSibling()) {
        if (child->isContainerNode())
            notifyNodeRemovedFromTree(toContainerNode(child));
    }

    if (!node->isElementNode())
        return;

    if (ElementShadow* shadow = toElement(node)->shadow()) {
        ShadowRootVector roots(shadow);
        for (size_t i = 0; i < roots.size(); ++i)
            notifyNodeRemovedFromTree(roots[i].get());
    }
}

}
