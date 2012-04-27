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

#include "config.h"

#include "cc/CCLayerSorter.h"

#include "TransformationMatrix.h"
#include "cc/CCMathUtil.h"
#include "cc/CCRenderSurface.h"
#include <limits.h>
#include <wtf/Deque.h>

using namespace std;

#define LOG_CHANNEL_PREFIX Log
#define SHOW_DEBUG_LOG 0

#if !defined( NDEBUG )
#if SHOW_DEBUG_LOG
static WTFLogChannel LogCCLayerSorter = { 0x00000000, "", WTFLogChannelOn };
#else
static WTFLogChannel LogCCLayerSorter = { 0x00000000, "", WTFLogChannelOff };
#endif
#endif

namespace WebCore {

inline static float perpProduct(const FloatSize& u, const FloatSize& v)
{
    return u.width() * v.height() - u.height() * v.width();
}

// Tests if two edges defined by their endpoints (a,b) and (c,d) intersect. Returns true and the
// point of intersection if they do and false otherwise.
static bool edgeEdgeTest(const FloatPoint& a, const FloatPoint& b, const FloatPoint& c, const FloatPoint& d, FloatPoint& r)
{
    FloatSize u = b - a;
    FloatSize v = d - c;
    FloatSize w = a - c;

    float denom = perpProduct(u, v);

    // If denom == 0 then the edges are parallel. While they could be overlapping
    // we don't bother to check here as the we'll find their intersections from the
    // corner to quad tests.
    if (!denom)
        return false;

    float s = perpProduct(v, w) / denom;
    if (s < 0 || s > 1)
        return false;

    float t = perpProduct(u, w) / denom;
    if (t < 0 || t > 1)
        return false;

    u.scale(s);
    r = a + u;
    return true;
}

// Checks whether layer "a" draws on top of layer "b". The weight value returned is an indication of
// the maximum z-depth difference between the layers or zero if the layers are found to be intesecting
// (some features are in front and some are behind).
CCLayerSorter::ABCompareResult CCLayerSorter::checkOverlap(LayerShape* a, LayerShape* b, float zThreshold, float& weight)
{
    weight = 0;

    // Early out if the projected bounds don't overlap.
    if (!a->projectedBounds.intersects(b->projectedBounds))
        return None;

    FloatPoint aPoints[4] = {a->projectedQuad.p1(), a->projectedQuad.p2(), a->projectedQuad.p3(), a->projectedQuad.p4() };
    FloatPoint bPoints[4] = {b->projectedQuad.p1(), b->projectedQuad.p2(), b->projectedQuad.p3(), b->projectedQuad.p4() };

    // Make a list of points that inside both layer quad projections.
    Vector<FloatPoint> overlapPoints;

    // Check all four corners of one layer against the other layer's quad.
    for (int i = 0; i < 4; ++i) {
        if (a->projectedQuad.containsPoint(bPoints[i]))
            overlapPoints.append(bPoints[i]);
        if (b->projectedQuad.containsPoint(aPoints[i]))
            overlapPoints.append(aPoints[i]);
    }

    // Check all the edges of one layer for intersection with the other layer's edges.
    FloatPoint r;
    for (int ea = 0; ea < 4; ++ea)
        for (int eb = 0; eb < 4; ++eb)
            if (edgeEdgeTest(aPoints[ea], aPoints[(ea + 1) % 4],
                             bPoints[eb], bPoints[(eb + 1) % 4],
                             r))
                overlapPoints.append(r);

    if (!overlapPoints.size())
        return None;

    // Check the corresponding layer depth value for all overlap points to determine
    // which layer is in front.
    float maxPositive = 0;
    float maxNegative = 0;
    for (unsigned o = 0; o < overlapPoints.size(); o++) {
        float za = a->layerZFromProjectedPoint(overlapPoints[o]);
        float zb = b->layerZFromProjectedPoint(overlapPoints[o]);

        float diff = za - zb;
        if (diff > maxPositive)
            maxPositive = diff;
        if (diff < maxNegative)
            maxNegative = diff;
    }

    float maxDiff = (fabsf(maxPositive) > fabsf(maxNegative) ? maxPositive : maxNegative);

    // If the results are inconsistent (and the z difference substantial to rule out
    // numerical errors) then the layers are intersecting. We will still return an
    // order based on the maximum depth difference but with an edge weight of zero
    // these layers will get priority if a graph cycle is present and needs to be broken.
    if (maxPositive > zThreshold && maxNegative < -zThreshold)
        weight = 0;
    else
        weight = fabsf(maxDiff);

    // Maintain relative order if the layers have the same depth at all intersection points.
    if (maxDiff <= 0)
        return ABeforeB;

    return BBeforeA;
}

CCLayerSorter::LayerShape::LayerShape(float width, float height, const TransformationMatrix& drawTransform)
{
    FloatQuad layerQuad(FloatPoint(-width * 0.5, height * 0.5),
                        FloatPoint(width * 0.5, height * 0.5),
                        FloatPoint(width * 0.5, -height * 0.5),
                        FloatPoint(-width * 0.5, -height * 0.5));

    // Compute the projection of the layer quad onto the z = 0 plane.

    FloatPoint clippedQuad[8];
    int numVerticesInClippedQuad;
    CCMathUtil::mapClippedQuad(drawTransform, layerQuad, clippedQuad, numVerticesInClippedQuad);

    if (numVerticesInClippedQuad < 3) {
        projectedBounds = FloatRect();
        return;
    }

    projectedBounds = CCMathUtil::computeEnclosingRectOfVertices(clippedQuad, numVerticesInClippedQuad);

    // NOTE: it will require very significant refactoring and overhead to deal with
    // generalized polygons or multiple quads per layer here. For the sake of layer
    // sorting it is equally correct to take a subsection of the polygon that can be made
    // into a quad. This will only be incorrect in the case of intersecting layers, which
    // are not supported yet anyway.
    projectedQuad.setP1(clippedQuad[0]);
    projectedQuad.setP2(clippedQuad[1]);
    projectedQuad.setP3(clippedQuad[2]);
    if (numVerticesInClippedQuad >= 4)
        projectedQuad.setP4(clippedQuad[3]);
    else
        projectedQuad.setP4(clippedQuad[2]); // this will be a degenerate quad that is actually a triangle.

    // Compute the normal of the layer's plane.
    FloatPoint3D c1 = drawTransform.mapPoint(FloatPoint3D(0, 0, 0));
    FloatPoint3D c2 = drawTransform.mapPoint(FloatPoint3D(0, 1, 0));
    FloatPoint3D c3 = drawTransform.mapPoint(FloatPoint3D(1, 0, 0));
    FloatPoint3D c12 = c2 - c1;
    FloatPoint3D c13 = c3 - c1;
    layerNormal = c13.cross(c12);

    transformOrigin = c1;
}

// Returns the Z coordinate of a point on the layer that projects
// to point p which lies on the z = 0 plane. It does it by computing the
// intersection of a line starting from p along the Z axis and the plane
// of the layer.
float CCLayerSorter::LayerShape::layerZFromProjectedPoint(const FloatPoint& p) const
{
    const FloatPoint3D zAxis(0, 0, 1);
    FloatPoint3D w = FloatPoint3D(p) - transformOrigin;

    float d = layerNormal.dot(zAxis);
    float n = -layerNormal.dot(w);

    // Check if layer is parallel to the z = 0 axis which will make it
    // invisible and hence returning zero is fine.
    if (!d)
        return 0;

    // The intersection point would be given by:
    // p + (n / d) * u  but since we are only interested in the 
    // z coordinate and p's z coord is zero, all we need is the value of n/d.
    return n / d;
}

void CCLayerSorter::createGraphNodes(LayerList::iterator first, LayerList::iterator last)
{
#if !defined( NDEBUG )
    LOG(CCLayerSorter, "Creating graph nodes:\n");
#endif
    float minZ = FLT_MAX;
    float maxZ = -FLT_MAX;
    for (LayerList::const_iterator it = first; it < last; it++) {
        m_nodes.append(GraphNode(*it));
        GraphNode& node = m_nodes.at(m_nodes.size() - 1);
        CCRenderSurface* renderSurface = node.layer->renderSurface();
        if (!node.layer->drawsContent() && !renderSurface)
            continue;

#if !defined( NDEBUG )
        LOG(CCLayerSorter, "Layer %d (%d x %d)\n", node.layer->id(), node.layer->bounds().width(), node.layer->bounds().height());
#endif

        TransformationMatrix drawTransform;
        float layerWidth, layerHeight;
        if (renderSurface) {
            drawTransform = renderSurface->drawTransform();
            layerWidth = renderSurface->contentRect().width();
            layerHeight = renderSurface->contentRect().height();
        } else {
            drawTransform = node.layer->drawTransform();
            layerWidth = node.layer->bounds().width();
            layerHeight = node.layer->bounds().height();
        }

        node.shape = LayerShape(layerWidth, layerHeight, drawTransform);

        maxZ = max(maxZ, node.shape.transformOrigin.z());
        minZ = min(minZ, node.shape.transformOrigin.z());
    }

    m_zRange = fabsf(maxZ - minZ);
}

void CCLayerSorter::createGraphEdges()
{
#if !defined( NDEBUG )
    LOG(CCLayerSorter, "Edges:\n");
#endif
    // Fraction of the total zRange below which z differences
    // are not considered reliable.
    const float zThresholdFactor = 0.01;
    float zThreshold = m_zRange * zThresholdFactor;

    for (unsigned na = 0; na < m_nodes.size(); na++) {
        GraphNode& nodeA = m_nodes[na];
        if (!nodeA.layer->drawsContent() && !nodeA.layer->renderSurface())
            continue;
        for (unsigned nb = na + 1; nb < m_nodes.size(); nb++) {
            GraphNode& nodeB = m_nodes[nb];
            if (!nodeB.layer->drawsContent() && !nodeB.layer->renderSurface())
                continue;
            float weight = 0;
            ABCompareResult overlapResult = checkOverlap(&nodeA.shape, &nodeB.shape, zThreshold, weight);
            GraphNode* startNode = 0;
            GraphNode* endNode = 0;
            if (overlapResult == ABeforeB) {
                startNode = &nodeA;
                endNode = &nodeB;
            } else if (overlapResult == BBeforeA) {
                startNode = &nodeB;
                endNode = &nodeA;
            }

            if (startNode) {
#if !defined( NDEBUG )
                LOG(CCLayerSorter, "%d -> %d\n", startNode->layer->id(), endNode->layer->id());
#endif
                m_edges.append(GraphEdge(startNode, endNode, weight));
            }
        }
    }

    for (unsigned i = 0; i < m_edges.size(); i++) {
        GraphEdge& edge = m_edges[i];
        m_activeEdges.add(&edge, &edge);
        edge.from->outgoing.append(&edge);
        edge.to->incoming.append(&edge);
        edge.to->incomingEdgeWeight += edge.weight;
    }
}

// Finds and removes an edge from the list by doing a swap with the
// last element of the list.
void CCLayerSorter::removeEdgeFromList(GraphEdge* edge, Vector<GraphEdge*>& list)
{
    size_t edgeIndex = list.find(edge);
    ASSERT(edgeIndex != notFound);
    if (list.size() == 1) {
        ASSERT(!edgeIndex);
        list.clear();
        return;
    }
    if (edgeIndex != list.size() - 1)
        list[edgeIndex] = list[list.size() - 1];

    list.removeLast();
}

// Sorts the given list of layers such that they can be painted in a back-to-front
// order. Sorting produces correct results for non-intersecting layers that don't have
// cyclical order dependencies. Cycles and intersections are broken (somewhat) aribtrarily.
// Sorting of layers is done via a topological sort of a directed graph whose nodes are
// the layers themselves. An edge from node A to node B signifies that layer A needs to
// be drawn before layer B. If A and B have no dependency between each other, then we
// preserve the ordering of those layers as they were in the original list.
//
// The draw order between two layers is determined by projecting the two triangles making
// up each layer quad to the Z = 0 plane, finding points of intersection between the triangles
// and backprojecting those points to the plane of the layer to determine the corresponding Z
// coordinate. The layer with the lower Z coordinate (farther from the eye) needs to be rendered
// first.
//
// If the layer projections don't intersect, then no edges (dependencies) are created
// between them in the graph. HOWEVER, in this case we still need to preserve the ordering
// of the original list of layers, since that list should already have proper z-index
// ordering of layers.
//
void CCLayerSorter::sort(LayerList::iterator first, LayerList::iterator last)
{
#if !defined( NDEBUG )
    LOG(CCLayerSorter, "Sorting start ----\n");
#endif
    createGraphNodes(first, last);

    createGraphEdges();

    Vector<GraphNode*> sortedList;
    Deque<GraphNode*> noIncomingEdgeNodeList;

    // Find all the nodes that don't have incoming edges.
    for (NodeList::iterator la = m_nodes.begin(); la < m_nodes.end(); la++) {
        if (!la->incoming.size())
            noIncomingEdgeNodeList.append(la);
    }

#if !defined( NDEBUG )
    LOG(CCLayerSorter, "Sorted list: ");
#endif
    while (m_activeEdges.size() || noIncomingEdgeNodeList.size()) {
        while (noIncomingEdgeNodeList.size()) {

            // It is necessary to preserve the existing ordering of layers, when there are
            // no explicit dependencies (because this existing ordering has correct
            // z-index/layout ordering). To preserve this ordering, we process Nodes in
            // the same order that they were added to the list.
            GraphNode* fromNode = noIncomingEdgeNodeList.takeFirst();

            // Add it to the final list.
            sortedList.append(fromNode);

#if !defined( NDEBUG )
            LOG(CCLayerSorter, "%d, ", fromNode->layer->id());
#endif

            // Remove all its outgoing edges from the graph.
            for (unsigned i = 0; i < fromNode->outgoing.size(); i++) {
                GraphEdge* outgoingEdge = fromNode->outgoing[i];

                m_activeEdges.remove(outgoingEdge);
                removeEdgeFromList(outgoingEdge, outgoingEdge->to->incoming);
                outgoingEdge->to->incomingEdgeWeight -= outgoingEdge->weight;

                if (!outgoingEdge->to->incoming.size())
                    noIncomingEdgeNodeList.append(outgoingEdge->to);
            }
            fromNode->outgoing.clear();
        }

        if (!m_activeEdges.size())
            break;

        // If there are still active edges but the list of nodes without incoming edges
        // is empty then we have run into a cycle. Break the cycle by finding the node
        // with the smallest overall incoming edge weight and use it. This will favor
        // nodes that have zero-weight incoming edges i.e. layers that are being
        // occluded by a layer that intersects them.
        float minIncomingEdgeWeight = FLT_MAX;
        GraphNode* nextNode = 0;
        for (unsigned i = 0; i < m_nodes.size(); i++) {
            if (m_nodes[i].incoming.size() && m_nodes[i].incomingEdgeWeight < minIncomingEdgeWeight) {
                minIncomingEdgeWeight = m_nodes[i].incomingEdgeWeight;
                nextNode = &m_nodes[i];
            }
        }
        ASSERT(nextNode);
        // Remove all its incoming edges.
        for (unsigned e = 0; e < nextNode->incoming.size(); e++) {
            GraphEdge* incomingEdge = nextNode->incoming[e];

            m_activeEdges.remove(incomingEdge);
            removeEdgeFromList(incomingEdge, incomingEdge->from->outgoing);
        }
        nextNode->incoming.clear();
        nextNode->incomingEdgeWeight = 0;
        noIncomingEdgeNodeList.append(nextNode);
#if !defined( NDEBUG )
        LOG(CCLayerSorter, "Breaking cycle by cleaning up incoming edges from %d (weight = %f)\n", nextNode->layer->id(), minIncomingEdgeWeight);
#endif
    }

    // Note: The original elements of the list are in no danger of having their ref count go to zero
    // here as they are all nodes of the layer hierarchy and are kept alive by their parent nodes.
    int count = 0;
    for (LayerList::iterator it = first; it < last; it++)
        *it = sortedList[count++]->layer;

#if !defined( NDEBUG )
    LOG(CCLayerSorter, "Sorting end ----\n");
#endif

    m_nodes.clear();
    m_edges.clear();
    m_activeEdges.clear();
}

}
