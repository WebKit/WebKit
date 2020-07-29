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

class Range;

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

    // Convenience overloads to help with transition from using a lot of live ranges.
    // FIXME: Once transition is over, remove and change callers to use makeSimpleRange instead.
    WEBCORE_EXPORT SimpleRange(const Range&);
    SimpleRange(const Ref<Range>&);
};

SimpleRange makeSimpleRange(const BoundaryPoint&);
SimpleRange makeSimpleRange(BoundaryPoint&&);
SimpleRange makeSimpleRange(const BoundaryPoint&, const BoundaryPoint&);
SimpleRange makeSimpleRange(BoundaryPoint&&, BoundaryPoint&&);
Optional<SimpleRange> makeSimpleRange(const Optional<BoundaryPoint>&);
WEBCORE_EXPORT Optional<SimpleRange> makeSimpleRange(Optional<BoundaryPoint>&&);
WEBCORE_EXPORT Optional<SimpleRange> makeSimpleRange(const Optional<BoundaryPoint>&, const Optional<BoundaryPoint>&);
WEBCORE_EXPORT Optional<SimpleRange> makeSimpleRange(Optional<BoundaryPoint>&&, Optional<BoundaryPoint>&&);

inline BoundaryPoint makeBoundaryPointHelper(const BoundaryPoint& point) { return point; }
inline BoundaryPoint makeBoundaryPointHelper(BoundaryPoint&& point) { return WTFMove(point); }
template<typename T> auto makeBoundaryPointHelper(T&& argument) -> decltype(makeBoundaryPoint(std::forward<T>(argument)))
{
    return makeBoundaryPoint(std::forward<T>(argument));
}

template<typename ...T> auto makeSimpleRange(T&& ...arguments) -> decltype(makeSimpleRange(makeBoundaryPointHelper(std::forward<T>(arguments))...))
{
    return makeSimpleRange(makeBoundaryPointHelper(std::forward<T>(arguments))...);
}

// FIXME: Would like these to have shorter names; another option is to change prefix to makeSimpleRange.
WEBCORE_EXPORT Optional<SimpleRange> makeRangeSelectingNode(Node&);
WEBCORE_EXPORT SimpleRange makeRangeSelectingNodeContents(Node&);

RefPtr<Node> commonInclusiveAncestor(const SimpleRange&);

bool operator==(const SimpleRange&, const SimpleRange&);

class IntersectingNodeRange;
IntersectingNodeRange intersectingNodes(const SimpleRange&);

struct OffsetRange {
    unsigned start { 0 };
    unsigned end { 0 };
};
OffsetRange characterDataOffsetRange(const SimpleRange&, const Node&);

class IntersectingNodeIterator : public std::iterator<std::forward_iterator_tag, Node> {
public:
    IntersectingNodeIterator(const SimpleRange&);

    Node& operator*() const { return *m_node; }
    Node* operator->() const { ASSERT(m_node); return m_node.get(); }

    operator bool() const { return m_node; }
    bool operator!() const { return !m_node; }
    bool operator!=(const std::nullptr_t) const { return m_node; }

    IntersectingNodeIterator& operator++() { advance(); return *this; }
    void advance();
    void advanceSkippingChildren();

private:
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

inline IntersectingNodeRange::IntersectingNodeRange(const SimpleRange& range)
    : m_range(range)
{
}

inline IntersectingNodeRange intersectingNodes(const SimpleRange& range)
{
    return { range };
}

inline SimpleRange::SimpleRange(const Ref<Range>& range)
    : SimpleRange(range.get())
{
}

}
