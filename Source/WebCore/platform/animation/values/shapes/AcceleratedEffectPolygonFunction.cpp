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
#include "AcceleratedEffectPolygonFunction.h"

#if ENABLE(THREADED_ANIMATIONS)

#include "AcceleratedEffectAnimationUtilities.h"
#include "Path.h"
#include <wtf/TinyLRUCache.h>
#include <wtf/ZippedRange.h>

namespace WebCore {

static WindRule windRule(const AcceleratedEffectPolygonFunction& value)
{
    return value.fillRule.value_or(WindRule::NonZero);
}

// MARK: - Path Caching

struct AcceleratedEffectPolygonPathPolicy : TinyLRUCachePolicy<Vector<FloatPoint>, WebCore::Path> {
public:
    static bool NODELETE isKeyNull(const Vector<FloatPoint>& points)
    {
        return !points.size();
    }

    static WebCore::Path createValueForKey(const Vector<FloatPoint>& points)
    {
        return WebCore::Path(points);
    }
};

static const WebCore::Path& cachedAcceleratedEffectPolygonPath(const Vector<FloatPoint>& points)
{
    static NeverDestroyed<TinyLRUCache<Vector<FloatPoint>, WebCore::Path, 4, AcceleratedEffectPolygonPathPolicy>> cache;
    return cache.get().get(points);
}

// MARK: - Path Evaluation

Path path(const AcceleratedEffectPolygonFunction& value, const FloatRect& boundingBox)
{
    auto boundingLocation = boundingBox.location();
    auto points = value.vertices.map([&](const auto& vertex) {
        return vertex + boundingLocation;
    });
    return cachedAcceleratedEffectPolygonPath(points);
}

// MARK: - Blending

static Vector<FloatPoint> blendVertices(const Vector<FloatPoint>& from, const Vector<FloatPoint>& to, const BlendingContext& context)
{
    ASSERT(from.size() == to.size());

    Vector<FloatPoint> result;
    result.reserveInitialCapacity(from.size());
    for (auto&& [fromVertex, toVertex] : zippedRange(from, to))
        result.append(WebCore::blendForAcceleratedEffect(fromVertex, toVertex, context));
    return result;
}

bool AcceleratedEffectInterpolation<AcceleratedEffectPolygonFunction>::canBlend(const AcceleratedEffectPolygonFunction& from, const AcceleratedEffectPolygonFunction& to)
{
    return windRule(from) == windRule(to)
        && from.vertices.size() == to.vertices.size();
}

AcceleratedEffectPolygonFunction AcceleratedEffectInterpolation<AcceleratedEffectPolygonFunction>::blend(const AcceleratedEffectPolygonFunction& from, const AcceleratedEffectPolygonFunction& to, const BlendingContext& context)
{
    return {
        .fillRule = from.fillRule,
        .vertices = blendVertices(from.vertices, to.vertices, context),
    };
}

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATIONS)
