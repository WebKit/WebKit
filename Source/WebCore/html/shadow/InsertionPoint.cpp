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
#include "HTMLNames.h"
#include "QualifiedName.h"
#include "ShadowRoot.h"
#include "StaticNodeList.h"

namespace WebCore {

using namespace HTMLNames;

InsertionPoint::InsertionPoint(const QualifiedName& tagName, Document* document)
    : HTMLElement(tagName, document, CreateInsertionPoint)
    , m_registeredWithShadowRoot(false)
{
}

InsertionPoint::~InsertionPoint()
{
}

void InsertionPoint::attach()
{
    if (ShadowRoot* root = containingShadowRoot())
        root->owner()->ensureDistribution();
    for (size_t i = 0; i < m_distribution.size(); ++i) {
        if (!m_distribution.at(i)->attached())
            m_distribution.at(i)->attach();
    }

    HTMLElement::attach();
}

void InsertionPoint::detach()
{
    if (ShadowRoot* root = containingShadowRoot())
        root->owner()->ensureDistribution();
    for (size_t i = 0; i < m_distribution.size(); ++i)
        m_distribution.at(i)->detach();

    HTMLElement::detach();
}

bool InsertionPoint::shouldUseFallbackElements() const
{
    return isActive() && !hasDistribution();
}

bool InsertionPoint::isShadowBoundary() const
{
    return treeScope()->rootNode()->isShadowRoot() && isActive();
}

bool InsertionPoint::isActive() const
{
    if (!containingShadowRoot())
        return false;
    const Node* node = parentNode();
    while (node) {
        if (node->isInsertionPoint())
            return false;

        node = node->parentNode();
    }
    return true;
}

PassRefPtr<NodeList> InsertionPoint::getDistributedNodes() const
{
    if (treeScope()->rootNode()->isShadowRoot())
        toShadowRoot(treeScope()->rootNode())->owner()->ensureDistributionFromDocument();

    Vector<RefPtr<Node> > nodes;

    for (size_t i = 0; i < m_distribution.size(); ++i)
        nodes.append(m_distribution.at(i));

    return StaticNodeList::adopt(nodes);
}

bool InsertionPoint::rendererIsNeeded(const NodeRenderingContext& context)
{
    return !isShadowBoundary() && HTMLElement::rendererIsNeeded(context);
}

void InsertionPoint::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    HTMLElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
    if (ShadowRoot* root = containingShadowRoot())
        root->owner()->invalidateDistribution();
}

Node::InsertionNotificationRequest InsertionPoint::insertedInto(ContainerNode* insertionPoint)
{
    HTMLElement::insertedInto(insertionPoint);

    if (ShadowRoot* root = containingShadowRoot()) {
        root->owner()->setValidityUndetermined();
        root->owner()->invalidateDistribution();
        if (isActive() && !m_registeredWithShadowRoot && insertionPoint->treeScope()->rootNode() == root) {
            m_registeredWithShadowRoot = true;
            root->registerInsertionPoint(this);
        }
    }


    return InsertionDone;
}

void InsertionPoint::removedFrom(ContainerNode* insertionPoint)
{
    ShadowRoot* root = containingShadowRoot();
    if (!root)
        root = insertionPoint->containingShadowRoot();

    // host can be null when removedFrom() is called from ElementShadow destructor.
    ElementShadow* rootOwner = root ? root->owner() : 0;
    if (rootOwner)
        rootOwner->invalidateDistribution();

    // Since this insertion point is no longer visible from the shadow subtree, it need to clean itself up.
    clearDistribution();

    if (m_registeredWithShadowRoot && insertionPoint->treeScope()->rootNode() == root) {
        ASSERT(root);
        m_registeredWithShadowRoot = false;
        root->unregisterInsertionPoint(this);
    }

    HTMLElement::removedFrom(insertionPoint);
}

void InsertionPoint::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == reset_style_inheritanceAttr) {
        if (!inDocument() || !attached() || !isActive())
            return;
        containingShadowRoot()->host()->setNeedsStyleRecalc();
    } else
        HTMLElement::parseAttribute(name, value);
}

bool InsertionPoint::resetStyleInheritance() const
{
    return fastHasAttribute(reset_style_inheritanceAttr);
}

void InsertionPoint::setResetStyleInheritance(bool value)
{
    setBooleanAttribute(reset_style_inheritanceAttr, value);
}

} // namespace WebCore
