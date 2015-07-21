/*
 * Copyright (C) 2014-2015 Apple Inc.  All rights reserved.
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


#include "config.h"
#include "PathUtilities.h"

#include "FloatPoint.h"
#include "FloatRect.h"
#include "GeometryUtilities.h"
#include <math.h>
#include <wtf/MathExtras.h>

namespace WebCore {

class FloatPointGraph {
    WTF_MAKE_NONCOPYABLE(FloatPointGraph);
public:
    FloatPointGraph() { }

    class Node : public FloatPoint {
        WTF_MAKE_NONCOPYABLE(Node);
    public:
        Node(FloatPoint point)
            : FloatPoint(point)
        { }

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

    typedef std::pair<Node*, Node*> Edge;
    typedef Vector<Edge> Polygon;

    Node* findOrCreateNode(FloatPoint);

    void reset()
    {
        for (auto& node : m_allNodes)
            node->reset();
    }

private:
    Vector<std::unique_ptr<Node>> m_allNodes;
};

FloatPointGraph::Node* FloatPointGraph::findOrCreateNode(FloatPoint point)
{
    for (auto& testNode : m_allNodes) {
        if (areEssentiallyEqual(*testNode, point))
            return testNode.get();
    }

    m_allNodes.append(std::make_unique<FloatPointGraph::Node>(point));
    return m_allNodes.last().get();
}

static bool findLineSegmentIntersection(const FloatPointGraph::Edge& edgeA, const FloatPointGraph::Edge& edgeB, FloatPoint& intersectionPoint)
{
    if (!findIntersection(*edgeA.first, *edgeA.second, *edgeB.first, *edgeB.second, intersectionPoint))
        return false;

    FloatPoint edgeAVec(*edgeA.second - *edgeA.first);
    FloatPoint edgeBVec(*edgeB.second - *edgeB.first);

    float dotA = edgeAVec.dot(toFloatPoint(intersectionPoint - *edgeA.first));
    if (dotA < 0 || dotA > edgeAVec.lengthSquared())
        return false;

    float dotB = edgeBVec.dot(toFloatPoint(intersectionPoint - *edgeB.first));
    if (dotB < 0 || dotB > edgeBVec.lengthSquared())
        return false;

    return true;
}

static bool addIntersectionPoints(Vector<FloatPointGraph::Polygon>& polys, FloatPointGraph& graph)
{
    bool foundAnyIntersections = false;

    Vector<FloatPointGraph::Edge> allEdges;
    for (auto& poly : polys)
        allEdges.appendVector(poly);

    for (const FloatPointGraph::Edge& edgeA : allEdges) {
        Vector<FloatPointGraph::Node*> intersectionPoints({edgeA.first, edgeA.second});

        for (const FloatPointGraph::Edge& edgeB : allEdges) {
            if (&edgeA == &edgeB)
                continue;

            FloatPoint intersectionPoint;
            if (!findLineSegmentIntersection(edgeA, edgeB, intersectionPoint))
                continue;
            foundAnyIntersections = true;
            intersectionPoints.append(graph.findOrCreateNode(intersectionPoint));
        }

        std::sort(intersectionPoints.begin(), intersectionPoints.end(), [edgeA] (FloatPointGraph::Node* a, FloatPointGraph::Node* b) {
            return FloatPoint(*edgeA.first - *b).lengthSquared() > FloatPoint(*edgeA.first - *a).lengthSquared();
        });

        for (unsigned pointIndex = 1; pointIndex < intersectionPoints.size(); pointIndex++)
            intersectionPoints[pointIndex - 1]->addNextPoint(intersectionPoints[pointIndex]);
    }

    return foundAnyIntersections;
}

static FloatPointGraph::Polygon walkGraphAndExtractPolygon(FloatPointGraph::Node* startNode)
{
    FloatPointGraph::Polygon outPoly;

    FloatPointGraph::Node* currentNode = startNode;
    FloatPointGraph::Node* previousNode = startNode;

    do {
        currentNode->visit();

        FloatPoint currentVec(*previousNode - *currentNode);
        currentVec.normalize();

        // Walk the graph, at each node choosing the next non-visited
        // point with the greatest internal angle.
        FloatPointGraph::Node* nextNode = nullptr;
        float nextNodeAngle = 0;
        for (auto potentialNextNode : currentNode->nextPoints()) {
            if (potentialNextNode == currentNode)
                continue;

            // If we can get back to the start, we should, ignoring the fact that we already visited it.
            // Otherwise we'll head inside the shape.
            if (potentialNextNode == startNode) {
                nextNode = startNode;
                break;
            }

            if (potentialNextNode->isVisited())
                continue;

            FloatPoint nextVec(*potentialNextNode - *currentNode);
            nextVec.normalize();

            float angle = acos(nextVec.dot(currentVec));
            float crossZ = nextVec.x() * currentVec.y() - nextVec.y() * currentVec.x();

            if (crossZ < 0)
                angle = (2 * piFloat) - angle;

            if (!nextNode || angle > nextNodeAngle) {
                nextNode = potentialNextNode;
                nextNodeAngle = angle;
            }
        }

        // If we don't end up at a node adjacent to the starting node,
        // something went wrong (there's probably a hole in the shape),
        // so bail out. We'll use a bounding box instead.
        if (!nextNode)
            return FloatPointGraph::Polygon();

        outPoly.append(std::make_pair(currentNode, nextNode));

        previousNode = currentNode;
        currentNode = nextNode;
    } while (currentNode != startNode);

    return outPoly;
}

static FloatPointGraph::Node* findUnvisitedPolygonStartPoint(Vector<FloatPointGraph::Polygon>& polys)
{
    for (auto& poly : polys) {
        for (auto& edge : poly) {
            if (edge.first->isVisited() || edge.second->isVisited())
                goto nextPolygon;
        }

        // FIXME: We should make sure we find an outside edge to start with.
        return poly[0].first;
    nextPolygon:
        continue;
    }
    return nullptr;
}

static Vector<FloatPointGraph::Polygon> unitePolygons(Vector<FloatPointGraph::Polygon>& polys, FloatPointGraph& graph)
{
    graph.reset();

    // There are no intersections, so the polygons are disjoint (we already removed wholly-contained rects in an earlier step).
    if (!addIntersectionPoints(polys, graph))
        return polys;

    Vector<FloatPointGraph::Polygon> unitedPolygons;

    while (FloatPointGraph::Node* startNode = findUnvisitedPolygonStartPoint(polys)) {
        FloatPointGraph::Polygon unitedPolygon = walkGraphAndExtractPolygon(startNode);
        if (unitedPolygon.isEmpty())
            return Vector<FloatPointGraph::Polygon>();
        unitedPolygons.append(unitedPolygon);
    }

    return unitedPolygons;
}

static FloatPointGraph::Polygon edgesForRect(FloatRect rect, FloatPointGraph& graph)
{
    auto minMin = graph.findOrCreateNode(rect.minXMinYCorner());
    auto minMax = graph.findOrCreateNode(rect.minXMaxYCorner());
    auto maxMax = graph.findOrCreateNode(rect.maxXMaxYCorner());
    auto maxMin = graph.findOrCreateNode(rect.maxXMinYCorner());

    return FloatPointGraph::Polygon({
        std::make_pair(minMin, maxMin),
        std::make_pair(maxMin, maxMax),
        std::make_pair(maxMax, minMax),
        std::make_pair(minMax, minMin)
    });
}

Path PathUtilities::pathWithShrinkWrappedRects(const Vector<FloatRect>& rects, float radius)
{
    Path path;

    if (rects.isEmpty())
        return path;

    if (rects.size() > 20) {
        path.addRoundedRect(unionRect(rects), FloatSize(radius, radius));
        return path;
    }

    Vector<FloatRect> sortedRects = rects;

    std::sort(sortedRects.begin(), sortedRects.end(), [](FloatRect a, FloatRect b) { return b.y() > a.y(); });

    FloatPointGraph graph;
    Vector<FloatPointGraph::Polygon> rectPolygons;
    rectPolygons.reserveInitialCapacity(sortedRects.size());

    for (auto& rect : sortedRects) {
        bool isContained = false;
        for (auto& otherRect : sortedRects) {
            if (&rect == &otherRect)
                continue;
            if (otherRect.contains(rect)) {
                isContained = true;
                break;
            }
        }

        if (!isContained)
            rectPolygons.append(edgesForRect(rect, graph));
    }

    Vector<FloatPointGraph::Polygon> polys = unitePolygons(rectPolygons, graph);

    if (polys.isEmpty()) {
        path.addRoundedRect(unionRect(sortedRects), FloatSize(radius, radius));
        return path;
    }

    for (auto& poly : polys) {
        for (unsigned i = 0; i < poly.size(); i++) {
            FloatPointGraph::Edge& toEdge = poly[i];
            // Connect the first edge to the last.
            FloatPointGraph::Edge& fromEdge = (i > 0) ? poly[i - 1] : poly[poly.size() - 1];

            FloatPoint fromEdgeVec = toFloatPoint(*fromEdge.second - *fromEdge.first);
            FloatPoint toEdgeVec = toFloatPoint(*toEdge.second - *toEdge.first);

            // Clamp the radius to no more than half the length of either adjacent edge,
            // because we want a smooth curve and don't want unequal radii.
            float clampedRadius = std::min(radius, fabsf(fromEdgeVec.x() ? fromEdgeVec.x() : fromEdgeVec.y()) / 2);
            clampedRadius = std::min(clampedRadius, fabsf(toEdgeVec.x() ? toEdgeVec.x() : toEdgeVec.y()) / 2);

            FloatPoint fromEdgeNorm = fromEdgeVec;
            fromEdgeNorm.normalize();
            FloatPoint toEdgeNorm = toEdgeVec;
            toEdgeNorm.normalize();

            // Project the radius along the incoming and outgoing edge.
            FloatSize fromOffset = clampedRadius * toFloatSize(fromEdgeNorm);
            FloatSize toOffset = clampedRadius * toFloatSize(toEdgeNorm);

            if (!i)
                path.moveTo(*fromEdge.second - fromOffset);
            else
                path.addLineTo(*fromEdge.second - fromOffset);
            path.addArcTo(*fromEdge.second, *toEdge.first + toOffset, clampedRadius);
        }

        path.closeSubpath();
    }

    return path;
}

}
