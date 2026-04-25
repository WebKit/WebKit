/*
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleRayFunction.h"

#include "AcceleratedEffectRayFunction.h"
#include "GeometryUtilities.h"
#include "Path.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StylePrimitiveNumericTypes+Serialization.h"
#include "TransformOperationData.h"

namespace WebCore {
namespace Style {

void Serialize<Ray>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const Ray& value)
{
    // ray() = ray( <angle> && <ray-size>? && contain? && [at <position>]? )
    // https://drafts.fxtf.org/motion-1/#ray-function

    serializationForCSS(builder, context, style, value.angle);

    if (!std::holds_alternative<CSS::Keyword::ClosestSide>(value.size)) {
        builder.append(' ');
        serializationForCSS(builder, context, style, value.size);
    }

    if (value.contain) {
        builder.append(' ');
        serializationForCSS(builder, context, style, *value.contain);
    }

    if (value.position) {
        builder.append(' ', nameLiteralForSerialization(CSSValueAt), ' ');
        serializationForCSS(builder, context, style, *value.position);
    }
}

// MARK: - Path

static double lengthForRayPath(const Ray& ray, const MotionPathData& data)
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

std::optional<WebCore::Path> tryPath(const Ray& ray, const TransformOperationData& transformData, ZoomFactor)
{
    auto motionPathData = transformData.motionPathData;
    if (!motionPathData || motionPathData->containingBlockBoundingRect.rect().isZero())
        return std::nullopt;

    auto elementBoundingBox = transformData.boundingBox;
    double length = lengthForRayPath(ray, *motionPathData);
    if (ray.contain)
        length = lengthForRayContainPath(elementBoundingBox, length);

    auto radians = deg2rad(toPositiveAngle(ray.angle.value) - 90.0);
    auto point = FloatPoint(std::cos(radians) * length, std::sin(radians) * length);

    auto currentOffset = motionPathData->currentOffset();

    WebCore::Path path;
    path.moveTo(currentOffset);
    path.addLineTo(currentOffset + point);
    return path;
}

// MARK: - Evaluation

#if ENABLE(THREADED_ANIMATIONS)

AcceleratedEffectRayFunction Evaluation<RayFunction, AcceleratedEffectRayFunction>::operator()(const RayFunction& ray, const TransformOperationData& data, ZoomFactor zoom)
{
    auto toAcceleratedEffectRaySize = [](RaySize size) -> AcceleratedEffectRayFunction::RaySize {
        return WTF::switchOn(size,
            [](const CSS::Keyword::ClosestCorner)  { return AcceleratedEffectRayFunction::RaySize::ClosestCorner; },
            [](const CSS::Keyword::ClosestSide)    { return AcceleratedEffectRayFunction::RaySize::ClosestSide; },
            [](const CSS::Keyword::FarthestCorner) { return AcceleratedEffectRayFunction::RaySize::FarthestCorner; },
            [](const CSS::Keyword::FarthestSide)   { return AcceleratedEffectRayFunction::RaySize::FarthestSide; },
            [](const CSS::Keyword::Sides)          { return AcceleratedEffectRayFunction::RaySize::Sides; }
        );
    };

    return {
        .angle = ray->angle.value,
        .size = toAcceleratedEffectRaySize(ray->size),
        .contain = ray->contain ? std::optional { AcceleratedEffectRayFunction::Contain { } } : std::nullopt,
        .position = ray->position ? std::optional { evaluate<FloatPoint>(*ray->position, data.motionPathData->containingBlockBoundingRect.rect().size(), zoom) } : std::nullopt,
    };
}

#endif

} // namespace Style
} // namespace WebCore
