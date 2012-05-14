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
#include "ContentDistributor.h"

#include "ContentSelectorQuery.h"
#include "HTMLContentElement.h"
#include "ShadowRoot.h"


namespace WebCore {

ContentDistribution::ContentDistribution()
{
}

ContentDistribution::~ContentDistribution()
{
    ASSERT(isEmpty());
}

ContentDistribution::Item* ContentDistribution::find(Node* node) const
{
    for (ContentDistribution::Item* item = first(); item; item = item->next()) {
        if (node == item->node())
            return item;
    }
    
    return 0;
}

void ContentDistribution::clear()
{
    if (isEmpty()) {
        ASSERT(!m_last);
        return;
    }

    RefPtr<ContentDistribution::Item> item = m_first;
    while (item) {
        ASSERT(!item->previous());
        RefPtr<ContentDistribution::Item> nextItem = item->m_next;
        item->m_next.clear();
        if (nextItem)
            nextItem->m_previous.clear();
        item = nextItem;
    }

    m_first.clear();
    m_last.clear();
}

void ContentDistribution::append(InsertionPoint* insertionPoint, Node* node)
{
    RefPtr<Item> child = Item::create(insertionPoint, node);

    if (isEmpty()) {
        ASSERT(!m_last);
        m_first = m_last = child;
        return;
    }

    ASSERT(!m_last->next());
    ASSERT(!child->previous());
    m_last->m_next = child;
    child->m_previous = m_last;
    m_last = m_last->next();
}

ContentDistributor::ContentDistributor()
    : m_phase(Prevented)
{
}

ContentDistributor::~ContentDistributor()
{
    ASSERT(m_pool.isEmpty());
}

void ContentDistributor::distribute(InsertionPoint* insertionPoint, ContentDistribution* distribution)
{
    ASSERT(m_phase == Prepared);
    ASSERT(distribution->isEmpty());

    ContentSelectorQuery query(insertionPoint);

    for (size_t i = 0; i < m_pool.size(); ++i) {
        Node* child = m_pool[i].get();
        if (!child)
            continue;
        if (!query.matches(child))
            continue;

        distribution->append(insertionPoint, child);
        m_nodeToInsertionPoint.add(distribution->last());
        m_pool[i] = 0;
    }
}

void ContentDistributor::clearDistribution(ContentDistribution* list)
{
    for (ContentDistribution::Item* item = list->first(); item; item = item->next())
        m_nodeToInsertionPoint.remove(item);

    list->clear();
}

ContentDistribution::Item* ContentDistributor::findFor(const Node* key) const
{
    InvertedTable::iterator found = m_nodeToInsertionPoint.find<const Node*, Translator>(key);
    return found != m_nodeToInsertionPoint.end() ? *found : 0;
}

InsertionPoint* ContentDistributor::findInsertionPointFor(const Node* key) const
{
    InvertedTable::iterator found = m_nodeToInsertionPoint.find<const Node*, Translator>(key);
    return found != m_nodeToInsertionPoint.end() ? (*found)->insertionPoint() : 0;
}

void ContentDistributor::willDistribute()
{
    m_phase = Started;
}

void ContentDistributor::didDistribute()
{
    ASSERT(m_phase != Prevented);
    m_phase = Prevented;
    m_pool.clear();
}

void ContentDistributor::preparePoolFor(Element* shadowHost)
{
    if (poolIsReady())
        return;

    ASSERT(m_pool.isEmpty());
    ASSERT(shadowHost);
    ASSERT(m_phase == Started);

    m_phase = Prepared;
    for (Node* node = shadowHost->firstChild(); node; node = node->nextSibling())
        m_pool.append(node);
}

}
