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

bool CCLayerSorter::pointInTriangle(const FloatPoint& point,
                                    const FloatPoint& a,
                                    const FloatPoint& b,
                                    const FloatPoint& c)
{
    // Algorithm from http://www.blackpawn.com/texts/pointinpoly/default.html
    float x0 = c.x() - a.x();
    float y0 = c.y() - a.y();
    float x1 = b.x() - a.x();
    float y1 = b.y() - a.y();
    float x2 = point.x() - a.x();
    float y2 = point.y() - a.y();

    float dot00 = x0 * x0 + y0 * y0;
    float dot01 = x0 * x1 + y0 * y1;
    float dot02 = x0 * x2 + y0 * y2;
    float dot11 = x1 * x1 + y1 * y1;
    float dot12 = x1 * x2 + y1 * y2;
    float denominator = dot00 * dot11 - dot01 * dot01;
    if (!denominator)
        // Triangle is zero-area. Treat query point as not being inside.
        return false;
    // Compute
    float inverseDenominator = 1.0f / denominator;
    float u = (dot11 * dot02 - dot01 * dot12) * inverseDenominator;
    float v = (dot00 * dot12 - dot01 * dot02) * inverseDenominator;

    return (u > 0.0f) && (v > 0.0f) && (u + v < 1.0f);
}

inline static float perpProduct(const FloatSize& u, const FloatSize& v)
{
    return u.width() * v.height() - u.height() * v.width();
}

inline static float innerProduct(const FloatSize& u, const FloatSize& v)
{
    return u.width() * v.width() + u.height() * v.height();
}


inline static bool pointInColinearEdge(const FloatPoint& p, const FloatPoint& a, const FloatPoint& b)
{
    // if ab is not vertical.
    if (a.x() != b.x())
        return (p.x() >= min(a.x(), b.x()) && p.x() <= max(a.x(), b.x()));

    return (p.y() >= min(a.y(), b.y()) && p.y() <= max(a.y(), b.y()));
}

