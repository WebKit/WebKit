/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "ShadowInclusionSelector.h"

#include "ShadowContentElement.h"
#include "ShadowRoot.h"


namespace WebCore {

void ShadowInclusion::append(PassRefPtr<ShadowInclusion> next)
{
    ASSERT(!m_next);
    ASSERT(!next->previous());
    m_next = next;
    m_next->m_previous = this;
}

void ShadowInclusion::unlink()
{
    ASSERT(!m_previous); // Can be called only for a head.
    RefPtr<ShadowInclusion> item = this;
    while (item) {
        ASSERT(!item->previous());
        RefPtr<ShadowInclusion> nextItem = item->m_next;
        item->m_next.clear();
        if (nextItem)
            nextItem->m_previous.clear();
        item = nextItem;
    }
}

ShadowInclusionList::ShadowInclusionList()
{
}

ShadowInclusionList::~ShadowInclusionList()
{
    ASSERT(isEmpty());
}

ShadowInclusion* ShadowInclusionList::find(Node* content) const
{
    for (ShadowInclusion* item = first(); item; item = item->next()) {
        if (content == item->content())
            return item;
    }
    
    return 0;
}

void ShadowInclusionList::clear()
{
    if (isEmpty()) {
        ASSERT(!m_last);
        return;
    }

    m_first->unlink();
    m_first.clear();
    m_last.clear();
}

void ShadowInclusionList::append(PassRefPtr<ShadowInclusion> child)
{
    if (isEmpty()) {
        ASSERT(!m_last);
        m_first = m_last = child;
        return;
    }

    m_last->append(child);
    m_last = m_last->next();
}

ShadowInclusionSelector::ShadowInclusionSelector()
{
}

ShadowInclusionSelector::~ShadowInclusionSelector()
{
    ASSERT(m_candidates.isEmpty());
}

void ShadowInclusionSelector::select(ShadowContentElement* contentElement, ShadowInclusionList* inclusions)
{
    ASSERT(inclusions->isEmpty());

    for (size_t i = 0; i < m_candidates.size(); ++i) {
        Node* child = m_candidates[i].get();
        if (!child)
            continue;
        if (!contentElement->shouldInclude(child))
            continue;

        RefPtr<ShadowInclusion> inclusion = ShadowInclusion::create(contentElement, child);
        inclusions->append(inclusion);
        m_inclusionSet.add(inclusion.get());
        m_candidates[i] = 0;
    }
}

void ShadowInclusionSelector::unselect(ShadowInclusionList* list)
{
    for (ShadowInclusion* inclusion = list->first(); inclusion; inclusion = inclusion->next())
        m_inclusionSet.remove(inclusion);
    list->clear();
}

ShadowInclusion* ShadowInclusionSelector::findFor(Node* key) const
{
    return m_inclusionSet.find(key);
}

void ShadowInclusionSelector::didSelect()
{
    m_candidates.clear();
}

void ShadowInclusionSelector::willSelectOver(ShadowRoot* scope)
{
    if (!m_candidates.isEmpty())
        return;
    for (Node* node = scope->shadowHost()->firstChild(); node; node = node->nextSibling())
        m_candidates.append(node);
}

}
