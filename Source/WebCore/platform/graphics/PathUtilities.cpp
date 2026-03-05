/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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
#include "PathUtilities.h"

#include "AffineTransform.h"
#include "FloatPointGraph.h"
#include "FloatRect.h"
#include "FloatRoundedRect.h"
#include "GeometryUtilities.h"
#include "Path.h"
#include <math.h>
#include <numbers>
#include <ranges>
#include <wtf/MathExtras.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

Vector<Path> PathUtilities::pathsWithShrinkWrappedRects(const Vector<FloatRect>& rects, float radius)
{
    if (rects.isEmpty())
        return { };

    if (rects.size() > 20) {
        Path path;
        for (const auto& rect : rects)
            path.addRoundedRect(rect, FloatSize(radius, radius));
        return { WTF::move(path) };
    }

    auto [graph, polys] = FloatPointGraph::polygonsForRect(rects);
    if (polys.isEmpty()) {
        Path path;
        for (const auto& rect : rects)
            path.addRoundedRect(rect, FloatSize(radius, radius));
        return { WTF::move(path) };
    }

    return WTF::map(polys, [&](auto& poly) {
        Path path;
        for (unsigned i = 0; i < poly.size(); ++i) {
            FloatPointGraph::Edge& toEdge = poly[i];
            // Connect the first edge to the last.
            FloatPointGraph::Edge& fromEdge = (i > 0) ? poly[i - 1] : poly[poly.size() - 1];

            FloatPoint fromEdgeVec = toFloatPoint(*fromEdge.second - *fromEdge.first);
            FloatPoint toEdgeVec = toFloatPoint(*toEdge.second - *toEdge.first);

            // Clamp the radius to no more than half the length of either adjacent edge,
            // because we want a smooth curve and don't want unequal radii.
            float clampedRadius = std::min(radius, fromEdgeVec.length() / 2);
            clampedRadius = std::min(clampedRadius, toEdgeVec.length() / 2);

            FloatPoint fromEdgeNorm = fromEdgeVec;
            fromEdgeNorm.normalize();
            FloatPoint toEdgeNorm = toEdgeVec;
            toEdgeNorm.normalize();

            // Project the radius along the incoming and outgoing edge.
            FloatSize fromOffset = clampedRadius * toFloatSize(fromEdgeNorm);
            FloatSize toOffset = clampedRadius * toFloatSize(toEdgeNorm);

            if (!i)
                path.moveTo(*fromEdge.second - fromOffset);
            else
                path.addLineTo(*fromEdge.second - fromOffset);
            path.addArcTo(*fromEdge.second, *toEdge.first + toOffset, clampedRadius);
        }
        path.closeSubpath();
        return path;
    });
}

Path PathUtilities::pathWithShrinkWrappedRects(const Vector<FloatRect>& rects, float radius)
{
    Vector<Path> paths = pathsWithShrinkWrappedRects(rects, radius);

    Path unionPath;
    for (const auto& path : paths)
        unionPath.addPath(path, AffineTransform());

    return unionPath;
}

Path PathUtilities::pathWithShrinkWrappedRects(const Vector<FloatRect>& rects, const CornerRadii& radii)
{
    if (radii.isUniformCornerRadius())
        return pathWithShrinkWrappedRects(rects, radii.topLeft().width());

    // FIXME: This could potentially take non-uniform radii into account when running the
    // shrink-wrap algorithm above, by averaging corner radii between adjacent edges.
    Path path;
    for (auto& rect : rects)
        path.addRoundedRect(FloatRoundedRect { rect, radii });
    return path;
}

} // namespace WebCore
