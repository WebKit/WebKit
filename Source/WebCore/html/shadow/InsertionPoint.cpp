/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InsertionPoint.h"

#include "ShadowRoot.h"
#include "ShadowTree.h"

namespace WebCore {

InsertionPoint::InsertionPoint(const QualifiedName& tagName, Document* document)
    : HTMLElement(tagName, document)
    , m_selections()
{
}

InsertionPoint::~InsertionPoint()
{
}

void InsertionPoint::attach()
{
    if (isShadowBoundary()) {
        ShadowRoot* root = toShadowRoot(treeScope()->rootNode());
        if (doesSelectFromHostChildren()) {
            distributeHostChildren(root->tree());
            attachDistributedNode();
        } else if (!root->olderShadowRoot()->assignedTo()) {
            ASSERT(!root->olderShadowRoot()->attached());
            assignShadowRoot(root->olderShadowRoot());
            root->olderShadowRoot()->attach();
        }
    }

    HTMLElement::attach();
}

void InsertionPoint::detach()
{
    ShadowRoot* root = toShadowRoot(shadowTreeRootNode());
    if (root && isActive()) {
        ShadowTree* tree = root->tree();

        if (doesSelectFromHostChildren())
            clearDistribution(tree);
        else if (ShadowRoot* assignedShadowRoot = assignedFrom())
            clearAssignment(assignedShadowRoot);

        // When shadow element is detached, shadow tree should be recreated to re-calculate selector for
        // other insertion points.
        tree->setNeedsReattachHostChildrenAndShadow();
    }

    ASSERT(m_selections.isEmpty());
    HTMLElement::detach();
}

ShadowRoot* InsertionPoint::assignedFrom() const
{
    Node* treeScopeRoot = treeScope()->rootNode();
    if (!treeScopeRoot->isShadowRoot())
        return 0;

    ShadowRoot* olderShadowRoot = toShadowRoot(treeScopeRoot)->olderShadowRoot();
    if (olderShadowRoot && olderShadowRoot->assignedTo() == this)
        return olderShadowRoot;
    return 0;
}

bool InsertionPoint::isShadowBoundary() const
{
    return treeScope()->rootNode()->isShadowRoot() && isActive();
}

bool InsertionPoint::isActive() const
{
    const Node* node = parentNode();
    while (node) {
        if (WebCore::isInsertionPoint(node))
            return false;

        node = node->parentNode();
    }
    return true;
}

bool InsertionPoint::rendererIsNeeded(const NodeRenderingContext& context)
{
    return !isShadowBoundary() && HTMLElement::rendererIsNeeded(context);
}

inline void InsertionPoint::distributeHostChildren(ShadowTree* tree)
{
    if (!tree->selector().isSelecting()) {
        // If HTMLContentSelector is not int selecting phase, it means InsertionPoint is attached from
        // non-ShadowTree node. To run distribute algorithm, we have to reattach ShadowTree.
        tree->setNeedsReattachHostChildrenAndShadow();
        return;
    }

    tree->selector().populateIfNecessary(tree->host());
    tree->selector().unselect(&m_selections);
    tree->selector().select(this, &m_selections);
}

inline void InsertionPoint::clearDistribution(ShadowTree* tree)
{
    tree->selector().unselect(&m_selections);
}

inline void InsertionPoint::attachDistributedNode()
{
    for (HTMLContentSelection* selection = m_selections.first(); selection; selection = selection->next())
        selection->node()->attach();
}

inline void InsertionPoint::assignShadowRoot(ShadowRoot* shadowRoot)
{
    shadowRoot->setAssignedTo(this);
    m_selections.clear();
    for (Node* node = shadowRoot->firstChild(); node; node = node->nextSibling())
        m_selections.append(HTMLContentSelection::create(this, node));
}

inline void InsertionPoint::clearAssignment(ShadowRoot* shadowRoot)
{
    shadowRoot->setAssignedTo(0);
    m_selections.clear();
}

} // namespace WebCore
