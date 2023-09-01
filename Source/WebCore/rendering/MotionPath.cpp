/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "MotionPath.h"

#include "GeometryUtilities.h"
#include "PathOperation.h"
#include "PathTraversalState.h"

namespace WebCore {

static FloatPoint offsetFromContainer(const RenderObject& renderer, RenderBlock& container, const FloatRect& referenceRect)
{
    auto offsetFromContainingBlock = renderer.offsetFromContainer(container, LayoutPoint());
    return FloatPoint(FloatPoint(offsetFromContainingBlock) - referenceRect.location());
}

std::optional<MotionPathData> MotionPath::motionPathDataForRenderer(const RenderElement& renderer)
{
    MotionPathData data;
    auto pathOperation = renderer.style().offsetPath();
    if (!pathOperation || !is<RenderLayerModelObject>(renderer))
        return std::nullopt;

    auto startingPositionForOffsetPosition = [&](const LengthPoint& offsetPosition, const FloatRect& referenceRect, RenderBlock& container) -> FloatPoint {
        // FIXME: Implement offset-position: normal.
        // If offset-position is auto, use top / left corner of the box.
        if (offsetPosition.x().isAuto() && offsetPosition.y().isAuto())
            return offsetFromContainer(renderer, container, referenceRect);
        return floatPointForLengthPoint(offsetPosition, referenceRect.size());
    };

    if (is<BoxPathOperation>(pathOperation)) {
        if (auto* container = renderer.containingBlock()) {
            auto& boxPathOperation = downcast<BoxPathOperation>(*pathOperation);
            data.containingBlockBoundingRect = snapRectToDevicePixelsIfNeeded(container->referenceBoxRect(boxPathOperation.referenceBox()), downcast<RenderLayerModelObject>(renderer));
            data.offsetFromContainingBlock = offsetFromContainer(renderer, *container, data.containingBlockBoundingRect);
            return data;
        }
        return std::nullopt;
    }
    if (is<ShapePathOperation>(pathOperation)) {
        if (auto* container = renderer.containingBlock()) {
            auto& shapePathOperation = downcast<ShapePathOperation>(*pathOperation);
            data.containingBlockBoundingRect = snapRectToDevicePixelsIfNeeded(container->referenceBoxRect(shapePathOperation.referenceBox()), downcast<RenderLayerModelObject>(renderer));
            data.offsetFromContainingBlock = offsetFromContainer(renderer, *container, data.containingBlockBoundingRect);
            return data;
        }
        return std::nullopt;
    }
    if (is<RayPathOperation>(pathOperation)) {
        if (auto* container = renderer.containingBlock()) {
            auto& rayPathOperation = downcast<RayPathOperation>(*pathOperation);
            auto pathReferenceBoxRect = snapRectToDevicePixelsIfNeeded(container->referenceBoxRect(rayPathOperation.referenceBox()), downcast<RenderLayerModelObject>(renderer));

            auto offsetPosition = renderer.style().offsetPosition();
            auto startingPosition = rayPathOperation.position();
            auto usedStartingPosition = startingPosition.x().isAuto() ? startingPositionForOffsetPosition(offsetPosition, pathReferenceBoxRect, *container) : floatPointForLengthPoint(startingPosition, pathReferenceBoxRect.size());
            data.usedStartingPosition = usedStartingPosition;
            data.offsetFromContainingBlock = offsetFromContainer(renderer, *container, pathReferenceBoxRect);
            data.containingBlockBoundingRect = pathReferenceBoxRect;

            return data;
        }
    }
    return std::nullopt;
}

static PathTraversalState traversalStateAtDistance(const Path& path, const Length& distance)
{
    auto pathLength = path.length();
    auto distanceValue = floatValueForLength(distance, pathLength);

    float resolvedLength = 0;
    if (path.isClosed()) {
        if (pathLength) {
            resolvedLength = fmod(distanceValue, pathLength);
            if (resolvedLength < 0)
                resolvedLength += pathLength;
        }
    } else
        resolvedLength = clampTo<float>(distanceValue, 0, pathLength);

    ASSERT(resolvedLength >= 0);
    return path.traversalStateAtLength(resolvedLength);
}

void MotionPath::applyMotionPathTransform(const RenderStyle& style, const TransformOperationData& transformData, TransformationMatrix& transform)
{
    if (!style.offsetPath())
        return;

    auto& boundingBox = transformData.boundingBox();
    auto transformOrigin = style.computeTransformOrigin(boundingBox).xy();
    auto anchor = transformOrigin;
    if (!style.offsetAnchor().x().isAuto())
        anchor = floatPointForLengthPoint(style.offsetAnchor(), boundingBox.size()) + boundingBox.location();

    // Shift element to the point on path specified by offset-path and offset-distance.
    auto path = style.offsetPath()->getPath(transformData);
    if (!path)
        return;
    auto traversalState = traversalStateAtDistance(*path, style.offsetDistance());
    transform.translate(traversalState.current().x(), traversalState.current().y());

    // Shift element to the anchor specified by offset-anchor.
    transform.translate(-anchor.x(), -anchor.y());

    auto shiftToOrigin = anchor - transformOrigin;
    transform.translate(shiftToOrigin.width(), shiftToOrigin.height());

    // Apply rotation.
    auto rotation = style.offsetRotate();
    if (rotation.hasAuto())
        transform.rotate(traversalState.normalAngle() + rotation.angle());
    else
        transform.rotate(rotation.angle());

    transform.translate(-shiftToOrigin.width(), -shiftToOrigin.height());
}

bool MotionPath::needsUpdateAfterContainingBlockLayout(const PathOperation& pathOperation)
{
    return is<RayPathOperation>(pathOperation) || is<BoxPathOperation>(pathOperation) || is<ShapePathOperation>(pathOperation);
}

double MotionPath::lengthForRayPath(const RayPathOperation& rayPathOperation, const MotionPathData& data)
{
    auto& boundingBox = data.containingBlockBoundingRect;
    auto distances = distanceOfPointToSidesOfRect(boundingBox, data.usedStartingPosition);

    switch (rayPathOperation.size()) {
    case RayPathOperation::Size::ClosestSide:
        return std::min( { distances.top(), distances.bottom(), distances.left(), distances.right() } );
    case RayPathOperation::Size::FarthestSide:
        return std::max( { distances.top(), distances.bottom(), distances.left(), distances.right() } );
    case RayPathOperation::Size::FarthestCorner:
        return std::hypot(std::max(distances.left(), distances.right()), std::max(distances.top(), distances.bottom()));
    case RayPathOperation::Size::ClosestCorner:
        return std::hypot(std::min(distances.left(), distances.right()), std::min(distances.top(), distances.bottom()));
    case RayPathOperation::Size::Sides:
        return lengthOfRayIntersectionWithBoundingBox(boundingBox, std::make_pair(data.usedStartingPosition, rayPathOperation.angle()));
    }
    RELEASE_ASSERT_NOT_REACHED();
}

double MotionPath::lengthForRayContainPath(const FloatRect& elementRect, double computedPathLength)
{
    return std::max(0.0, computedPathLength - (std::max(elementRect.width(), elementRect.height()) / 2));
}

static FloatPoint currentOffsetForData(const MotionPathData& data)
{
    return FloatPoint(data.usedStartingPosition - data.offsetFromContainingBlock);
}

std::optional<Path> MotionPath::computePathForRay(const RayPathOperation& rayPathOperation, const TransformOperationData& data)
{
    auto motionPathData = data.motionPathData();
    auto elementBoundingBox = data.boundingBox();
    if (!motionPathData || motionPathData->containingBlockBoundingRect.isZero())
        return std::nullopt;

    double length = lengthForRayPath(rayPathOperation, *motionPathData);
    if (rayPathOperation.isContaining())
        length = lengthForRayContainPath(elementBoundingBox, length);

    auto radians = deg2rad(toPositiveAngle(rayPathOperation.angle()) - 90.0);
    auto point = FloatPoint(std::cos(radians) * length, std::sin(radians) * length);

    Path path;
    path.moveTo(currentOffsetForData(*motionPathData));
    path.addLineTo(currentOffsetForData(*motionPathData) + point);
    return path;
}

std::optional<Path> MotionPath::computePathForBox(const BoxPathOperation&, const TransformOperationData& data)
{
    if (auto motionPathData = data.motionPathData()) {
        Path path;
        auto rect = motionPathData->containingBlockBoundingRect;
        auto shiftedPoint = motionPathData->offsetFromContainingBlock;
        shiftedPoint.scale(-1);
        rect.setLocation(shiftedPoint);
        path.addRect(rect);
        return path;
    }
    return std::nullopt;
}


} // namespace WebCore
