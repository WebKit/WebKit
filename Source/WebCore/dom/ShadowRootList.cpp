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
#include "ShadowRootList.h"

#include "Document.h"
#include "Element.h"
#include "HTMLContentSelector.h"
#include "RuntimeEnabledFeatures.h"
#include "ShadowRoot.h"
#include "Text.h"

namespace WebCore {

ShadowRootList::ShadowRootList()
    : m_needsRecalculateContent(false)
{
}

ShadowRootList::~ShadowRootList()
{
    ASSERT(!hasShadowRoot());
}

void ShadowRootList::pushShadowRoot(ShadowRoot* shadowRoot)
{
#if ENABLE(SHADOW_DOM)
    if (!RuntimeEnabledFeatures::multipleShadowSubtreesEnabled())
        ASSERT(!hasShadowRoot());
#else
    ASSERT(!hasShadowRoot());
#endif

    m_shadowRoots.push(shadowRoot);
}

ShadowRoot* ShadowRootList::popShadowRoot()
{
    if (!hasShadowRoot())
        return 0;

    return m_shadowRoots.removeHead();
}

void ShadowRootList::insertedIntoDocument()
{
    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot())
        root->insertedIntoDocument();
}

void ShadowRootList::removedFromDocument()
{
    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot())
        root->removedFromDocument();
}

void ShadowRootList::insertedIntoTree(bool deep)
{
    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot())
        root->insertedIntoTree(deep);
}

void ShadowRootList::removedFromTree(bool deep)
{
    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot())
        root->removedFromTree(deep);
}

void ShadowRootList::willRemove()
{
    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot())
        root->willRemove();
}

void ShadowRootList::attach()
{
    // FIXME: Currently we only support the case that the shadow root list has at most one shadow root.
    // See also https://bugs.webkit.org/show_bug.cgi?id=77503 and its dependent bugs.
    ASSERT(m_shadowRoots.size() <= 1);

    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        if (!root->attached())
            root->attach();
    }
}

void ShadowRootList::detach()
{
    // FIXME: Currently we only support the case that the shadow root list has at most one shadow root.
    // See also https://bugs.webkit.org/show_bug.cgi?id=77503 and its dependent bugs.
    ASSERT(m_shadowRoots.size() <= 1);

    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        if (root->attached())
            root->detach();
    }
}

InsertionPoint* ShadowRootList::insertionPointFor(Node* node) const
{
    if (!m_selector)
        return 0;
    HTMLContentSelection* found = m_selector->findFor(node);
    if (!found)
        return 0;
    return found->insertionPoint();
}

bool ShadowRootList::isSelectorActive() const
{
    return m_selector && m_selector->hasCandidates();
}

void ShadowRootList::reattach()
{
    detach();
    attach();
}

bool ShadowRootList::childNeedsStyleRecalc()
{
    ASSERT(youngestShadowRoot());
    if (!youngestShadowRoot())
        return false;

    return youngestShadowRoot()->childNeedsStyleRecalc();
}

bool ShadowRootList::needsStyleRecalc()
{
    ASSERT(youngestShadowRoot());
    if (!youngestShadowRoot())
        return false;

    return youngestShadowRoot()->needsStyleRecalc();
}

void ShadowRootList::recalcShadowTreeStyle(Node::StyleChange change)
{
    ShadowRoot* youngest = youngestShadowRoot();
    if (!youngest)
        return;

    if (needsReattachHostChildrenAndShadow())
        reattachHostChildrenAndShadow();
    else {
        for (Node* n = youngest->firstChild(); n; n = n->nextSibling()) {
            if (n->isElementNode())
                static_cast<Element*>(n)->recalcStyle(change);
            else if (n->isTextNode())
                toText(n)->recalcTextStyle(change);
        }
    }

    clearNeedsReattachHostChildrenAndShadow();
    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        root->clearNeedsStyleRecalc();
        root->clearChildNeedsStyleRecalc();
    }
}

bool ShadowRootList::needsReattachHostChildrenAndShadow()
{
    return m_needsRecalculateContent || (youngestShadowRoot() && youngestShadowRoot()->hasContentElement());
}

void ShadowRootList::hostChildrenChanged()
{
    ASSERT(youngestShadowRoot());

    if (!youngestShadowRoot()->hasContentElement())
        return;

    // This results in forced detaching/attaching of the shadow render tree. See ShadowRoot::recalcStyle().
    setNeedsReattachHostChildrenAndShadow();
}

void ShadowRootList::setNeedsReattachHostChildrenAndShadow()
{
    m_needsRecalculateContent = true;

    host()->setNeedsStyleRecalc();
}

void ShadowRootList::reattachHostChildrenAndShadow()
{
    ASSERT(youngestShadowRoot());

    Node* hostNode = youngestShadowRoot()->host();
    if (!hostNode)
        return;

    for (Node* child = hostNode->firstChild(); child; child = child->nextSibling()) {
        if (child->attached())
            child->detach();
    }

    reattach();

    for (Node* child = hostNode->firstChild(); child; child = child->nextSibling()) {
        if (!child->attached())
            child->attach();
    }
}

HTMLContentSelector* ShadowRootList::ensureSelector()
{
    if (!m_selector)
        m_selector = adoptPtr(new HTMLContentSelector());
    m_selector->willSelectOver(host());
    return m_selector.get();
}

} // namespace
