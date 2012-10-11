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
#include "ElementShadow.h"
#include "HTMLContentElement.h"
#include "ShadowRoot.h"


namespace WebCore {

ContentDistributor::ContentDistributor()
    : m_validity(Undetermined)
{
}

ContentDistributor::~ContentDistributor()
{
}

InsertionPoint* ContentDistributor::findInsertionPointFor(const Node* key) const
{
    return m_nodeToInsertionPoint.get(key);
}

void ContentDistributor::distribute(Element* host)
{
    ASSERT(needsDistribution());
    ASSERT(m_nodeToInsertionPoint.isEmpty());

    m_validity = Valid;

    ContentDistribution pool;
    for (Node* node = host->firstChild(); node; node = node->nextSibling()) {
        if (!isInsertionPoint(node)) {
            pool.append(node);
            continue;
        }

        InsertionPoint* insertionPoint = toInsertionPoint(node);
        if (insertionPoint->hasDistribution()) {
            for (size_t i = 0; i < insertionPoint->size(); ++i)
                pool.append(insertionPoint->at(i));
        } else {
            for (Node* fallbackNode = insertionPoint->firstChild(); fallbackNode; fallbackNode = fallbackNode->nextSibling())
                pool.append(fallbackNode);
        }
    }
    Vector<bool> distributed(pool.size());
    distributed.fill(false);

    for (ShadowRoot* root = host->youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        for (Node* node = root; node; node = node->traverseNextNode(root)) {
            if (!isInsertionPoint(node))
                continue;
            InsertionPoint* point = toInsertionPoint(node);
            if (!point->isActive())
                continue;
            ShadowRoot* older = root->olderShadowRoot();
            if (point->doesSelectFromHostChildren())
                distributeSelectionsTo(point, pool, distributed);
            else if (older && !older->assignedTo()) {
                distributeNodeChildrenTo(point, older);
                older->setAssignedTo(point);
            }
        }
    }
}

bool ContentDistributor::invalidate(Element* host)
{
    ASSERT(needsInvalidation());
    bool needsReattach = (m_validity == Undetermined) || !m_nodeToInsertionPoint.isEmpty();

    for (ShadowRoot* root = host->youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        root->setAssignedTo(0);

        for (Node* node = root; node; node = node->traverseNextNode(root)) {
            if (!isInsertionPoint(node))
                continue;
            needsReattach = needsReattach || true;
            InsertionPoint* point = toInsertionPoint(node);
            point->clearDistribution();
        }
    }

    m_validity = Invalidating;
    m_nodeToInsertionPoint.clear();
    return needsReattach;
}

void ContentDistributor::finishInivalidation()
{
    ASSERT(m_validity == Invalidating);
    m_validity = Invalidated;
}

void ContentDistributor::distributeSelectionsTo(InsertionPoint* insertionPoint, const ContentDistribution& pool, Vector<bool>& distributed)
{
    ContentDistribution distribution;
    ContentSelectorQuery query(insertionPoint);

    for (size_t i = 0; i < pool.size(); ++i) {
        if (distributed[i])
            continue;

        if (!query.matches(pool, i))
            continue;

        Node* child = pool[i].get();
        distribution.append(child);
        m_nodeToInsertionPoint.add(child, insertionPoint);
        distributed[i] = true;
    }

    insertionPoint->setDistribution(distribution);
}

void ContentDistributor::distributeNodeChildrenTo(InsertionPoint* insertionPoint, ContainerNode* containerNode)
{
    ContentDistribution distribution;
    for (Node* node = containerNode->firstChild(); node; node = node->nextSibling()) {
        distribution.append(node);
        m_nodeToInsertionPoint.add(node, insertionPoint);
    }

    insertionPoint->setDistribution(distribution);
}

void ContentDistributor::invalidateDistributionIn(ContentDistribution* list)
{
    for (size_t i = 0; i < list->size(); ++i)
        m_nodeToInsertionPoint.remove(list->at(i).get());
    list->clear();
}

}
