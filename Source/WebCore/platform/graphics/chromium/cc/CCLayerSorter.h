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

#include "CCLayerImpl.h"
#include "FloatPoint3D.h"
#include "FloatQuad.h"
#include "FloatRect.h"
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace WebKit {
class WebTransformationMatrix;
}

namespace WebCore {

class CCLayerSorter {
    WTF_MAKE_NONCOPYABLE(CCLayerSorter);
public:
    CCLayerSorter() : m_zRange(0) { }

    typedef Vector<CCLayerImpl*> LayerList;

    void sort(LayerList::iterator first, LayerList::iterator last);

    // Holds various useful properties derived from a layer's 3D outline.
    struct LayerShape {
        LayerShape() { }
        LayerShape(float width, float height, const WebKit::WebTransformationMatrix& drawTransform);

        float layerZFromProjectedPoint(const FloatPoint&) const;

        FloatPoint3D layerNormal;
        FloatPoint3D transformOrigin;
        FloatQuad projectedQuad;
        FloatRect projectedBounds;
    };

    enum ABCompareResult {
        ABeforeB,
        BBeforeA,
        None
    };

    static ABCompareResult checkOverlap(LayerShape*, LayerShape*, float zThreshold, float& weight);

private:
    struct GraphEdge;

    struct GraphNode {
        explicit GraphNode(CCLayerImpl* cclayer) : layer(cclayer), incomingEdgeWeight(0) { }

        CCLayerImpl* layer;
        LayerShape shape;
        Vector<GraphEdge*> incoming;
        Vector<GraphEdge*> outgoing;
        float incomingEdgeWeight;
    };

    struct GraphEdge {
        GraphEdge(GraphNode* fromNode, GraphNode* toNode, float weight) : from(fromNode), to(toNode), weight(weight) { };

        GraphNode* from;
        GraphNode* to;
        float weight;
    };

    typedef Vector<GraphNode> NodeList;
    typedef Vector<GraphEdge> EdgeList;
    NodeList m_nodes;
    EdgeList m_edges;
    float m_zRange;

    typedef HashMap<GraphEdge*, GraphEdge*> EdgeMap;
    EdgeMap m_activeEdges;

    void createGraphNodes(LayerList::iterator first, LayerList::iterator last);
    void createGraphEdges();
    void removeEdgeFromList(GraphEdge*, Vector<GraphEdge*>&);
};

}
#endif