// Tests if two edges defined by their endpoints (a,b) and (c,d) intersect. Returns true and the
// point of intersection if they do and false otherwise.
static bool edgeEdgeTest(const FloatPoint& a, const FloatPoint& b, const FloatPoint& c, const FloatPoint& d, FloatPoint& r)
{
    FloatSize u = b - a;
    FloatSize v = d - c;
    FloatSize w = a - c;

    float denom = perpProduct(u, v);

    // If denom == 0 then the edges are parallel.
    if (!denom) {
        // If the edges are not colinear then there's no intersection.
        if (perpProduct(u, w) || perpProduct(v, w))
            return false;

        if (pointInColinearEdge(a, c, d)) {
            r = a;
            return true;
        }
        if (pointInColinearEdge(b, c, d)) {
            r = b;
            return true;
        }
        if (pointInColinearEdge(c, a, b)) {
            r = c;
            return true;
        }
        if (pointInColinearEdge(d, a, b)) {
            r = d;
            return true;
        }

        return false;
    }
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

float CCLayerSorter::calculateZDiff(const LayerShape& layerA, const LayerShape& layerB, float earlyExitThreshold)
{
    LayerIntersector intersector(layerA, layerB, earlyExitThreshold);
    intersector.go();
    return intersector.zDiff;
}

CCLayerSorter::LayerIntersector::LayerIntersector(const LayerShape& layerA, const LayerShape& layerB, float earlyExitThreshold)
    : layerA(layerA)
    , layerB(layerB)
    , zDiff(0)
    , earlyExitThreshold(earlyExitThreshold)
{
}

void CCLayerSorter::LayerIntersector::go()
{
    (triangleTriangleTest(layerA.c1, layerA.c2, layerA.c3, layerB.c1, layerB.c2, layerB.c3)
     || triangleTriangleTest(layerA.c3, layerA.c4, layerA.c1, layerB.c1, layerB.c2, layerB.c3)
     || triangleTriangleTest(layerA.c1, layerA.c2, layerA.c3, layerB.c3, layerB.c4, layerB.c1)
     || triangleTriangleTest(layerA.c3, layerA.c4, layerA.c1, layerB.c3, layerB.c4, layerB.c1));
}

// Checks if segment pq intersects any of the sides of triangle abc.
bool CCLayerSorter::LayerIntersector::edgeTriangleTest(const FloatPoint& p, const FloatPoint& q, const FloatPoint& a, const FloatPoint& b, const FloatPoint& c)
{
    FloatPoint r;
    if ((edgeEdgeTest(p, q, a, b, r) && checkZDiff(r))
        || (edgeEdgeTest(p, q, a, c, r) && checkZDiff(r))
        || (edgeEdgeTest(p, q, b, c, r) && checkZDiff(r)))
        return true;

    return false;
}

// Checks if two co-planar triangles intersect.
bool CCLayerSorter::LayerIntersector::triangleTriangleTest(const FloatPoint& a1, const FloatPoint& b1, const FloatPoint& c1, const FloatPoint& a2, const FloatPoint& b2, const FloatPoint& c2)
{
    // Check all edges of first triangle with edges of the second one.
    if (edgeTriangleTest(a1, b1, a2, b2, c2)
        || edgeTriangleTest(a1, c1, a2, b2, c2)
        || edgeTriangleTest(b1, c1, a2, b2, c2))
        return true;

    // Check all points of the first triangle for inclusion in the second triangle.
    if ((pointInTriangle(a1, a2, b2, c2) && checkZDiff(a1))
        || (pointInTriangle(b1, a2, b2, c2) && checkZDiff(b1))
        || (pointInTriangle(c1, a2, b2, c2) && checkZDiff(c1)))
        return true;

    // Check all points of the second triangle for inclusion in the first triangle.
    if ((pointInTriangle(a2, a1, b1, c1) && checkZDiff(a2))
        || (pointInTriangle(b2, a1, b1, c1) && checkZDiff(b2))
        || (pointInTriangle(c2, a1, b1, c1) && checkZDiff(c2)))
        return true;

    return false;
}

// Computes the z difference between point p projected onto the two layers to determine
// which layer is in front. If the new z difference is higher than the currently
// recorded one the this becomes the point of choice for doing the comparison between the 
// layers. If the value of difference is higher than our early exit threshold then
// it means that the z difference is conclusive enough that we don't have to check
// other intersection points.
bool CCLayerSorter::LayerIntersector::checkZDiff(const FloatPoint& p)
{
    float za = layerZFromProjectedPoint(layerA, p);
    float zb = layerZFromProjectedPoint(layerB, p);

    float diff = za - zb;
    float absDiff = fabsf(diff);
    if ((absDiff) > fabsf(zDiff)) {
        intersectionPoint = p;
        zDiff = diff;
    }

    if (absDiff > earlyExitThreshold)
        return true;

    return false;
}


// Returns the Z coordinate of a point on the given layer that projects
// to point p which lies on the z = 0 plane. It does it by computing the
// intersection of a line starting from p along the Z axis and the plane
// of the layer.
float CCLayerSorter::LayerIntersector::layerZFromProjectedPoint(const LayerShape& layer, const FloatPoint& p)
{
    const FloatPoint3D zAxis(0, 0, 1);
    FloatPoint3D w = FloatPoint3D(p.x(), p.y(), 0) - layer.origin;

    float d = layer.normal.dot(zAxis);
    float n = -layer.normal.dot(w);

    // Check if layer is parallel to the z = 0 axis
    if (!d)
        return layer.origin.z();

    // The intersection point would be given by:
    // p + (n / d) * u  but since we are only interested in the 
    // z coordinate and p's z coord is zero, all we need is the value of n/d.
    return n / d;
}

CCLayerSorter::CCLayerSorter()
    : m_zRange(0)
{
}

CCLayerSorter::ABCompareResult CCLayerSorter::checkOverlap(GraphNode* a, GraphNode* b)
{
    if (!a->shape.boundingBox.intersects(b->shape.boundingBox))
        return None;

    // These thresholds are defined relative to the total Z range. If further Z-fighting
    // bugs come up due to precision errors, it may be worth considering doing an ulp
    // error measurement instead.
    float exitThreshold = m_zRange * 0.01;
    float nonZeroThreshold = max(exitThreshold, 0.001f);

    float zDiff = calculateZDiff(a->shape, b->shape, exitThreshold);

    if (zDiff > nonZeroThreshold)
        return BBeforeA;
    if (zDiff < -nonZeroThreshold)
        return ABeforeB;

    return None;
}

CCLayerSorter::LayerShape::LayerShape(const FloatPoint3D& p1, const FloatPoint3D& p2, const FloatPoint3D& p3, const FloatPoint3D& p4)
    : normal((p2 - p1).cross(p3 - p1))
    , c1(FloatPoint(p1.x(), p1.y()))
    , c2(FloatPoint(p2.x(), p2.y()))
    , c3(FloatPoint(p3.x(), p3.y()))
    , c4(FloatPoint(p4.x(), p4.y()))
    , origin(p1)
{
    boundingBox.fitToPoints(c1, c2, c3, c4);
}

void CCLayerSorter::createGraphNodes(LayerList::iterator first, LayerList::iterator last)
{
#if !defined( NDEBUG )
    LOG(CCLayerSorter, "Creating graph nodes:\n");
#endif
    float minZ = FLT_MAX;
    float maxZ = -FLT_MAX;
    for (LayerList::const_iterator it = first; it < last; it++) {
        m_nodes.append(GraphNode(it->get()));
        GraphNode& node = m_nodes.at(m_nodes.size() - 1);
        CCRenderSurface* renderSurface = node.layer->renderSurface();
        if (!node.layer->drawsContent() && !renderSurface)
            continue;

#if !defined( NDEBUG )
        LOG(CCLayerSorter, "Layer %d (%d x %d)\n", node.layer->debugID(), node.layer->bounds().width(), node.layer->bounds().height());
#endif

        TransformationMatrix drawTransform;
        float layerWidth, layerHeight;
        if (renderSurface) {
            drawTransform = renderSurface->drawTransform();
            layerWidth = 0.5 * renderSurface->contentRect().width();
            layerHeight = 0.5 * renderSurface->contentRect().height();
        } else {
            drawTransform = node.layer->drawTransform();
            layerWidth = 0.5 * node.layer->bounds().width();
            layerHeight = 0.5 * node.layer->bounds().height();
        }

        FloatPoint3D c1 = drawTransform.mapPoint(FloatPoint3D(-layerWidth, layerHeight, 0));
        FloatPoint3D c2 = drawTransform.mapPoint(FloatPoint3D(layerWidth, layerHeight, 0));
        FloatPoint3D c3 = drawTransform.mapPoint(FloatPoint3D(layerWidth, -layerHeight, 0));
        FloatPoint3D c4 = drawTransform.mapPoint(FloatPoint3D(-layerWidth, -layerHeight, 0));
        node.shape = LayerShape(c1, c2, c3, c4);
    
        maxZ = max(c4.z(), max(c3.z(), max(c2.z(), max(maxZ, c1.z()))));
        minZ = min(c4.z(), min(c3.z(), min(c2.z(), min(minZ, c1.z()))));
    }
    if (last - first)
        m_zRange = fabsf(maxZ - minZ);
}

void CCLayerSorter::createGraphEdges()
{
#if !defined( NDEBUG )
    LOG(CCLayerSorter, "Edges:\n");
#endif
    for (unsigned na = 0; na < m_nodes.size(); na++) {
        GraphNode& nodeA = m_nodes[na];
        if (!nodeA.layer->drawsContent() && !nodeA.layer->renderSurface())
            continue;
        for (unsigned nb = na + 1; nb < m_nodes.size(); nb++) {
            GraphNode& nodeB = m_nodes[nb];
            if (!nodeB.layer->drawsContent() && !nodeB.layer->renderSurface())
                continue;
            ABCompareResult overlapResult = checkOverlap(&nodeA, &nodeB);
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
                LOG(CCLayerSorter, "%d -> %d\n", startNode->layer->debugID(), endNode->layer->debugID());
#endif
                m_edges.append(GraphEdge(startNode, endNode));
            }
        }
    }

    for (unsigned i = 0; i < m_edges.size(); i++) {
        GraphEdge& edge = m_edges[i];
        m_activeEdges.add(&edge, &edge);
        edge.from->outgoing.append(&edge);
        edge.to->incoming.append(&edge);
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
// cyclical order dependencies. Cycles and intersections are broken aribtrarily.
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
            LOG(CCLayerSorter, "%d, ", fromNode->layer->debugID());
#endif

            // Remove all its outgoing edges from the graph.
            for (unsigned i = 0; i < fromNode->outgoing.size(); i++) {
                GraphEdge* outgoingEdge = fromNode->outgoing[i];

                m_activeEdges.remove(outgoingEdge);
                removeEdgeFromList(outgoingEdge, outgoingEdge->to->incoming);

                if (!outgoingEdge->to->incoming.size())
                    noIncomingEdgeNodeList.append(outgoingEdge->to);
            }
            fromNode->outgoing.clear();
        }

        if (!m_activeEdges.size())
            break;

        // If there are still active edges but the list of nodes without incoming edges
        // is empty then we have run into a cycle. Break the cycle by finding the node
        // with the least number incoming edges and remove them all.
        unsigned minIncomingEdgeCount = UINT_MAX;
        GraphNode* nextNode = 0;
        for (unsigned i = 0; i < m_nodes.size(); i++) {
            if (m_nodes[i].incoming.size() && (m_nodes[i].incoming.size() < minIncomingEdgeCount)) {
                minIncomingEdgeCount = m_nodes[i].incoming.size();
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
        noIncomingEdgeNodeList.append(nextNode);
#if !defined( NDEBUG )
        LOG(CCLayerSorter, "Breaking cycle by cleaning up %d edges from %d\n", minIncomingEdgeCount, nextNode->layer->debugID());
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
