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
#include "AcceleratedEffectCircleFunction.h"

#if ENABLE(THREADED_ANIMATIONS)

#include "AcceleratedEffectAnimationUtilities.h"
#include "GeometryUtilities.h"
#include "Path.h"
#include <wtf/TinyLRUCache.h>

namespace WebCore {

// MARK: - Path Caching

struct AcceleratedEffectCirclePathPolicy final : public TinyLRUCachePolicy<FloatRect, WebCore::Path> {
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

static const WebCore::Path& cachedAcceleratedEffectCirclePath(const FloatRect& rect)
{
    static NeverDestroyed<TinyLRUCache<FloatRect, WebCore::Path, 4, AcceleratedEffectCirclePathPolicy>> cache;
    return cache.get().get(rect);
}

// MARK: - Path Evaluation

static FloatPoint resolvePosition(const AcceleratedEffectCircleFunction& value, FloatSize boundingBox)
{
    return value.position ? *value.position : FloatPoint { boundingBox.width() / 2, boundingBox.height() / 2 };
}

static float resolveRadius(const AcceleratedEffectCircleFunction& value, FloatSize boxSize, FloatPoint center)
{
    return WTF::switchOn(value.radius,
        [](float length) -> float {
            return length;
        },
        [&](AcceleratedEffectCircleFunction::Extent extent) -> float {
            switch (extent) {
            case AcceleratedEffectCircleFunction::Extent::ClosestSide:
                return distanceToClosestSide(center, boxSize);
            case AcceleratedEffectCircleFunction::Extent::FarthestSide:
                return distanceToFarthestSide(center, boxSize);
            case AcceleratedEffectCircleFunction::Extent::ClosestCorner:
                return distanceToClosestCorner(center, boxSize);
            case AcceleratedEffectCircleFunction::Extent::FarthestCorner:
                return distanceToFarthestCorner(center, boxSize);
            }
            RELEASE_ASSERT_NOT_REACHED();
        }
    );
}

Path pathForCenterCoordinate(const AcceleratedEffectCircleFunction& value, const FloatRect& boundingBox, FloatPoint center)
{
    auto radius = resolveRadius(value, boundingBox.size(), center);
    auto bounding = FloatRect {
        center.x() - radius + boundingBox.x(),
        center.y() - radius + boundingBox.y(),
        radius * 2,
        radius * 2
    };
    return cachedAcceleratedEffectCirclePath(bounding);
}

Path path(const AcceleratedEffectCircleFunction& value, const FloatRect& boundingBox)
{
    return pathForCenterCoordinate(value, boundingBox, resolvePosition(value, boundingBox.size()));
}

// MARK: - Blending

bool AcceleratedEffectInterpolation<AcceleratedEffectCircleFunction>::canBlend(const AcceleratedEffectCircleFunction& from, const AcceleratedEffectCircleFunction& to)
{
    auto canBlendRadius = [](const auto& fromRadius, const auto& toRadius) {
        // FIXME: Determine how to interpolate between keywords. See bug 125108.
        return WTF::holdsAlternative<float>(fromRadius)
            && WTF::holdsAlternative<float>(toRadius);
    };

    return canBlendRadius(from.radius, to.radius)
        && WebCore::canBlendForAcceleratedEffect(from.position, to.position);
}

AcceleratedEffectCircleFunction AcceleratedEffectInterpolation<AcceleratedEffectCircleFunction>::blend(const AcceleratedEffectCircleFunction& from, const AcceleratedEffectCircleFunction& to, const BlendingContext& context)
{
    auto blendRadius = [](const auto& fromRadius, const auto& toRadius, const BlendingContext& context) -> AcceleratedEffectCircleFunction::RadialSize {
        return WTF::visit(WTF::makeVisitor(
            [&](float lengthA, float lengthB) -> AcceleratedEffectCircleFunction::RadialSize {
                return WebCore::blend(lengthA, lengthB, context);
            },
            [&](const AcceleratedEffectCircleFunction::Extent& extent, const AcceleratedEffectCircleFunction::Extent&) -> AcceleratedEffectCircleFunction::RadialSize {
                return extent;
            },
            [&](float, const AcceleratedEffectCircleFunction::Extent&) -> AcceleratedEffectCircleFunction::RadialSize {
                RELEASE_ASSERT_NOT_REACHED();
            },
            [&](const AcceleratedEffectCircleFunction::Extent&, float) -> AcceleratedEffectCircleFunction::RadialSize {
                RELEASE_ASSERT_NOT_REACHED();
            }
        ), fromRadius, toRadius);
    };

    return {
        .radius = blendRadius(from.radius, to.radius, context),
        .position = WebCore::blendForAcceleratedEffect(from.position, to.position, context),
    };
}

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATIONS)
