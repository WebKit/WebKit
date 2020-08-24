/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SimpleRange.h"

#include "CharacterData.h"
#include "NodeTraversal.h"
#include "ShadowRoot.h"

namespace WebCore {

SimpleRange::SimpleRange(const BoundaryPoint& start, const BoundaryPoint& end)
    : start(start)
    , end(end)
{
}

SimpleRange::SimpleRange(BoundaryPoint&& start, BoundaryPoint&& end)
    : start(WTFMove(start))
    , end(WTFMove(end))
{
}

bool operator==(const SimpleRange& a, const SimpleRange& b)
{
    return a.start == b.start && a.end == b.end;
}

// FIXME: Create BoundaryPoint.cpp and move this there.
Optional<BoundaryPoint> makeBoundaryPointBeforeNode(Node& node)
{
    auto parent = node.parentNode();
    if (!parent)
        return WTF::nullopt;
    return BoundaryPoint { *parent, node.computeNodeIndex() };
}

// FIXME: Create BoundaryPoint.cpp and move this there.
Optional<BoundaryPoint> makeBoundaryPointAfterNode(Node& node)
{
    auto parent = node.parentNode();
    if (!parent)
        return WTF::nullopt;
    return BoundaryPoint { *parent, node.computeNodeIndex() + 1 };
}

// FIXME: Create BoundaryPoint.cpp and move this there.
static bool isOffsetBeforeChild(ContainerNode& container, unsigned offset, Node& child)
{
    if (!offset)
        return true;
    unsigned currentOffset = 0;
    for (auto currentChild = container.firstChild(); currentChild && currentChild != &child; currentChild = currentChild->nextSibling()) {
        if (offset <= ++currentOffset)
            return true;
    }
    return false;
}

// FIXME: Create BoundaryPoint.cpp and move this there.
PartialOrdering documentOrder(const BoundaryPoint& a, const BoundaryPoint& b)
{
    if (a.offset == b.offset)
        return documentOrder(a.container, b.container);

    if (a.container.ptr() == b.container.ptr())
        return a.offset < b.offset ? PartialOrdering::less : PartialOrdering::greater;

    for (auto ancestor = b.container.ptr(); ancestor; ) {
        auto nextAncestor = ancestor->parentOrShadowHostNode();
        if (nextAncestor == a.container.ptr())
            return isOffsetBeforeChild(*nextAncestor, a.offset, *ancestor) ? PartialOrdering::less : PartialOrdering::greater;
        ancestor = nextAncestor;
    }

    for (auto ancestor = a.container.ptr(); ancestor; ) {
        auto nextAncestor = ancestor->parentOrShadowHostNode();
        if (nextAncestor == b.container.ptr())
            return isOffsetBeforeChild(*nextAncestor, b.offset, *ancestor) ? PartialOrdering::greater : PartialOrdering::less;
        ancestor = nextAncestor;
    }

    return documentOrder(a.container, b.container);
}

Optional<SimpleRange> makeRangeSelectingNode(Node& node)
{
    auto parent = node.parentNode();
    if (!parent)
        return WTF::nullopt;
    unsigned offset = node.computeNodeIndex();
    return SimpleRange { { *parent, offset }, { *parent, offset + 1 } };
}

SimpleRange makeRangeSelectingNodeContents(Node& node)
{
    return { makeBoundaryPointBeforeNodeContents(node), makeBoundaryPointAfterNodeContents(node) };
}

OffsetRange characterDataOffsetRange(const SimpleRange& range, const Node& node)
{
    return { &node == range.start.container.ptr() ? range.start.offset : 0,
        &node == range.end.container.ptr() ? range.end.offset : std::numeric_limits<unsigned>::max() };
}

static RefPtr<Node> firstIntersectingNode(const SimpleRange& range)
{
    if (range.start.container->isCharacterDataNode())
        return range.start.container.ptr();
    if (auto child = range.start.container->traverseToChildAt(range.start.offset))
        return child;
    return NodeTraversal::nextSkippingChildren(range.start.container);
}

static RefPtr<Node> nodePastLastIntersectingNode(const SimpleRange& range)
{
    if (range.end.container->isCharacterDataNode())
        return NodeTraversal::nextSkippingChildren(range.end.container);
    if (auto child = range.end.container->traverseToChildAt(range.end.offset))
        return child;
    return NodeTraversal::nextSkippingChildren(range.end.container);
}

IntersectingNodeIterator::IntersectingNodeIterator(const SimpleRange& range)
    : m_node(firstIntersectingNode(range))
    , m_pastLastNode(nodePastLastIntersectingNode(range))
{
    enforceEndInvariant();
}

void IntersectingNodeIterator::advance()
{
    ASSERT(m_node);
    m_node = NodeTraversal::next(*m_node);
    enforceEndInvariant();
}

void IntersectingNodeIterator::advanceSkippingChildren()
{
    ASSERT(m_node);
    m_node = m_node->contains(m_pastLastNode.get()) ? nullptr : NodeTraversal::nextSkippingChildren(*m_node);
    enforceEndInvariant();
}

void IntersectingNodeIterator::enforceEndInvariant()
{
    if (m_node == m_pastLastNode || !m_node) {
        m_node = nullptr;
        m_pastLastNode = nullptr;
    }
}

RefPtr<Node> commonInclusiveAncestor(const SimpleRange& range)
{
    return commonInclusiveAncestor(range.start.container, range.end.container);
}

}
