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

#include "ElementShadow.h"
#include "ShadowRoot.h"

namespace WebCore {

InsertionPoint::InsertionPoint(const QualifiedName& tagName, Document* document)
    : HTMLElement(tagName, document)
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
            distributeHostChildren(root->owner());
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
    ShadowRoot* root = shadowTreeRootNode();
    if (root && isActive()) {
        ElementShadow* shadow = root->owner();

        if (doesSelectFromHostChildren())
            clearDistribution(shadow);
        else if (ShadowRoot* assignedShadowRoot = assignedFrom())
            clearAssignment(assignedShadowRoot);

        // When shadow element is detached, shadow tree should be recreated to re-calculate selector for
        // other insertion points.
        shadow->setNeedsRedistributing();
    }

    ASSERT(m_distribution.isEmpty());
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

inline void InsertionPoint::distributeHostChildren(ElementShadow* shadow)
{
    if (!shadow->distributor().inDistribution()) {
        // If ContentDistributor is not int selecting phase, it means InsertionPoint is attached from
        // non-ElementShadow node. To run distribute algorithm, we have to reattach ElementShadow.
        shadow->setNeedsRedistributing();
        return;
    }

    shadow->distributor().preparePoolFor(shadow->host());
    shadow->distributor().clearDistribution(&m_distribution);
    shadow->distributor().distribute(this, &m_distribution);
}

inline void InsertionPoint::clearDistribution(ElementShadow* shadow)
{
    shadow->distributor().clearDistribution(&m_distribution);
}

inline void InsertionPoint::attachDistributedNode()
{
    for (size_t i = 0; i < m_distribution.size(); ++i)
        m_distribution.at(i)->attach();
}

inline void InsertionPoint::assignShadowRoot(ShadowRoot* shadowRoot)
{
    shadowRoot->setAssignedTo(this);
    m_distribution.clear();
    for (Node* node = shadowRoot->firstChild(); node; node = node->nextSibling())
        m_distribution.append(node);
}

inline void InsertionPoint::clearAssignment(ShadowRoot* shadowRoot)
{
    shadowRoot->setAssignedTo(0);
    m_distribution.clear();
}

Node* InsertionPoint::nextTo(const Node* node) const
{
    size_t index = m_distribution.find(node);
    if (index == notFound || index + 1 == m_distribution.size())
        return 0;
    return m_distribution.at(index + 1).get();
}

Node* InsertionPoint::previousTo(const Node* node) const
{
    size_t index = m_distribution.find(node);
    if (index == notFound || !index)
        return 0;
    return m_distribution.at(index - 1).get();
}


} // namespace WebCore
