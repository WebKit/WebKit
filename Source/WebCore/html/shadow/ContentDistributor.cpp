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
#include "HTMLShadowElement.h"
#include "NodeTraversal.h"
#include "ShadowRoot.h"


namespace WebCore {

void ContentDistribution::swap(ContentDistribution& other)
{
    m_nodes.swap(other.m_nodes);
    m_indices.swap(other.m_indices);
}

void ContentDistribution::append(PassRefPtr<Node> node)
{
    size_t size = m_nodes.size();
    m_indices.set(node.get(), size);
    m_nodes.append(node);
}

size_t ContentDistribution::find(const Node* node) const
{
    HashMap<const Node*, size_t>::const_iterator it = m_indices.find(node);
    if (it == m_indices.end())
        return notFound;

    return it.get()->value;
}

Node* ContentDistribution::nextTo(const Node* node) const
{
    size_t index = find(node);
    if (index == notFound || index + 1 == size())
        return 0;
    return at(index + 1).get();
}

Node* ContentDistribution::previousTo(const Node* node) const
{
    size_t index = find(node);
    if (index == notFound || !index)
        return 0;
    return at(index - 1).get();
}


ShadowRootContentDistributionData::ShadowRootContentDistributionData()
    : m_insertionPointAssignedTo(0)
    , m_numberOfShadowElementChildren(0)
    , m_numberOfContentElementChildren(0)
    , m_numberOfElementShadowChildren(0)
    , m_insertionPointListIsValid(false)
{
}

void ShadowRootContentDistributionData::invalidateInsertionPointList()
{
    m_insertionPointListIsValid = false;
    m_insertionPointList.clear();
}

const Vector<RefPtr<InsertionPoint> >& ShadowRootContentDistributionData::ensureInsertionPointList(ShadowRoot* shadowRoot)
{
    if (m_insertionPointListIsValid)
        return m_insertionPointList;

    m_insertionPointListIsValid = true;
    ASSERT(m_insertionPointList.isEmpty());

    if (!shadowRoot->hasInsertionPoint())
        return m_insertionPointList;

    for (Element* element = ElementTraversal::firstWithin(shadowRoot); element; element = ElementTraversal::next(element, shadowRoot)) {
        if (element->isInsertionPoint())
            m_insertionPointList.append(toInsertionPoint(element));
    }

    return m_insertionPointList;
}

void ShadowRootContentDistributionData::regiterInsertionPoint(ShadowRoot* scope, InsertionPoint* point)
{
    switch (point->insertionPointType()) {
    case InsertionPoint::ShadowInsertionPoint:
        ++m_numberOfShadowElementChildren;
        break;
    case InsertionPoint::ContentInsertionPoint:
        ++m_numberOfContentElementChildren;
        scope->owner()->setShouldCollectSelectFeatureSet();
        break;
    }

    invalidateInsertionPointList();
}

void ShadowRootContentDistributionData::unregisterInsertionPoint(ShadowRoot* scope, InsertionPoint* point)
{
    switch (point->insertionPointType()) {
    case InsertionPoint::ShadowInsertionPoint:
        ASSERT(m_numberOfShadowElementChildren > 0);
        --m_numberOfShadowElementChildren;
        break;
    case InsertionPoint::ContentInsertionPoint:
        ASSERT(m_numberOfContentElementChildren > 0);
        --m_numberOfContentElementChildren;
        if (scope->owner())
            scope->owner()->setShouldCollectSelectFeatureSet();
        break;
    }

    invalidateInsertionPointList();
}


ContentDistributor::ContentDistributor()
    : m_validity(Undetermined)
{
}

ContentDistributor::~ContentDistributor()
{
}

InsertionPoint* ContentDistributor::findInsertionPointFor(const Node* key) const
{
    return m_nodeToInsertionPoint.get(key).get();
}

void ContentDistributor::populate(Node* node, ContentDistribution& pool)
{
    if (!isActiveInsertionPoint(node)) {
        pool.append(node);
        return;
    }

    InsertionPoint* insertionPoint = toInsertionPoint(node);
    if (insertionPoint->hasDistribution()) {
        for (size_t i = 0; i < insertionPoint->size(); ++i)
            populate(insertionPoint->at(i), pool);
    } else {
        for (Node* fallbackNode = insertionPoint->firstChild(); fallbackNode; fallbackNode = fallbackNode->nextSibling())
            pool.append(fallbackNode);
    }
}

void ContentDistributor::distribute(Element* host)
{
    ASSERT(needsDistribution());
    ASSERT(m_nodeToInsertionPoint.isEmpty());

    m_validity = Valid;

    ContentDistribution pool;
    for (Node* node = host->firstChild(); node; node = node->nextSibling())
        populate(node, pool);

    Vector<bool> distributed(pool.size());
    distributed.fill(false);

    Vector<HTMLShadowElement*, 8> activeShadowInsertionPoints;
    for (ShadowRoot* root = host->youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        HTMLShadowElement* firstActiveShadowInsertionPoint = 0;

        const Vector<RefPtr<InsertionPoint> >& insertionPoints = root->insertionPointList();
        for (size_t i = 0; i < insertionPoints.size(); ++i) {
            InsertionPoint* point = insertionPoints[i].get();
            if (!point->isActive())
                continue;

            if (isHTMLShadowElement(point)) {
                if (!firstActiveShadowInsertionPoint)
                    firstActiveShadowInsertionPoint = toHTMLShadowElement(point);
            } else {
                distributeSelectionsTo(point, pool, distributed);
                if (ElementShadow* shadow = point->parentNode()->isElementNode() ? toElement(point->parentNode())->shadow() : 0)
                    shadow->invalidateDistribution();
            }
        }

        if (firstActiveShadowInsertionPoint)
            activeShadowInsertionPoints.append(firstActiveShadowInsertionPoint);
    }

    for (size_t i = activeShadowInsertionPoints.size(); i > 0; --i) {
        HTMLShadowElement* shadowElement = activeShadowInsertionPoints[i - 1];
        ShadowRoot* root = shadowElement->containingShadowRoot();
        ASSERT(root);
        if (root->olderShadowRoot()) {
            distributeNodeChildrenTo(shadowElement, root->olderShadowRoot());
            root->olderShadowRoot()->setAssignedTo(shadowElement);
        } else {
            distributeSelectionsTo(shadowElement, pool, distributed);
            if (ElementShadow* shadow = shadowElement->parentNode()->isElementNode() ? toElement(shadowElement->parentNode())->shadow() : 0)
                shadow->invalidateDistribution();
        }
    }
}

bool ContentDistributor::invalidate(Element* host)
{
    ASSERT(needsInvalidation());
    bool needsReattach = (m_validity == Undetermined) || !m_nodeToInsertionPoint.isEmpty();

    for (ShadowRoot* root = host->youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        root->setAssignedTo(0);
        const Vector<RefPtr<InsertionPoint> >& insertionPoints = root->insertionPointList();
        for (size_t i = 0; i < insertionPoints.size(); ++i) {
            needsReattach = needsReattach || true;
            insertionPoints[i]->clearDistribution();
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

        if (!query.matches(pool.nodes(), i))
            continue;

        Node* child = pool.at(i).get();
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
        if (isActiveInsertionPoint(node)) {
            InsertionPoint* innerInsertionPoint = toInsertionPoint(node);
            if (innerInsertionPoint->hasDistribution()) {
                for (size_t i = 0; i < innerInsertionPoint->size(); ++i) {
                    distribution.append(innerInsertionPoint->at(i));
                    if (!m_nodeToInsertionPoint.contains(innerInsertionPoint->at(i)))
                        m_nodeToInsertionPoint.add(innerInsertionPoint->at(i), insertionPoint);
                }
            } else {
                for (Node* child = innerInsertionPoint->firstChild(); child; child = child->nextSibling()) {
                    distribution.append(child);
                    m_nodeToInsertionPoint.add(child, insertionPoint);
                }
            }
        } else {
            distribution.append(node);
            if (!m_nodeToInsertionPoint.contains(node))
                m_nodeToInsertionPoint.add(node, insertionPoint);
        }
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
