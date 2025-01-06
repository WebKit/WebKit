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
#include "StylePolygonFunction.h"

#include "FloatRect.h"
#include "GeometryUtilities.h"
#include "Path.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include <wtf/TinyLRUCache.h>

namespace WebCore {
namespace Style {

// MARK: - Path Caching

struct PolygonPathPolicy : TinyLRUCachePolicy<Vector<FloatPoint>, WebCore::Path> {
public:
    static bool isKeyNull(const Vector<FloatPoint>& points)
    {
        return !points.size();
    }

    static WebCore::Path createValueForKey(const Vector<FloatPoint>& points)
    {
        return WebCore::Path(points);
    }
};

static const WebCore::Path& cachedPolygonPath(const Vector<FloatPoint>& points)
{
    static NeverDestroyed<TinyLRUCache<Vector<FloatPoint>, WebCore::Path, 4, PolygonPathPolicy>> cache;
    return cache.get().get(points);
}

// MARK: - Path

WebCore::Path PathComputation<Polygon>::operator()(const Polygon& value, const FloatRect& boundingBox)
{
    auto boundingLocation = boundingBox.location();
    auto boundingSize = boundingBox.size();
    auto points = value.vertices.value.map([&](const auto& vertex) -> FloatPoint {
        return evaluate(vertex, boundingSize) + boundingLocation;
    });
    return cachedPolygonPath(points);
}

// MARK: - Wind Rule

WebCore::WindRule WindRuleComputation<Polygon>::operator()(const Polygon& value)
{
    return (!value.fillRule || std::holds_alternative<CSS::Keyword::Nonzero>(*value.fillRule)) ? WindRule::NonZero : WindRule::EvenOdd;
}

// MARK: - Blending

auto Blending<Polygon>::canBlend(const Polygon& a, const Polygon& b) -> bool
{
    return windRule(a) == windRule(b)
        && a.vertices.size() == b.vertices.size();
}

auto Blending<Polygon>::blend(const Polygon& a, const Polygon& b, const BlendingContext& context) -> Polygon
{
    return {
        .fillRule = a.fillRule,
        .vertices = WebCore::Style::blend(a.vertices, b.vertices, context),
    };
}

} // namespace Style
} // namespace WebCore
