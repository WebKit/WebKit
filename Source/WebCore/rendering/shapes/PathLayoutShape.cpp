/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "PathLayoutShape.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace WebCore {

LayoutRect PathLayoutShape::shapeMarginLogicalBoundingBox() const
{
    auto bounds = m_bounds;
    bounds.inflate(shapeMargin());
    return LayoutRect(bounds);
}

LineSegment PathLayoutShape::getExcludedInterval(LayoutUnit logicalTop, LayoutUnit logicalHeight) const
{
    float y1 = logicalTop;
    float y2 = logicalTop + logicalHeight;

    auto margin = shapeMargin();
    if (m_polylines.isEmpty() || !m_bounds.overlapsYRange(y1 - margin, y2 + margin))
        return { };

    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();

    auto includePoint = [&](FloatPoint point) {
        auto verticalGap = std::max({ 0.0f, y1 - point.y(), point.y() - y2 });
        if (verticalGap > margin)
            return;
        auto reach = margin ? std::sqrt(margin * margin - verticalGap * verticalGap) : 0.0f;
        minX = std::min(minX, point.x() - reach);
        maxX = std::max(maxX, point.x() + reach);
    };

    for (auto& polyline : m_polylines) {
        for (size_t index = 0; index + 1 < polyline.size(); ++index) {
            auto start = polyline[index];
            auto end = polyline[index + 1];

            if (!margin) {
                if (start.y() >= y1 && start.y() <= y2)
                    includePoint(start);

                if (start.y() != end.y()) {
                    for (float y : { y1, y2 }) {
                        if (y < std::min(start.y(), end.y()) || y > std::max(start.y(), end.y()))
                            continue;
                        auto fraction = (y - start.y()) / (end.y() - start.y());
                        includePoint({ start.x() + fraction * (end.x() - start.x()), y });
                    }
                }
                continue;
            }

            includePoint(start);
            auto span = end - start;
            auto steps = static_cast<unsigned>(std::ceil(span.diagonalLength()));
            for (unsigned step = 1; step < steps; ++step)
                includePoint(start + span.scaled(float(step) / steps));
        }
        if (!polyline.isEmpty())
            includePoint(polyline.last());
    }

    if (minX > maxX)
        return { };
    return { minX, maxX };
}

void PathLayoutShape::buildDisplayPaths(DisplayPaths& paths) const
{
    for (auto& polyline : m_polylines) {
        if (polyline.isEmpty())
            continue;
        paths.shape.moveTo(polyline.first());
        for (size_t index = 1; index < polyline.size(); ++index)
            paths.shape.addLineTo(polyline[index]);
        paths.shape.closeSubpath();
    }
}

} // namespace WebCore
