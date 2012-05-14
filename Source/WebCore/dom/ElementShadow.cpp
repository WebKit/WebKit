/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
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
#include "ElementShadow.h"

#include "ContainerNodeAlgorithms.h"
#include "Document.h"
#include "Element.h"
#include "HTMLShadowElement.h"
#include "InspectorInstrumentation.h"
#include "RuntimeEnabledFeatures.h"
#include "ShadowRoot.h"
#include "StyleResolver.h"
#include "Text.h"

namespace WebCore {

ElementShadow::ElementShadow()
    : m_needsRedistributing(false)
{
}

ElementShadow::~ElementShadow()
{
    removeAllShadowRoots();
}

static bool validateShadowRoot(Document* document, ShadowRoot* shadowRoot, ExceptionCode& ec)
{
    if (!shadowRoot)
        return true;

    if (shadowRoot->shadowHost()) {
        ec = HIERARCHY_REQUEST_ERR;
        return false;
    }

    if (shadowRoot->document() != document) {
        ec = WRONG_DOCUMENT_ERR;
        return false;
    }

    return true;
}

void ElementShadow::addShadowRoot(Element* shadowHost, PassRefPtr<ShadowRoot> shadowRoot, ExceptionCode& ec)
{
    ASSERT(shadowHost);
    ASSERT(shadowRoot);

    if (!validateShadowRoot(shadowHost->document(), shadowRoot.get(), ec))
        return;

    shadowRoot->setShadowHost(shadowHost);
    ChildNodeInsertionNotifier(shadowHost).notify(shadowRoot.get());

    if (shadowHost->attached()) {
        shadowRoot->lazyAttach();
        detach();
        shadowHost->detachChildren();
    }

    m_shadowRoots.push(shadowRoot.get());
    InspectorInstrumentation::didPushShadowRoot(shadowHost, shadowRoot.get());
}

void ElementShadow::removeAllShadowRoots()
{
    // Dont protect this ref count.
    Element* shadowHost = host();

    while (RefPtr<ShadowRoot> oldRoot = m_shadowRoots.removeHead()) {
        InspectorInstrumentation::willPopShadowRoot(shadowHost, oldRoot.get());
        shadowHost->document()->removeFocusedNodeOfSubtree(oldRoot.get());

        if (oldRoot->attached())
            oldRoot->detach();

        oldRoot->setShadowHost(0);
        oldRoot->setPrev(0);
        oldRoot->setNext(0);
        shadowHost->document()->adoptIfNeeded(oldRoot.get());
        ChildNodeRemovalNotifier(shadowHost).notify(oldRoot.get());
    }

    if (shadowHost->attached())
        shadowHost->attachChildrenLazily();
}

void ElementShadow::setParentTreeScope(TreeScope* scope)
{
    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot())
        root->setParentTreeScope(scope);
}

void ElementShadow::attach()
{
    // The pool nodes are populated lazily in
    // ensureDistributor(), and here we just ensure that it is in clean state.
    ASSERT(!distributor().poolIsReady());

    distributor().willDistribute();
    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        if (!root->attached())
            root->attach();
    }
    distributor().didDistribute();
}

void ElementShadow::attachHost(Element* host)
{
    attach();
    host->attachChildrenIfNeeded();
    host->attachAsNode();
}

void ElementShadow::detach()
{
    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        if (root->attached())
            root->detach();
    }
}

void ElementShadow::detachHost(Element* host)
{
    host->detachChildrenIfNeeded();
    detach();
    host->detachAsNode();
}

InsertionPoint* ElementShadow::insertionPointFor(const Node* node) const
{
    ASSERT(node && node->parentNode());

    if (node->parentNode()->isShadowRoot()) {
        if (InsertionPoint* insertionPoint = toShadowRoot(node->parentNode())->assignedTo())
            return insertionPoint;

        return 0;
    }

    return distributor().findInsertionPointFor(node);
}

ContentDistribution::Item* ElementShadow::distributionItemFor(const Node* node) const
{
    return m_distributor.findFor(node);
}

void ElementShadow::reattach()
{
    detach();
    attach();
}

bool ElementShadow::childNeedsStyleRecalc()
{
    ASSERT(youngestShadowRoot());
    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot())
        if (root->childNeedsStyleRecalc())
            return true;

    return false;
}

bool ElementShadow::needsStyleRecalc()
{
    ASSERT(youngestShadowRoot());
    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot())
        if (root->needsStyleRecalc())
            return true;

    return false;
}

void ElementShadow::recalcStyle(Node::StyleChange change)
{
    ShadowRoot* youngest = youngestShadowRoot();
    if (!youngest)
        return;

    if (needsRedistributing())
        reattachHostChildrenAndShadow();
    else {
        StyleResolver* styleResolver = youngest->document()->styleResolver();

        styleResolver->pushParentShadowRoot(youngest);
        for (Node* n = youngest->firstChild(); n; n = n->nextSibling()) {
            if (n->isElementNode())
                static_cast<Element*>(n)->recalcStyle(change);
            else if (n->isTextNode())
                toText(n)->recalcTextStyle(change);
        }
        styleResolver->popParentShadowRoot(youngest);
    }

    clearNeedsRedistributing();
    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        root->clearNeedsStyleRecalc();
        root->clearChildNeedsStyleRecalc();
    }
}

bool ElementShadow::needsRedistributing()
{
    return m_needsRedistributing || (youngestShadowRoot() && youngestShadowRoot()->hasInsertionPoint());
}

void ElementShadow::hostChildrenChanged()
{
    ASSERT(youngestShadowRoot());

    if (!youngestShadowRoot()->hasInsertionPoint())
        return;

    // This results in forced detaching/attaching of the shadow render tree. See ShadowRoot::recalcStyle().
    setNeedsRedistributing();
}

void ElementShadow::setNeedsRedistributing()
{
    m_needsRedistributing = true;

    host()->setNeedsStyleRecalc();
}

void ElementShadow::reattachHostChildrenAndShadow()
{
    ASSERT(youngestShadowRoot());

    Element* hostNode = youngestShadowRoot()->host();
    hostNode->detachChildrenIfNeeded();
    reattach();
    hostNode->attachChildrenIfNeeded();
}

} // namespace
