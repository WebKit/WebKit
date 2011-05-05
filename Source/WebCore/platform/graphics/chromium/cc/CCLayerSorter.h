/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CCLayerSorter_h
#define CCLayerSorter_h

#include "FloatPoint3D.h"
#include "FloatRect.h"
#include "cc/CCLayerImpl.h"
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace WebCore {

class CCLayerSorter {
    WTF_MAKE_NONCOPYABLE(CCLayerSorter);
public:
    CCLayerSorter();

    typedef Vector<RefPtr<CCLayerImpl> > LayerList;

    void sort(LayerList::iterator first, LayerList::iterator last);
private:
    struct GraphEdge;

    struct GraphNode {
        GraphNode(CCLayerImpl* cclayer) : layer(cclayer) { };
        CCLayerImpl* layer;
        FloatPoint c1, c2, c3, c4;
        FloatPoint3D normal;
        FloatPoint3D origin;
        FloatRect boundingBox;
        Vector<GraphEdge*> incoming;
        Vector<GraphEdge*> outgoing;
    };

    struct GraphEdge {
        GraphEdge(GraphNode* fromNode, GraphNode* toNode) : from(fromNode), to(toNode) { };

        GraphNode* from;
        GraphNode* to;
    };

    struct LayerIntersector {
        LayerIntersector(GraphNode*, GraphNode*, float);

        void go();

        float layerZFromProjectedPoint(GraphNode*, const FloatPoint&);
        bool triangleTriangleTest(const FloatPoint&, const FloatPoint&, const FloatPoint&, const FloatPoint&, const FloatPoint&, const FloatPoint&);
        bool edgeTriangleTest(const FloatPoint&, const FloatPoint&, const FloatPoint&, const FloatPoint&, const FloatPoint&);
        bool checkZDiff(const FloatPoint&);

        FloatPoint intersectionPoint;
        GraphNode* nodeA;
        GraphNode* nodeB;
        float zDiff;
        float earlyExitThreshold;
    };

    typedef Vector<GraphNode> NodeList;
    typedef Vector<GraphEdge> EdgeList;
    NodeList m_nodes;
    EdgeList m_edges;
    float m_zRange;

    typedef HashMap<GraphEdge*, GraphEdge*> EdgeMap;
    EdgeMap m_activeEdges;

    float m_zDiffEpsilon;

    void createGraphNodes(LayerList::iterator first, LayerList::iterator last);
    void createGraphEdges();
    void removeEdgeFromList(GraphEdge*, Vector<GraphEdge*>&);

    enum ABCompareResult {
        ABeforeB,
        BBeforeA,
        None
    };
    ABCompareResult checkOverlap(GraphNode*, GraphNode*);
};

}
#endif
