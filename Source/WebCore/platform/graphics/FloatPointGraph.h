/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include <WebCore/FloatPoint.h>
#include <memory>
#include <wtf/Vector.h>

namespace WebCore {

class FloatPointGraph {
    WTF_MAKE_NONCOPYABLE(FloatPointGraph);
public:
    FloatPointGraph() = default;
    FloatPointGraph(FloatPointGraph&&) = default;
    FloatPointGraph& operator=(FloatPointGraph&&) = default;

    class Node : public FloatPoint {
        WTF_MAKE_TZONE_ALLOCATED(Node);
        WTF_MAKE_NONCOPYABLE(Node);
    public:
        Node(FloatPoint point)
            : FloatPoint(point)
        {
        }

        const Vector<Node*>& nextPoints() const { return m_nextPoints; }

        void addNextPoint(Node* node)
        {
            if (!m_nextPoints.contains(node))
                m_nextPoints.append(node);
        }

        bool isVisited() const { return m_visited; }
        void visit() { m_visited = true; }

        void reset() { m_visited = false; m_nextPoints.clear(); }

    private:
        Vector<Node*> m_nextPoints;
        bool m_visited { false };
    };

    using Edge = std::pair<Node*, Node*>;
    using Polygon = Vector<Edge>;

    static std::pair<FloatPointGraph, Vector<Polygon>> polygonsForRect(const Vector<FloatRect>&);

    Node* findOrCreateNode(FloatPoint);
    void reset();

private:
    Vector<std::unique_ptr<Node>> m_allNodes;
};

} // namespace WebCore
