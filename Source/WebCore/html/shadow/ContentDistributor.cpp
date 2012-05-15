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

        distribution->append(child);
        m_nodeToInsertionPoint.add(child, insertionPoint);
        m_pool[i] = 0;
    }
}

void ContentDistributor::clearDistribution(ContentDistribution* list)
{
    for (size_t i = 0; i < list->size(); ++i)
        m_nodeToInsertionPoint.remove(list->at(i).get());
    list->clear();
}

InsertionPoint* ContentDistributor::findInsertionPointFor(const Node* key) const
{
    return m_nodeToInsertionPoint.get(key);
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
