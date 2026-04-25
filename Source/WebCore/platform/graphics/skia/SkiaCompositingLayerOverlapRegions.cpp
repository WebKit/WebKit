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

#if USE(COORDINATED_GRAPHICS) && USE(SKIA)
#include "FloatRect.h"
#include "TransformationMatrix.h"
#include <array>

namespace WebCore {

template<typename T>
struct Point4D {
    T x;
    T y;
    T z;
    T w;
};

template<typename T>
Point4D<T> operator+(const Point4D<T>& s, const Point4D<T>& t)
{
    return { s.x + t.x, s.y + t.y, s.z + t.z, s.w + t.w };
}

template<typename T>
Point4D<T> operator-(const Point4D<T>& s, const Point4D<T>& t)
{
    return { s.x - t.x, s.y - t.y, s.z - t.z, s.w - t.w };
}

template<typename T>
Point4D<T> operator*(T s, const Point4D<T>& t)
{
    return { s * t.x, s * t.y, s * t.z, s * t.w };
}

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

IntRect ComputeOverlapRegionData::transformedBoundingBox(const TransformationMatrix& transform, const FloatRect& rect) const
{
    using Point = Point4D<double>;
    auto mapPoint = [&](const FloatPoint& p) -> Point {
        double x = p.x();
        double y = p.y();
        double z = 0;
        double w = 1;
        transform.map4ComponentPoint(x, y, z, w);
        return { x, y, z, w };
    };

    std::array<Point, 4> vertex = {
        mapPoint(rect.minXMinYCorner()),
        mapPoint(rect.maxXMinYCorner()),
        mapPoint(rect.maxXMaxYCorner()),
        mapPoint(rect.minXMaxYCorner())
    };
    std::array<bool, 4> isPositive = {
        vertex[0].w >= 0,
        vertex[1].w >= 0,
        vertex[2].w >= 0,
        vertex[3].w >= 0
    };

    auto findFirstPositiveVertex = [&]() {
        int i = 0;
        for (; i < 4; i++) {
            if (isPositive[i])
                return i;
        }
        return i;
    };
    auto findFirstNegativeVertex = [&]() {
        int i = 0;
        for (; i < 4; i++) {
            if (!isPositive[i])
                return i;
        }
        return i;
    };

    auto leftVertexIndex = [](int i) {
        return (i + 1) % 4;
    };
    auto diagonalVertexIndex = [](int i) {
        return (i + 2) % 4;
    };
    auto rightVertexIndex = [](int i) {
        return (i + 3) % 4;
    };

    auto minMax2 = [](double d1, double d2) -> MinMax<double> {
        if (d1 < d2)
            return { d1, d2 };
        return { d2, d1 };
    };
    auto minMax3 = [&](double d1, double d2, double d3) -> MinMax<double> {
        auto minmax = minMax2(d1, d2);
        return { std::min(minmax.min, d3), std::max(minmax.max, d3) };
    };
    auto minMax4 = [&](double d1, double d2, double d3, double d4) -> MinMax<double> {
        auto minmax1 = minMax2(d1, d2);
        auto minmax2 = minMax2(d3, d4);
        return { std::min(minmax1.min, minmax2.min), std::max(minmax1.max, minmax2.max) };
    };

    auto clipped = [&](const MinMax<double>& xMinMax, const MinMax<double>& yMinMax) -> IntRect {
        int minX = std::max<double>(xMinMax.min, clipBounds.x());
        int minY = std::max<double>(yMinMax.min, clipBounds.y());
        int maxX = std::min<double>(xMinMax.max, clipBounds.maxX());
        int maxY = std::min<double>(yMinMax.max, clipBounds.maxY());
        return { minX, minY, maxX - minX, maxY - minY };
    };

    auto toPositive = [&](const Point& positive, const Point& negative) -> Point {
        ASSERT(positive.w > 0);
        ASSERT(negative.w <= 0);
        auto v = positive.w * negative - negative.w * positive;
        v.w = 0;
        return v;
    };
    auto boundingBoxPPP = [&](const Point& p1, const Point& p2, const Point& p3) -> IntRect {
        ASSERT(p1.w >= 0);
        ASSERT(p2.w >= 0);
        ASSERT(p3.w >= 0);
        auto xMinMax = minMax3(p1.x / p1.w, p2.x / p2.w, p3.x / p3.w);
        auto yMinMax = minMax3(p1.y / p1.w, p2.y / p2.w, p3.y / p3.w);
        return clipped(xMinMax, yMinMax);
    };
    auto boundingBoxPPN = [&](const Point& p1, const Point& p2, const Point& n3) -> IntRect {
        return boundingBoxPPP(p1, p2, toPositive(p1, n3));
    };
    auto boundingBoxPNN = [&](const Point& p1, const Point& n2, const Point& n3) -> IntRect {
        return boundingBoxPPP(p1, toPositive(p1, n2), toPositive(p1, n3));
    };

    auto boundingBoxPPPByIndex = [&](int i1, int i2, int i3) {
        return boundingBoxPPP(vertex[i1], vertex[i2], vertex[i3]);
    };
    auto boundingBoxPPNByIndex = [&](int i1, int i2, int i3) {
        return boundingBoxPPN(vertex[i1], vertex[i2], vertex[i3]);
    };
    auto boundingBoxPNNByIndex = [&](int i1, int i2, int i3) {
        return boundingBoxPNN(vertex[i1], vertex[i2], vertex[i3]);
    };

    int count = isPositive[0] + isPositive[1] + isPositive[2] + isPositive[3];
    switch (count) {
    case 0:
        return { };
    case 1: {
        int i = findFirstPositiveVertex();
        ASSERT(i < 4);
        return boundingBoxPNNByIndex(i, rightVertexIndex(i), leftVertexIndex(i));
    }
    case 2: {
        int i = findFirstPositiveVertex();
        ASSERT(i < 3);
        if (!i && isPositive[3])
            i = 3;
        int positiveRightIndex = i;
        int positiveLeftIndex = leftVertexIndex(i);
        ASSERT(isPositive[positiveLeftIndex]);
        return unionRect(boundingBoxPPNByIndex(positiveRightIndex, positiveLeftIndex, leftVertexIndex(positiveLeftIndex)), boundingBoxPPNByIndex(positiveRightIndex, positiveLeftIndex, rightVertexIndex(positiveRightIndex)));
    }
    case 3: {
        int i = findFirstNegativeVertex();
        ASSERT(i < 4);
        return unionRect(boundingBoxPPNByIndex(leftVertexIndex(i), rightVertexIndex(i), i), boundingBoxPPPByIndex(leftVertexIndex(i), rightVertexIndex(i), diagonalVertexIndex(i)));
    }
    case 4: {
        auto xMinMax = minMax4(vertex[0].x / vertex[0].w, vertex[1].x / vertex[1].w, vertex[2].x / vertex[2].w, vertex[3].x / vertex[3].w);
        auto yMinMax = minMax4(vertex[0].y / vertex[0].w, vertex[1].y / vertex[1].w, vertex[2].y / vertex[2].w, vertex[3].y / vertex[3].w);
        return clipped(xMinMax, yMinMax);
    }
    }
    ASSERT_NOT_REACHED();
    return { };
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
