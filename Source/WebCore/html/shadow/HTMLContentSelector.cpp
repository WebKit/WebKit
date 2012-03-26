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
#include "HTMLContentSelector.h"

#include "ContentSelectorQuery.h"
#include "HTMLContentElement.h"
#include "ShadowRoot.h"


namespace WebCore {

void HTMLContentSelection::append(PassRefPtr<HTMLContentSelection> next)
{
    ASSERT(!m_next);
    ASSERT(!next->previous());
    m_next = next;
    m_next->m_previous = this;
}

void HTMLContentSelection::unlink()
{
    ASSERT(!m_previous); // Can be called only for a head.
    RefPtr<HTMLContentSelection> item = this;
    while (item) {
        ASSERT(!item->previous());
        RefPtr<HTMLContentSelection> nextItem = item->m_next;
        item->m_next.clear();
        if (nextItem)
            nextItem->m_previous.clear();
        item = nextItem;
    }
}

HTMLContentSelectionList::HTMLContentSelectionList()
{
}

HTMLContentSelectionList::~HTMLContentSelectionList()
{
    ASSERT(isEmpty());
}

HTMLContentSelection* HTMLContentSelectionList::find(Node* node) const
{
    for (HTMLContentSelection* item = first(); item; item = item->next()) {
        if (node == item->node())
            return item;
    }
    
    return 0;
}

void HTMLContentSelectionList::clear()
{
    if (isEmpty()) {
        ASSERT(!m_last);
        return;
    }

    m_first->unlink();
    m_first.clear();
    m_last.clear();
}

void HTMLContentSelectionList::append(PassRefPtr<HTMLContentSelection> child)
{
    if (isEmpty()) {
        ASSERT(!m_last);
        m_first = m_last = child;
        return;
    }

    m_last->append(child);
    m_last = m_last->next();
}

HTMLContentSelector::HTMLContentSelector()
    : m_phase(SelectionPrevented)
{
}

HTMLContentSelector::~HTMLContentSelector()
{
    ASSERT(m_candidates.isEmpty());
}

void HTMLContentSelector::select(InsertionPoint* insertionPoint, HTMLContentSelectionList* selections)
{
    ASSERT(m_phase == HostChildrenPopulated);
    ASSERT(selections->isEmpty());

    ContentSelectorQuery query(insertionPoint);

    for (size_t i = 0; i < m_candidates.size(); ++i) {
        Node* child = m_candidates[i].get();
        if (!child)
            continue;
        if (!query.matches(child))
            continue;

        RefPtr<HTMLContentSelection> selection = HTMLContentSelection::create(insertionPoint, child);

        selections->append(selection);
        m_selectionSet.add(selection.get());
        m_candidates[i] = 0;
    }
}

void HTMLContentSelector::unselect(HTMLContentSelectionList* list)
{
    for (HTMLContentSelection* selection = list->first(); selection; selection = selection->next())
        m_selectionSet.remove(selection);
    list->clear();
}

HTMLContentSelection* HTMLContentSelector::findFor(const Node* key) const
{
    return m_selectionSet.find(key);
}

void HTMLContentSelector::willSelect()
{
    m_phase = SelectionStarted;
}

void HTMLContentSelector::didSelect()
{
    ASSERT(m_phase != SelectionPrevented);
    m_phase = SelectionPrevented;
    m_candidates.clear();
}

void HTMLContentSelector::populateIfNecessary(Element* shadowHost)
{
    if (hasPopulated())
        return;

    ASSERT(m_candidates.isEmpty());
    ASSERT(shadowHost);
    ASSERT(m_phase == SelectionStarted);

    m_phase = HostChildrenPopulated;
    for (Node* node = shadowHost->firstChild(); node; node = node->nextSibling())
        m_candidates.append(node);
}

}
