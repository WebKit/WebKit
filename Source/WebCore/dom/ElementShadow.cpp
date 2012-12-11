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

#include "CSSParser.h"
#include "CSSSelectorList.h"
#include "ContainerNodeAlgorithms.h"
#include "Document.h"
#include "Element.h"
#include "HTMLContentElement.h"
#include "HTMLShadowElement.h"
#include "InspectorInstrumentation.h"
#include "NodeTraversal.h"
#include "ShadowRoot.h"
#include "StyleResolver.h"

namespace WebCore {

ElementShadow::ElementShadow()
    : m_shouldCollectSelectFeatureSet(false)
{
}

ElementShadow::~ElementShadow()
{
    ASSERT(m_shadowRoots.isEmpty());
}

static bool validateShadowRoot(Document* document, ShadowRoot* shadowRoot, ExceptionCode& ec)
{
    if (!shadowRoot)
        return true;

    if (shadowRoot->host()) {
        ec = HIERARCHY_REQUEST_ERR;
        return false;
    }

    if (shadowRoot->document() != document) {
        ec = WRONG_DOCUMENT_ERR;
        return false;
    }

    return true;
}

void ElementShadow::addShadowRoot(Element* shadowHost, PassRefPtr<ShadowRoot> shadowRoot, ShadowRoot::ShadowRootType type, ExceptionCode& ec)
{
    ASSERT(shadowHost);
    ASSERT(shadowRoot);

    if (!validateShadowRoot(shadowHost->document(), shadowRoot.get(), ec))
        return;

    if (type == ShadowRoot::AuthorShadowRoot)
        shadowHost->willAddAuthorShadowRoot();

    shadowRoot->setHost(shadowHost);
    shadowRoot->setParentTreeScope(shadowHost->treeScope());
    m_shadowRoots.push(shadowRoot.get());
    setValidityUndetermined();
    invalidateDistribution(shadowHost);
    ChildNodeInsertionNotifier(shadowHost).notify(shadowRoot.get());

    // FIXME(94905): ShadowHost should be reattached during recalcStyle.
    // Set some flag here and recreate shadow hosts' renderer in
    // Element::recalcStyle.
    if (shadowHost->attached())
        shadowHost->lazyReattach();

    InspectorInstrumentation::didPushShadowRoot(shadowHost, shadowRoot.get());
}

void ElementShadow::removeAllShadowRoots()
{
    // Dont protect this ref count.
    Element* shadowHost = host();

    while (RefPtr<ShadowRoot> oldRoot = m_shadowRoots.head()) {
        InspectorInstrumentation::willPopShadowRoot(shadowHost, oldRoot.get());
        shadowHost->document()->removeFocusedNodeOfSubtree(oldRoot.get());

        if (oldRoot->attached())
            oldRoot->detach();

        m_shadowRoots.removeHead();
        oldRoot->setHost(0);
        oldRoot->setPrev(0);
        oldRoot->setNext(0);
        shadowHost->document()->adoptIfNeeded(oldRoot.get());
        ChildNodeRemovalNotifier(shadowHost).notify(oldRoot.get());
    }

    invalidateDistribution(shadowHost);
}

void ElementShadow::attach()
{
    ensureDistribution();
    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        if (!root->attached())
            root->attach();
    }
}

void ElementShadow::detach()
{
    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        if (root->attached())
            root->detach();
    }
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
    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot())
        root->recalcStyle(change);
}

void ElementShadow::ensureDistribution()
{
    if (!m_distributor.needsDistribution())
        return;
    m_distributor.distribute(host());
}

void ElementShadow::setValidityUndetermined()
{
    m_distributor.setValidity(ContentDistributor::Undetermined);
}

void ElementShadow::invalidateDistribution()
{
    invalidateDistribution(host());
}

void ElementShadow::invalidateDistribution(Element* host)
{
    bool needsInvalidation = m_distributor.needsInvalidation();
    bool needsReattach = needsInvalidation ? m_distributor.invalidate(host) : false;

    if (needsReattach && host->attached()) {
        for (Node* n = host->firstChild(); n; n = n->nextSibling())
            n->lazyReattach();
        host->setNeedsStyleRecalc();
    }

    if (needsInvalidation)
        m_distributor.finishInivalidation();
}

void ElementShadow::setShouldCollectSelectFeatureSet()
{
    if (shouldCollectSelectFeatureSet())
        return;

    m_shouldCollectSelectFeatureSet = true;

    if (ShadowRoot* parentShadowRoot = host()->containingShadowRoot()) {
        if (ElementShadow* parentElementShadow = parentShadowRoot->owner())
            parentElementShadow->setShouldCollectSelectFeatureSet();
    }
}

void ElementShadow::ensureSelectFeatureSetCollected()
{
    if (!m_shouldCollectSelectFeatureSet)
        return;

    m_selectFeatures.clear();
    for (ShadowRoot* root = oldestShadowRoot(); root; root = root->youngerShadowRoot())
        collectSelectFeatureSetFrom(root);
    m_shouldCollectSelectFeatureSet = false;
}

void ElementShadow::collectSelectFeatureSetFrom(ShadowRoot* root)
{
    if (root->hasElementShadow()) {
        for (Node* node = root->firstChild(); node; node = NodeTraversal::next(node)) {
            if (ElementShadow* elementShadow = node->isElementNode() ? toElement(node)->shadow() : 0) {
                elementShadow->ensureSelectFeatureSetCollected();
                m_selectFeatures.add(elementShadow->m_selectFeatures);
            }
        }
    }

    if (root->hasContentElement()) {
        for (Node* node = root->firstChild(); node; node = NodeTraversal::next(node)) {
            if (isHTMLContentElement(node)) {
                const CSSSelectorList& list = toHTMLContentElement(node)->selectorList();
                for (CSSSelector* selector = list.first(); selector; selector = list.next(selector))
                    m_selectFeatures.collectFeaturesFromSelector(selector);                    
            }
        }
    }
}

void ElementShadow::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::DOM);
    info.addMember(m_shadowRoots);
    ShadowRoot* shadowRoot = m_shadowRoots.head();
    while (shadowRoot) {
        info.addMember(shadowRoot);
        shadowRoot = shadowRoot->next();
    }
    info.addMember(m_distributor);
}

void invalidateParentDistributionIfNecessary(Element* element, SelectRuleFeatureSet::SelectRuleFeatureMask updatedFeatures)
{
    ElementShadow* elementShadow = shadowOfParentForDistribution(element);
    if (!elementShadow)
        return;

    elementShadow->ensureSelectFeatureSetCollected();
    if (elementShadow->selectRuleFeatureSet().hasSelectorFor(updatedFeatures))
        elementShadow->invalidateDistribution();
}

} // namespace
