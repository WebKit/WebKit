/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleCircleFunction.h"

#include "FloatRect.h"
#include "GeometryUtilities.h"
#include "Path.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include <wtf/TinyLRUCache.h>

namespace WebCore {
namespace Style {

// MARK: - Path Caching

struct CirclePathPolicy final : public TinyLRUCachePolicy<FloatRect, WebCore::Path> {
public:
    static bool isKeyNull(const FloatRect& rect)
    {
        return rect.isEmpty();
    }

    static WebCore::Path createValueForKey(const FloatRect& rect)
    {
        WebCore::Path path;
        path.addEllipseInRect(rect);
        return path;
    }
};

static const WebCore::Path& cachedCirclePath(const FloatRect& rect)
{
    static NeverDestroyed<TinyLRUCache<FloatRect, WebCore::Path, 4, CirclePathPolicy>> cache;
    return cache.get().get(rect);
}

// MARK: - Path Generation

FloatPoint resolvePosition(const Circle& value, FloatSize boundingBox)
{
    return value.position ? evaluate(*value.position, boundingBox) : FloatPoint { boundingBox.width() / 2, boundingBox.height() / 2 };
}

float resolveRadius(const Circle& value, FloatSize boxSize, FloatPoint center)
{
    return WTF::switchOn(value.radius,
        [&](const Circle::Length& length) -> float {
            return evaluate(length, boxSize.diagonalLength() / sqrtOfTwoFloat);
        },
        [&](const Circle::Extent& extent) -> float {
            return WTF::switchOn(extent,
                [&](CSS::ClosestSide) -> float {
                    return distanceToClosestSide(center, boxSize);
                },
                [&](CSS::FarthestSide) -> float {
                    return distanceToFarthestSide(center, boxSize);
                },
                [&](CSS::ClosestCorner) -> float {
                    return distanceToClosestCorner(center, boxSize);
                },
                [&](CSS::FarthestCorner) -> float {
                    return distanceToFarthestCorner(center, boxSize);
                }
            );
        }
    );
}

WebCore::Path pathForCenterCoordinate(const Circle& value, const FloatRect& boundingBox, FloatPoint center)
{
    auto radius = resolveRadius(value, boundingBox.size(), center);
    auto bounding = FloatRect {
        center.x() - radius + boundingBox.x(),
        center.y() - radius + boundingBox.y(),
        radius * 2,
        radius * 2
    };
    return cachedCirclePath(bounding);
}

WebCore::Path PathComputation<Circle>::operator()(const Circle& value, const FloatRect& boundingBox)
{
    return pathForCenterCoordinate(value, boundingBox, resolvePosition(value, boundingBox.size()));
}

// MARK: - Blending

auto Blending<Circle>::canBlend(const Circle& a, const Circle& b) -> bool
{
    auto canBlendRadius = [](const auto& radiusA, const auto& radiusB) {
        return std::visit(WTF::makeVisitor(
            [](const Circle::Length&, const Circle::Length&) {
                return true;
            },
            [](const auto&, const auto&) {
                // FIXME: Determine how to interpolate between keywords. See bug 125108.
                return false;
            }
        ), radiusA, radiusB);
    };

    return canBlendRadius(a.radius, b.radius)
        && WebCore::Style::canBlend(a.position, b.position);
}

auto Blending<Circle>::blend(const Circle& a, const Circle& b, const BlendingContext& context) -> Circle
{
    auto blendRadius = [](const auto& radiusA, const auto& radiusB, const BlendingContext& context) -> Circle::RadialSize {
        return std::visit(WTF::makeVisitor(
            [&](const Circle::Length& lengthA, const Circle::Length& lengthB) -> Circle::RadialSize {
                return WebCore::Style::blend(lengthA, lengthB, context);
            },
            [&](const auto& a, const auto&) -> Circle::RadialSize {
                return a;
            }
        ), radiusA, radiusB);
    };

    return {
        .radius = blendRadius(a.radius, b.radius, context),
        .position = WebCore::Style::blend(a.position, b.position, context),
    };
}

} // namespace Style
} // namespace WebCore
