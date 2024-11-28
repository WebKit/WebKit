/*
 * Copyright (C) 2024 Jani Hautakangas <jani@kodegood.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TextureMapperLayer3DRenderingContext.h"

#include "TextureMapperLayer.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(TextureMapperLayer3DRenderingContext);

void TextureMapperLayer3DRenderingContext::paint(const Vector<TextureMapperLayer*>& layers, const std::function<void(TextureMapperLayer*, const FloatPolygon&)>& paintLayerFunction)
{
    if (layers.isEmpty())
        return;

    Deque<TextureMapperLayerPolygon> layerList;
    for (auto* layer : layers)
        layerList.append({ { layer->effectiveLayerRect(), layer->toSurfaceTransform() }, layer, false });

    auto root = makeUnique<TextureMapperLayerNode>(layerList.takeFirst());
    buildTree(*root, layerList);
    traverseTreeAndPaint(*root, paintLayerFunction);
}

// Build BSP tree for rendering polygons with painter's algorithm.
// For more information:
// https://en.wikipedia.org/wiki/Binary_space_partitioning
void TextureMapperLayer3DRenderingContext::buildTree(TextureMapperLayerNode& root, Deque<TextureMapperLayerPolygon>& polygons)
{
    if (polygons.isEmpty())
        return;

    auto& rootGeometry = root.firstPolygon().geometry;
    FloatPlane3D rootPlane(rootGeometry.normal(), rootGeometry.vertexAt(0));

    Deque<TextureMapperLayerPolygon> backList, frontList;
    for (auto& polygon : polygons) {
        switch (classifyPolygon(polygon, rootPlane)) {
        case PolygonPosition::InFront:
            frontList.append(WTFMove(polygon));
            break;
        case PolygonPosition::Behind:
            backList.append(WTFMove(polygon));
            break;
        case PolygonPosition::Coplanar:
            root.polygons.append(WTFMove(polygon));
            break;
        case PolygonPosition::Intersecting:
            auto [backGeometry, frontGeometry] = polygon.geometry.split(rootPlane);
            if (backGeometry.numberOfVertices() > 2)
                backList.append({ backGeometry, polygon.layer, true });
            if (frontGeometry.numberOfVertices() > 2)
                frontList.append({ frontGeometry, polygon.layer, true });
            break;
        }
    }

    if (!frontList.isEmpty()) {
        root.frontNode = makeUnique<TextureMapperLayerNode>(frontList.takeFirst());
        buildTree(*root.frontNode, frontList);
    }

    if (!backList.isEmpty()) {
        root.backNode = makeUnique<TextureMapperLayerNode>(backList.takeFirst());
        buildTree(*root.backNode, backList);
    }
}

void TextureMapperLayer3DRenderingContext::traverseTreeAndPaint(TextureMapperLayerNode& node, const std::function<void(TextureMapperLayer*, const FloatPolygon&)>& paintLayerFunction)
{
    auto& geometry = node.firstPolygon().geometry;
    FloatPlane3D plane(geometry.normal(), geometry.vertexAt(0));

    auto* frontNode = node.frontNode.get();
    auto* backNode = node.backNode.get();

    // if polygon is facing away from camera then swap nodes to reverse
    // the traversal order
    if (plane.normal().z() < 0)
        std::swap(frontNode, backNode);

    if (backNode)
        traverseTreeAndPaint(*backNode, paintLayerFunction);

    for (auto& polygon : node.polygons)
        paintLayerFunction(polygon.layer, polygon.layerClipArea());

    if (frontNode)
        traverseTreeAndPaint(*frontNode, paintLayerFunction);
}

TextureMapperLayer3DRenderingContext::PolygonPosition TextureMapperLayer3DRenderingContext::classifyPolygon(const TextureMapperLayerPolygon& polygon, const FloatPlane3D& plane)
{
    const float epsilon = 0.05f; // Tolerance for intersection check

    int inFrontCount = 0;
    int behindCount = 0;
    for (unsigned i = 0; i < polygon.geometry.numberOfVertices(); i++) {
        const auto& vertex = polygon.geometry.vertexAt(i);
        float distance = plane.distanceToPoint(vertex);

        if (distance > epsilon)
            inFrontCount++;
        else if (distance < -epsilon)
            behindCount++;
    }

    if (inFrontCount > 0 && behindCount > 0)
        return PolygonPosition::Intersecting;
    if (inFrontCount > 0)
        return PolygonPosition::InFront;
    if (behindCount > 0)
        return PolygonPosition::Behind;
    return PolygonPosition::Coplanar;
}

} // namespace WebCore
