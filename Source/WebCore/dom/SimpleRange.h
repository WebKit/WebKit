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

#pragma once

#include "BoundaryPoint.h"

namespace WebCore {

struct SimpleRange {
    BoundaryPoint start;
    BoundaryPoint end;

    Node& startContainer() const { return start.container.get(); }
    unsigned startOffset() const { return start.offset; }
    Node& endContainer() const { return end.container.get(); }
    unsigned endOffset() const { return end.offset; }

    bool collapsed() const { return start == end; }

    WEBCORE_EXPORT SimpleRange(const BoundaryPoint&, const BoundaryPoint&);
    WEBCORE_EXPORT SimpleRange(BoundaryPoint&&, BoundaryPoint&&);
};

SimpleRange makeSimpleRangeHelper(BoundaryPoint&&, BoundaryPoint&&);
Optional<SimpleRange> makeSimpleRangeHelper(Optional<BoundaryPoint>&&, Optional<BoundaryPoint>&&);
SimpleRange makeSimpleRangeHelper(BoundaryPoint&&);
Optional<SimpleRange> makeSimpleRangeHelper(Optional<BoundaryPoint>&&);

inline BoundaryPoint makeBoundaryPointHelper(const BoundaryPoint& point) { return point; }
inline BoundaryPoint makeBoundaryPointHelper(BoundaryPoint&& point) { return WTFMove(point); }
inline Optional<BoundaryPoint> makeBoundaryPointHelper(const Optional<BoundaryPoint>& point) { return point; }
inline Optional<BoundaryPoint> makeBoundaryPointHelper(Optional<BoundaryPoint>&& point) { return WTFMove(point); }
template<typename T> auto makeBoundaryPointHelper(T&& argument) -> decltype(makeBoundaryPoint(std::forward<T>(argument))) { return makeBoundaryPoint(std::forward<T>(argument)); }

template<typename ...T> auto makeSimpleRange(T&& ...arguments) -> decltype(makeSimpleRangeHelper(makeBoundaryPointHelper(std::forward<T>(arguments))...)) { return makeSimpleRangeHelper(makeBoundaryPointHelper(std::forward<T>(arguments))...); }

// FIXME: Would like these to have shorter names; another option is to change prefix to makeSimpleRange.
WEBCORE_EXPORT Optional<SimpleRange> makeRangeSelectingNode(Node&);
WEBCORE_EXPORT SimpleRange makeRangeSelectingNodeContents(Node&);

WEBCORE_EXPORT RefPtr<Node> commonInclusiveAncestor(const SimpleRange&);

bool operator==(const SimpleRange&, const SimpleRange&);

WEBCORE_EXPORT bool isPointInRange(const SimpleRange&, const BoundaryPoint&);
bool isPointInRange(const SimpleRange&, const Optional<BoundaryPoint>&);

WEBCORE_EXPORT bool contains(const SimpleRange& outerRange, const SimpleRange& innerRange);
WEBCORE_EXPORT bool intersects(const SimpleRange&, const SimpleRange&);
WEBCORE_EXPORT SimpleRange unionRange(const SimpleRange&, const SimpleRange&);
WEBCORE_EXPORT Optional<SimpleRange> intersection(const Optional<SimpleRange>&, const Optional<SimpleRange>&);

WEBCORE_EXPORT bool contains(const SimpleRange&, const Node&);
WEBCORE_EXPORT bool intersects(const SimpleRange&, const Node&);

// Returns equivalent if point is in range.
WEBCORE_EXPORT PartialOrdering documentOrder(const SimpleRange&, const BoundaryPoint&);
WEBCORE_EXPORT PartialOrdering documentOrder(const BoundaryPoint&, const SimpleRange&);

class IntersectingNodeRange;
IntersectingNodeRange intersectingNodes(const SimpleRange&);

class IntersectingNodeRangeWithQuirk;
IntersectingNodeRangeWithQuirk intersectingNodesWithDeprecatedZeroOffsetStartQuirk(const SimpleRange&);

struct OffsetRange {
    unsigned start { 0 };
    unsigned end { 0 };
};
OffsetRange characterDataOffsetRange(const SimpleRange&, const Node&);

class IntersectingNodeIterator : public std::iterator<std::forward_iterator_tag, Node> {
public:
    IntersectingNodeIterator(const SimpleRange&);

    enum QuirkFlag { DeprecatedZeroOffsetStartQuirk };
    IntersectingNodeIterator(const SimpleRange&, QuirkFlag);

    Node& operator*() const { return *m_node; }
    Node* operator->() const { ASSERT(m_node); return m_node.get(); }

    operator bool() const { return m_node; }
    bool operator!() const { return !m_node; }
    bool operator!=(const std::nullptr_t) const { return m_node; }

    IntersectingNodeIterator& operator++() { advance(); return *this; }
    void advance();
    void advanceSkippingChildren();

private:
    void enforceEndInvariant();

    RefPtr<Node> m_node;
    RefPtr<Node> m_pastLastNode;
};

class IntersectingNodeRange {
public:
    IntersectingNodeRange(const SimpleRange&);

    IntersectingNodeIterator begin() const { return m_range; }
    static constexpr std::nullptr_t end() { return nullptr; }

private:
    SimpleRange m_range;
};

class IntersectingNodeRangeWithQuirk {
public:
    IntersectingNodeRangeWithQuirk(const SimpleRange&);

    IntersectingNodeIterator begin() const { return { m_range, IntersectingNodeIterator::DeprecatedZeroOffsetStartQuirk }; }
    static constexpr std::nullptr_t end() { return nullptr; }

private:
    SimpleRange m_range;
};

inline IntersectingNodeRange::IntersectingNodeRange(const SimpleRange& range)
    : m_range(range)
{
}

inline IntersectingNodeRangeWithQuirk::IntersectingNodeRangeWithQuirk(const SimpleRange& range)
    : m_range(range)
{
}

inline IntersectingNodeRange intersectingNodes(const SimpleRange& range)
{
    return { range };
}

inline IntersectingNodeRangeWithQuirk intersectingNodesWithDeprecatedZeroOffsetStartQuirk(const SimpleRange& range)
{
    return { range };
}

inline SimpleRange makeSimpleRangeHelper(BoundaryPoint&& start, BoundaryPoint&& end)
{
    return { WTFMove(start), WTFMove(end) };
}

inline Optional<SimpleRange> makeSimpleRangeHelper(Optional<BoundaryPoint>&& start, Optional<BoundaryPoint>&& end)
{
    if (!start || !end)
        return WTF::nullopt;
    return makeSimpleRangeHelper(WTFMove(*start), WTFMove(*end));
}

inline SimpleRange makeSimpleRangeHelper(BoundaryPoint&& point)
{
    auto end = point;
    return makeSimpleRangeHelper(WTFMove(point), WTFMove(end));
}

inline Optional<SimpleRange> makeSimpleRangeHelper(Optional<BoundaryPoint>&& point)
{
    if (!point)
        return WTF::nullopt;
    return makeSimpleRangeHelper(WTFMove(*point));
}

}
