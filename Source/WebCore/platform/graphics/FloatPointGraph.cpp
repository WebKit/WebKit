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

#include "config.h"
#include "FloatPointGraph.h"

#include "GeometryUtilities.h"
#include <wtf/MathExtras.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FloatPointGraph::Node);

FloatPointGraph::Node* FloatPointGraph::findOrCreateNode(FloatPoint point)
{
    for (auto& testNode : m_allNodes) {
        auto tolerance = [&] (auto a, auto b) {
            auto toleranceInPixel = 0.005f;
            return toleranceInPixel / std::max(std::abs(a), std::abs(b)) * 100.f;
        };
        if (WTF::areEssentiallyEqual(testNode->x(), point.x(), tolerance(testNode->x(), point.x())) && WTF::areEssentiallyEqual(testNode->y(), point.y(), tolerance(testNode->y(), point.y())))
            return testNode.get();
    }

    m_allNodes.append(makeUnique<FloatPointGraph::Node>(point));
    return m_allNodes.last().get();
}

void FloatPointGraph::reset()
{
    for (auto& node : m_allNodes)
        node->reset();
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
        Vector<FloatPointGraph::Node*> intersectionPoints({ edgeA.first, edgeA.second });

        for (const FloatPointGraph::Edge& edgeB : allEdges) {
            if (&edgeA == &edgeB)
                continue;

            FloatPoint intersectionPoint;
            if (!findLineSegmentIntersection(edgeA, edgeB, intersectionPoint))
                continue;
            foundAnyIntersections = true;
            intersectionPoints.append(graph.findOrCreateNode(intersectionPoint));
        }

        std::ranges::sort(intersectionPoints, [edgeA](auto* a, auto* b) {
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
        for (auto* potentialNextNode : currentNode->nextPoints()) {
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
                angle = (2 * std::numbers::pi_v<float>) - angle;

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

std::pair<FloatPointGraph, Vector<FloatPointGraph::Polygon>> FloatPointGraph::polygonsForRect(const Vector<FloatRect>& rects)
{
    Vector<FloatRect> sortedRects = rects;
    // FIXME: Replace it with 2 dimensional sort.
    std::ranges::sort(sortedRects, { }, &FloatRect::x);
    std::ranges::sort(sortedRects, { }, &FloatRect::y);

    FloatPointGraph graph;

    Vector<Polygon> rectPolygons;
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

    auto unitedPolygons = unitePolygons(rectPolygons, graph);

    return { WTF::move(graph), WTF::move(unitedPolygons) };
}

} // namespace WebCore
