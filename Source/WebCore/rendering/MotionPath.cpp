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

#include "BorderShape.h"
#include "GeometryUtilities.h"
#include "PathOperation.h"
#include "PathTraversalState.h"
#include "RenderBlock.h"
#include "RenderObjectInlines.h"
#include "RenderStyleInlines.h"
#include "TransformOperationData.h"
#include "TransformationMatrix.h"

namespace WebCore {

static FloatPoint offsetFromContainer(const RenderObject& renderer, RenderBlock& container, const FloatRect& referenceRect)
{
    auto offsetFromContainingBlock = renderer.offsetFromContainer(container, LayoutPoint());
    return FloatPoint(FloatPoint(offsetFromContainingBlock) - referenceRect.location());
}

static FloatRoundedRect containingBlockRectForRenderer(const RenderObject& renderer, RenderBlock& container, const Style::OffsetPath& offsetPath)
{
    return WTF::switchOn(offsetPath,
        [&](const Style::BoxPath& offsetPath) -> FloatRoundedRect {
            auto referenceBox = offsetPath.referenceBox();
            auto referenceRect = container.referenceBoxRect(referenceBox);
            auto borderShape = BorderShape::shapeForBorderRect(container.style(), LayoutRect(referenceRect));
            return borderShape.deprecatedPixelSnappedRoundedRect(container.document().deviceScaleFactor());
        },
        [&](const auto& offsetPath) -> FloatRoundedRect {
            auto referenceBox = offsetPath.referenceBox();
            auto snappedRect = snapRectToDevicePixelsIfNeeded(container.referenceBoxRect(referenceBox), downcast<RenderLayerModelObject>(renderer));
            return FloatRoundedRect { snappedRect };
        },
        [&](const CSS::Keyword::None&) -> FloatRoundedRect {
            RELEASE_ASSERT_NOT_REACHED();
        }
    );
}

static FloatPoint normalPositionForOffsetPath(const Style::OffsetPath& offsetPath, const FloatRect& referenceRect)
{
    if (WTF::holdsAlternative<Style::RayPath>(offsetPath) || WTF::holdsAlternative<Style::BasicShapePath>(offsetPath))
        return referenceRect.center();
    return { };
}

std::optional<MotionPathData> MotionPath::motionPathDataForRenderer(const RenderElement& renderer)
{
    if (!is<RenderLayerModelObject>(renderer))
        return std::nullopt;

    auto& offsetPath = renderer.style().offsetPath();
    bool canBuildMotionPathData = WTF::switchOn(offsetPath,
        [](const CSS::Keyword::None&) {
            return false;
        },
        [](const Style::BasicShapePath& offsetPath) {
            return !std::holds_alternative<Style::PathFunction>(offsetPath.shape());
        },
        [](const auto&) {
            return true;
        }
    );
    if (!canBuildMotionPathData)
        return std::nullopt;

    auto startingPositionForOffsetPosition = [&](const Style::OffsetPosition& offsetPosition, const FloatRect& referenceRect, RenderBlock& container) -> FloatPoint {
        return WTF::switchOn(offsetPosition,
            [&](const CSS::Keyword::Normal&) {
                // If offset-position is normal, the element does not have an offset starting position.
                return normalPositionForOffsetPath(offsetPath, referenceRect);
            },
            [&](const CSS::Keyword::Auto&)  {
                // If offset-position is auto, use top / left corner of the box.
                return offsetFromContainer(renderer, container, referenceRect);
            },
            [&](const Style::Position& position) {
                return Style::evaluate<FloatPoint>(position, referenceRect.size(), Style::ZoomNeeded { });
            }
        );
    };

    auto* container = renderer.containingBlock();
    if (!container)
        return std::nullopt;

    MotionPathData data;
    data.containingBlockBoundingRect = containingBlockRectForRenderer(renderer, *container, offsetPath);
    data.offsetFromContainingBlock = offsetFromContainer(renderer, *container, data.containingBlockBoundingRect.rect());

    auto& offsetPosition = renderer.style().offsetPosition();

    WTF::switchOn(offsetPath,
        [&](const Style::BasicShapePath&) {
            data.usedStartingPosition = startingPositionForOffsetPosition(offsetPosition, data.containingBlockBoundingRect.rect(), *container);
        },
        [&](const Style::RayPath& offsetPath) {
            auto startingPosition = offsetPath.ray()->position;
            data.usedStartingPosition = startingPosition
                ? Style::evaluate<FloatPoint>(*startingPosition, data.containingBlockBoundingRect.rect().size(), Style::ZoomNeeded { })
                : startingPositionForOffsetPosition(offsetPosition, data.containingBlockBoundingRect.rect(), *container);
        },
        [&](const auto&) { }
    );

    return data;
}

static PathTraversalState traversalStateAtDistance(const Path& path, float distanceValue)
{
    auto pathLength = path.length();
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

void MotionPath::applyMotionPathTransform(TransformationMatrix& matrix, const TransformOperationData& transformData, FloatPoint transformOrigin, TransformBox transformBox, const Path& offsetPath, std::optional<FloatPoint> offsetAnchor, float offsetDistance, float offsetRotate, bool offsetRotateHasAuto)
{
    auto boundingBox = transformData.boundingBox;
    auto anchor = transformOrigin;

    if (offsetAnchor)
        anchor = *offsetAnchor + boundingBox.location();

    auto shiftToOrigin = anchor - transformOrigin;

    if (transformData.isSVGRenderer && transformBox != TransformBox::ViewBox)
        anchor += boundingBox.location();

    auto traversalState = traversalStateAtDistance(offsetPath, offsetDistance);
    matrix.translate(traversalState.current().x(), traversalState.current().y());

    // Shift element to the anchor specified by offset-anchor.
    matrix.translate(-anchor.x(), -anchor.y());

    matrix.translate(shiftToOrigin.width(), shiftToOrigin.height());

    // Apply rotation.
    if (offsetRotateHasAuto)
        matrix.rotate(traversalState.normalAngle() + offsetRotate);
    else
        matrix.rotate(offsetRotate);

    matrix.translate(-shiftToOrigin.width(), -shiftToOrigin.height());
}

void MotionPath::applyMotionPathTransform(TransformationMatrix& matrix, const TransformOperationData& transformData, const RenderStyle& style)
{
    auto offsetPath = Style::tryPath(style.offsetPath(), transformData);
    if (!offsetPath)
        return;

    auto boundingBox = transformData.boundingBox;

    auto transformOrigin = style.computeTransformOrigin(boundingBox).xy();
    auto transformBox = style.transformBox();

    auto offsetDistance = Style::evaluate<float>(style.offsetDistance(), offsetPath->length(), Style::ZoomNeeded { });
    auto offsetAnchor = WTF::switchOn(style.offsetAnchor(),
        [&](const Style::Position& position) -> std::optional<FloatPoint> {
            return Style::evaluate<FloatPoint>(position, boundingBox.size(), Style::ZoomNeeded { });
        },
        [&](const CSS::Keyword::Auto&) -> std::optional<FloatPoint> {
            return { };
        }
    );
    auto offsetRotate = style.offsetRotate().angle().value;
    auto offsetRotateHasAuto = style.offsetRotate().hasAuto();

    applyMotionPathTransform(
        matrix,
        transformData,
        transformOrigin,
        transformBox,
        *offsetPath,
        offsetAnchor,
        offsetDistance,
        offsetRotate,
        offsetRotateHasAuto
    );
}

bool MotionPath::needsUpdateAfterContainingBlockLayout(const Style::OffsetPath& offsetPath)
{
    return WTF::holdsAlternative<Style::RayPath>(offsetPath)
        || WTF::holdsAlternative<Style::BoxPath>(offsetPath)
        || WTF::holdsAlternative<Style::BasicShapePath>(offsetPath);
}

static double lengthForRayPath(const Style::Ray& ray, const MotionPathData& data)
{
    auto& boundingBox = data.containingBlockBoundingRect.rect();
    auto distances = distanceOfPointToSidesOfRect(boundingBox, data.usedStartingPosition);

    return WTF::switchOn(ray.size,
        [&](CSS::Keyword::ClosestSide) {
            return std::min( { distances.top(), distances.bottom(), distances.left(), distances.right() } );
        },
        [&](CSS::Keyword::FarthestSide) {
            return std::max( { distances.top(), distances.bottom(), distances.left(), distances.right() } );
        },
        [&](CSS::Keyword::FarthestCorner) {
            return std::hypot(std::max(distances.left(), distances.right()), std::max(distances.top(), distances.bottom()));
        },
        [&](CSS::Keyword::ClosestCorner) {
            return std::hypot(std::min(distances.left(), distances.right()), std::min(distances.top(), distances.bottom()));
        },
        [&](CSS::Keyword::Sides) {
            return lengthOfRayIntersectionWithBoundingBox(boundingBox, std::make_pair(data.usedStartingPosition, ray.angle.value));
        }
    );
}

static double lengthForRayContainPath(const FloatRect& elementRect, double computedPathLength)
{
    return std::max(0.0, computedPathLength - (std::max(elementRect.width(), elementRect.height()) / 2));
}

static FloatPoint currentOffsetForData(const MotionPathData& data)
{
    return FloatPoint(data.usedStartingPosition - data.offsetFromContainingBlock);
}

std::optional<Path> MotionPath::computePathForRay(const RayPathOperation& rayPathOperation, const TransformOperationData& transformData)
{
    auto motionPathData = transformData.motionPathData;
    if (!motionPathData || motionPathData->containingBlockBoundingRect.rect().isZero())
        return std::nullopt;

    auto elementBoundingBox = transformData.boundingBox;
    double length = lengthForRayPath(*rayPathOperation.ray(), *motionPathData);
    if (rayPathOperation.ray()->contain)
        length = lengthForRayContainPath(elementBoundingBox, length);

    auto radians = deg2rad(toPositiveAngle(rayPathOperation.ray()->angle.value) - 90.0);
    auto point = FloatPoint(std::cos(radians) * length, std::sin(radians) * length);

    Path path;
    path.moveTo(currentOffsetForData(*motionPathData));
    path.addLineTo(currentOffsetForData(*motionPathData) + point);
    return path;
}

static FloatRoundedRect offsetRectForData(const MotionPathData& data)
{
    auto rect = data.containingBlockBoundingRect;
    auto shiftedPoint = data.offsetFromContainingBlock;
    shiftedPoint.scale(-1);
    rect.setLocation(shiftedPoint);
    return rect;
}

std::optional<Path> MotionPath::computePathForBox(const BoxPathOperation&, const TransformOperationData& transformData)
{
    if (auto motionPathData = transformData.motionPathData) {
        Path path;
        path.addRoundedRect(offsetRectForData(*motionPathData), PathRoundedRect::Strategy::PreferBezier);
        return path;
    }
    return std::nullopt;
}

std::optional<Path> MotionPath::computePathForShape(const ShapePathOperation& pathOperation, const TransformOperationData& transformData)
{
    if (auto motionPathData = transformData.motionPathData) {
        auto containingBlockRect = offsetRectForData(*motionPathData).rect();
        return WTF::switchOn(pathOperation.shape(),
            [&]<Style::ShapeWithCenterCoordinate T>(const T& shape) -> std::optional<Path> {
                if (!shape->position)
                    return Style::pathForCenterCoordinate(*shape, containingBlockRect, motionPathData->usedStartingPosition);
                return Style::path(shape, containingBlockRect);
            },
            [&](const auto& shape) -> std::optional<Path> {
                return Style::path(shape, containingBlockRect);
            }
        );
    }
    return pathOperation.pathForReferenceRect(transformData.boundingBox);
}

} // namespace WebCore
