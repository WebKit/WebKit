/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2022 Sony Interactive Entertainment Inc.
 * Copyright (C) 2026 Igalia S.L.
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
#include "SkiaCompositingLayerOverlapRegions.h"

#if USE(COORDINATED_GRAPHICS) && USE(SKIA) && !USE(TEXTURE_MAPPER)
#include "FloatRect.h"
#include "LayoutRect.h"
#include "Polygon4D.h"
#include "TransformationMatrix.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <ranges>

namespace WebCore {

template<typename T>
struct MinMax {
    T min;
    T max;
};

void ComputeOverlapRegionData::resolveOverlaps(const IntRect& newRegion)
{
    Region newOverlapRegion(newRegion);
    newOverlapRegion.intersect(nonOverlapRegion);
    nonOverlapRegion.subtract(newOverlapRegion);
    overlapRegion.unite(newOverlapRegion);

    Region newNonOverlapRegion(newRegion);
    newNonOverlapRegion.subtract(overlapRegion);
    nonOverlapRegion.unite(newNonOverlapRegion);
}

IntRect projectedBoundingBox(const TransformationMatrix& transform, const FloatRect& rect, const IntRect& clipBounds)
{
    if (clipBounds.isEmpty())
        return { };

    // Clip before dividing by w, so edges running to infinity stop at the clip bounds.
    auto vertices = Polygon4D::clipToFrontOfCamera(rect, transform);
    const std::array<Point4D, 4> clipPlanes {
        Point4D { 1, 0, 0, -static_cast<double>(clipBounds.x()) },
        Point4D { -1, 0, 0, static_cast<double>(clipBounds.maxX()) },
        Point4D { 0, 1, 0, -static_cast<double>(clipBounds.y()) },
        Point4D { 0, -1, 0, static_cast<double>(clipBounds.maxY()) }
    };
    for (const auto& plane : clipPlanes)
        vertices = Polygon4D::clipToPlane(vertices, plane);

    MinMax<double> xMinMax { std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity() };
    MinMax<double> yMinMax = xMinMax;
    for (const auto& point : vertices) {
        const double x = point.x / point.w;
        const double y = point.y / point.w;
        xMinMax = { std::min(xMinMax.min, x), std::max(xMinMax.max, x) };
        yMinMax = { std::min(yMinMax.min, y), std::max(yMinMax.max, y) };
    }

    if (xMinMax.min > xMinMax.max)
        return { };

    int minX = std::floor(std::clamp<double>(xMinMax.min, clipBounds.x(), clipBounds.maxX()));
    int minY = std::floor(std::clamp<double>(yMinMax.min, clipBounds.y(), clipBounds.maxY()));
    int maxX = std::ceil(std::clamp<double>(xMinMax.max, clipBounds.x(), clipBounds.maxX()));
    int maxY = std::ceil(std::clamp<double>(yMinMax.max, clipBounds.y(), clipBounds.maxY()));
    return { minX, minY, maxX - minX, maxY - minY };
}

IntRect layerPlaneClipBounds(const TransformationMatrix& deviceToLayer, const FloatRect& deviceClip, const Function<bool(const IntRect&)>& fits)
{
    // A device point maps into the layer plane with w = a * x + b * y + c, which is one over the depth of that
    // point. The plane is visible only where w > 0, and close to its horizon at w = 0 a single device pixel covers
    // an unbounded part of the plane. Keep only what stays at least one device pixel away from the horizon, and
    // when that does not fit, give up the part of the plane that is farthest away first.
    const double a = deviceToLayer.m14();
    const double b = deviceToLayer.m24();
    const double c = deviceToLayer.m44();
    const std::array<FloatPoint, 4> corners { deviceClip.minXMinYCorner(), deviceClip.maxXMinYCorner(), deviceClip.maxXMaxYCorner(), deviceClip.minXMaxYCorner() };
    auto wAt = [&](const FloatPoint& p) {
        return a * p.x() + b * p.y() + c;
    };

    auto [minCornerW, maxCornerW] = std::ranges::minmax(corners | std::views::transform(wAt));
    if (maxCornerW <= 0)
        return { };

    auto clipBounds = [&](double minW) -> IntRect {
        MinMax<double> xMinMax { std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity() };
        MinMax<double> yMinMax = xMinMax;
        auto include = [&](const FloatPoint& p) {
            const auto mapped = deviceToLayer.mapPoint(p);
            xMinMax = { std::min<double>(xMinMax.min, mapped.x()), std::max<double>(xMinMax.max, mapped.x()) };
            yMinMax = { std::min<double>(yMinMax.min, mapped.y()), std::max<double>(yMinMax.max, mapped.y()) };
        };

        for (size_t i = 0; i < corners.size(); ++i) {
            const auto& p = corners[i];
            const auto& q = corners[(i + 1) % corners.size()];
            const double dp = wAt(p) - minW;
            const double dq = wAt(q) - minW;
            if (dp >= 0)
                include(p);
            if ((dp >= 0) != (dq >= 0))
                include(p + (q - p) * static_cast<float>(dp / (dp - dq)));
        }
        if (xMinMax.min > xMinMax.max)
            return { };

        const auto bounds = enclosingIntRect(LayoutRect::infiniteRect());
        int minX = std::floor(std::clamp<double>(xMinMax.min, bounds.x(), bounds.maxX()));
        int minY = std::floor(std::clamp<double>(yMinMax.min, bounds.y(), bounds.maxY()));
        int maxX = std::ceil(std::clamp<double>(xMinMax.max, bounds.x(), bounds.maxX()));
        int maxY = std::ceil(std::clamp<double>(yMinMax.max, bounds.y(), bounds.maxY()));
        return { minX, minY, maxX - minX, maxY - minY };
    };

    double nearW = maxCornerW;
    double farW = std::max(minCornerW, std::hypot(a, b));
    auto bounds = clipBounds(farW);
    if (!fits || fits(bounds) || !fits(clipBounds(nearW)))
        return bounds;

    // Bisect in depth, which is one over w.
    for (unsigned i = 0; i < 16; ++i) {
        const double w = std::sqrt(nearW * farW);
        if (fits(clipBounds(w)))
            nearW = w;
        else
            farW = w;
    }
    return clipBounds(nearW);
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA) && !USE(TEXTURE_MAPPER)
