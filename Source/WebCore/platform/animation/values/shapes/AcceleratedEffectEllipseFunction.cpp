/*
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
#include "AcceleratedEffectEllipseFunction.h"

#if ENABLE(THREADED_ANIMATIONS)

#include "AcceleratedEffectAnimationUtilities.h"
#include "GeometryUtilities.h"
#include "Path.h"
#include <wtf/TinyLRUCache.h>

namespace WebCore {

// MARK: - Path Caching

struct AcceleratedEffectEllipsePathPolicy final : public TinyLRUCachePolicy<FloatRect, WebCore::Path> {
public:
    static bool NODELETE isKeyNull(const FloatRect& rect)
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

static const WebCore::Path& cachedAcceleratedEffectEllipsePath(const FloatRect& rect)
{
    static NeverDestroyed<TinyLRUCache<FloatRect, WebCore::Path, 4, AcceleratedEffectEllipsePathPolicy>> cache;
    return cache.get().get(rect);
}

// MARK: - Path Evaluation

static FloatPoint resolvePosition(const AcceleratedEffectEllipseFunction& value, FloatSize boundingBox)
{
    return value.position ? *value.position : FloatPoint { boundingBox.width() / 2, boundingBox.height() / 2 };
}

static FloatSize resolveRadii(const AcceleratedEffectEllipseFunction& value, FloatSize boxSize, FloatPoint center)
{
    auto sizeForAxis = [&](const AcceleratedEffectEllipseFunction::RadialSize& radius, float centerValue, float dimensionSize) {
        return WTF::switchOn(radius,
            [&](float length) -> float {
                return length;
            },
            [&](AcceleratedEffectEllipseFunction::Extent extent) -> float {
                switch (extent) {
                case AcceleratedEffectEllipseFunction::Extent::ClosestSide:
                    return std::min(std::abs(centerValue), std::abs(dimensionSize - centerValue));
                case AcceleratedEffectEllipseFunction::Extent::FarthestSide:
                    return std::max(std::abs(centerValue), std::abs(dimensionSize - centerValue));
                case AcceleratedEffectEllipseFunction::Extent::ClosestCorner:
                    return distanceToClosestCorner(center, boxSize);
                case AcceleratedEffectEllipseFunction::Extent::FarthestCorner:
                    return distanceToFarthestCorner(center, boxSize);
                }
                RELEASE_ASSERT_NOT_REACHED();
            }
        );
    };

    return {
        sizeForAxis(get<0>(value.radii), center.x(), boxSize.width()),
        sizeForAxis(get<1>(value.radii), center.y(), boxSize.height())
    };
}

Path pathForCenterCoordinate(const AcceleratedEffectEllipseFunction& value, const FloatRect& boundingBox, FloatPoint center)
{
    auto radii = resolveRadii(value, boundingBox.size(), center);
    auto bounding = FloatRect {
        center.x() - radii.width() + boundingBox.x(),
        center.y() - radii.height() + boundingBox.y(),
        radii.width() * 2,
        radii.height() * 2
    };
    return cachedAcceleratedEffectEllipsePath(bounding);
}

Path path(const AcceleratedEffectEllipseFunction& value, const FloatRect& boundingBox)
{
    return pathForCenterCoordinate(value, boundingBox, resolvePosition(value, boundingBox.size()));
}

// MARK: - Blending

bool AcceleratedEffectInterpolation<AcceleratedEffectEllipseFunction>::canBlend(const AcceleratedEffectEllipseFunction& from, const AcceleratedEffectEllipseFunction& to)
{
    auto canBlendRadius = [](const auto& fromRadius, const auto& toRadius) {
        // FIXME: Determine how to interpolate between keywords. See bug 125108.
        return WTF::holdsAlternative<float>(fromRadius)
            && WTF::holdsAlternative<float>(toRadius);
    };

    return canBlendRadius(get<0>(from.radii), get<0>(to.radii))
        && canBlendRadius(get<1>(from.radii), get<1>(to.radii))
        && WebCore::canBlendForAcceleratedEffect(from.position, to.position);
}

AcceleratedEffectEllipseFunction AcceleratedEffectInterpolation<AcceleratedEffectEllipseFunction>::blend(const AcceleratedEffectEllipseFunction& from, const AcceleratedEffectEllipseFunction& to, const BlendingContext& context)
{
    auto blendRadius = [](const auto& fromRadius, const auto& toRadius, const BlendingContext& context) -> AcceleratedEffectEllipseFunction::RadialSize {
        return WTF::visit(WTF::makeVisitor(
            [&](float lengthA, float lengthB) -> AcceleratedEffectEllipseFunction::RadialSize {
                return WebCore::blendForAcceleratedEffect(lengthA, lengthB, context);
            },
            [&](const AcceleratedEffectEllipseFunction::Extent& extent, const AcceleratedEffectEllipseFunction::Extent&) -> AcceleratedEffectEllipseFunction::RadialSize {
                return extent;
            },
            [&](float, const AcceleratedEffectEllipseFunction::Extent&) -> AcceleratedEffectEllipseFunction::RadialSize {
                RELEASE_ASSERT_NOT_REACHED();
            },
            [&](const AcceleratedEffectEllipseFunction::Extent&, float) -> AcceleratedEffectEllipseFunction::RadialSize {
                RELEASE_ASSERT_NOT_REACHED();
            }
        ), fromRadius, toRadius);
    };

    return {
        .radii = {
            blendRadius(get<0>(from.radii), get<0>(to.radii), context),
            blendRadius(get<1>(from.radii), get<1>(to.radii), context)
        },
        .position = WebCore::blendForAcceleratedEffect(from.position, to.position, context),
    };
}

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATIONS)
