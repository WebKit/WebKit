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
#include "RuntimeEnabledFeatures.h"
#include "ShadowRoot.h"

namespace WebCore {

class ShadowRootVector : public Vector<RefPtr<ShadowRoot> > {
public:
    explicit ShadowRootVector(ShadowRootList* list)
    {
        for (ShadowRoot* root = list->youngestShadowRoot(); root; root = root->olderShadowRoot())
            append(root);
    }
};

ShadowRootList::ShadowRootList()
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
    ShadowRootVector roots(this);
    for (size_t i = 0; i < roots.size(); ++i)
        roots[i]->insertedIntoDocument();
}

void ShadowRootList::removedFromDocument()
{
    ShadowRootVector roots(this);
    for (size_t i = 0; i < roots.size(); ++i)
        roots[i]->removedFromDocument();
}

void ShadowRootList::insertedIntoTree(bool deep)
{
    ShadowRootVector roots(this);
    for (size_t i = 0; i < roots.size(); ++i)
        roots[i]->insertedIntoTree(deep);
}

void ShadowRootList::removedFromTree(bool deep)
{
    ShadowRootVector roots(this);
    for (size_t i = 0; i < roots.size(); ++i)
        roots[i]->removedFromTree(deep);
}

void ShadowRootList::willRemove()
{
    ShadowRootVector roots(this);
    for (size_t i = 0; i < roots.size(); ++i)
        roots[i]->willRemove();
}

void ShadowRootList::hostChildrenChanged()
{
    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot())
        root->hostChildrenChanged();
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

}
