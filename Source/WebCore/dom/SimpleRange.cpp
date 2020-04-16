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
#include "Range.h"

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

SimpleRange::SimpleRange(const Range& other)
    : start(other.startContainer(), other.startOffset())
    , end(other.endContainer(), other.endOffset())
{
}

bool operator==(const SimpleRange& a, const SimpleRange& b)
{
    return a.start == b.start && a.end == b.end;
}

static BoundaryPoint makeBoundaryPointAfterNodeContents(Node& node)
{
    return { node, node.length() };
}

SimpleRange makeRangeSelectingNodeContents(Node& node)
{
    return { makeBoundaryPointBeforeNodeContents(node), makeBoundaryPointAfterNodeContents(node) };
}

RefPtr<Node> IntersectingNodeRange::first() const
{
    if (m_range.start.container->isCharacterDataNode())
        return m_range.start.container.ptr();
    if (auto child = m_range.start.container->traverseToChildAt(m_range.start.offset))
        return child;
    return NodeTraversal::nextSkippingChildren(m_range.start.container);
}

RefPtr<Node> IntersectingNodeRange::sentinel() const
{
    if (m_range.end.container->isCharacterDataNode())
        return NodeTraversal::nextSkippingChildren(m_range.end.container);
    if (auto child = m_range.end.container->traverseToChildAt(m_range.end.offset))
        return child;
    return NodeTraversal::nextSkippingChildren(m_range.end.container);
}

OffsetRange characterDataOffsetRange(const SimpleRange& range, const Node& node)
{
    return { &node == range.start.container.ptr() ? range.start.offset : 0,
        &node == range.end.container.ptr() ? range.end.offset : std::numeric_limits<unsigned>::max() };
}

IntersectingNodeIterator& IntersectingNodeIterator::operator++()
{
    ASSERT(m_node);
    m_node = NodeTraversal::next(*m_node);
    return *this;
}

}
