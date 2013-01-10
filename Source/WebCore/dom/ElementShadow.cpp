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
    m_distributor.didShadowBoundaryChange(shadowHost);
    ChildNodeInsertionNotifier(shadowHost).notify(shadowRoot.get());

    // Existence of shadow roots requires the host and its children to do traversal using ComposedShadowTreeWalker.
    shadowHost->setNeedsShadowTreeWalker();

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
        oldRoot->setParentTreeScope(shadowHost->document());
        oldRoot->setPrev(0);
        oldRoot->setNext(0);
        ChildNodeRemovalNotifier(shadowHost).notify(oldRoot.get());
    }

    m_distributor.invalidateDistribution(shadowHost);
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

} // namespace
