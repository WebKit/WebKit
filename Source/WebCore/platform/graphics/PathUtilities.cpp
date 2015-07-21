/*
 * Copyright (C) 2014-2015 Apple Inc.  All rights reserved.
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

#include "FloatPoint.h"
#include "FloatRect.h"
#include <math.h>

namespace WebCore {

static void addShrinkWrapRightCorner(Path& path, const FloatRect* fromRect, const FloatRect* toRect, float radius)
{
    FloatSize horizontalRadius(radius, 0);
    FloatSize verticalRadius(0, radius);

    if (!fromRect) {
        // For the first (top) rect:

        path.moveTo(toRect->minXMinYCorner() + horizontalRadius);

        // Across the top, towards the right.
        path.addLineTo(toRect->maxXMinYCorner() - horizontalRadius);

        // Arc the top corner.
        path.addArcTo(toRect->maxXMinYCorner(), toRect->maxXMinYCorner() + verticalRadius, radius);
    } else if (!toRect) {
        // For the last rect:

        // Down the right.
        path.addLineTo(fromRect->maxXMaxYCorner() - verticalRadius);

        // Arc the bottom corner.
        path.addArcTo(fromRect->maxXMaxYCorner(), fromRect->maxXMaxYCorner() - horizontalRadius, radius);
    } else {
        // For middle rects:

        float rightEdgeDifference = toRect->maxX() - fromRect->maxX();

        // Skip over rects with equal edges, because we can't make
        // sensible curves between them.
        if (fabsf(rightEdgeDifference) < std::numeric_limits<float>::epsilon())
            return;

        if (rightEdgeDifference < 0) {
            float effectiveY = std::max(toRect->y(), fromRect->maxY());
            FloatPoint toRectMaxXMinYCorner = FloatPoint(toRect->maxX(), effectiveY);

            // Down the right.
            path.addLineTo(FloatPoint(fromRect->maxX(), effectiveY) - verticalRadius);

            // Arc the outer corner.
            path.addArcTo(FloatPoint(fromRect->maxX(), effectiveY), FloatPoint(fromRect->maxX(), effectiveY) - horizontalRadius, radius);

            // Across the bottom, towards the left.
            path.addLineTo(toRectMaxXMinYCorner + horizontalRadius);

            // Arc the inner corner.
            path.addArcTo(toRectMaxXMinYCorner, toRectMaxXMinYCorner + verticalRadius, radius);
        } else {
            float effectiveY = std::min(toRect->y(), fromRect->maxY());
            FloatPoint toRectMaxXMinYCorner = FloatPoint(toRect->maxX(), effectiveY);

            // Down the right.
            path.addLineTo(FloatPoint(fromRect->maxX(), effectiveY) - verticalRadius);

            // Arc the inner corner.
            path.addArcTo(FloatPoint(fromRect->maxX(), effectiveY), FloatPoint(fromRect->maxX(), effectiveY) + horizontalRadius, radius);

            // Across the bottom, towards the right.
            path.addLineTo(toRectMaxXMinYCorner - horizontalRadius);

            // Arc the outer corner.
            path.addArcTo(toRectMaxXMinYCorner, toRectMaxXMinYCorner + verticalRadius, radius);
        }
    }
}

static void addShrinkWrapLeftCorner(Path& path, const FloatRect* fromRect, const FloatRect* toRect, float radius)
{
    FloatSize horizontalRadius(radius, 0);
    FloatSize verticalRadius(0, radius);

    if (!fromRect) {
        // For the first (bottom) rect:

        // Across the bottom, towards the left.
        path.addLineTo(toRect->minXMaxYCorner() + horizontalRadius);

        // Arc the bottom corner.
        path.addArcTo(toRect->minXMaxYCorner(), toRect->minXMaxYCorner() - verticalRadius, radius);

    } else if (!toRect) {
        // For the last (top) rect:

        // Up the left.
        path.addLineTo(fromRect->minXMinYCorner() + verticalRadius);

        // Arc the top corner.
        path.addArcTo(fromRect->minXMinYCorner(), fromRect->minXMinYCorner() + horizontalRadius, radius);
    } else {
        // For middle rects:
        float leftEdgeDifference = fromRect->x() - toRect->x();

        // Skip over rects with equal edges, because we can't make
        // sensible curves between them.
        if (fabsf(leftEdgeDifference) < std::numeric_limits<float>::epsilon())
            return;

        if (leftEdgeDifference < 0) {
            float effectiveY = std::min(toRect->maxY(), fromRect->y());
            FloatPoint toRectMinXMaxYCorner = FloatPoint(toRect->x(), effectiveY);

            // Up the right.
            path.addLineTo(FloatPoint(fromRect->x(), effectiveY) + verticalRadius);

            // Arc the inner corner.
            path.addArcTo(FloatPoint(fromRect->x(), effectiveY), FloatPoint(fromRect->x(), effectiveY) + horizontalRadius, radius);

            // Across the bottom, towards the right.
            path.addLineTo(toRectMinXMaxYCorner - horizontalRadius);

            // Arc the outer corner.
            path.addArcTo(toRectMinXMaxYCorner, toRectMinXMaxYCorner - verticalRadius, radius);
        } else {
            float effectiveY = std::max(toRect->maxY(), fromRect->y());
            FloatPoint toRectMinXMaxYCorner = FloatPoint(toRect->x(), effectiveY);

            // Up the right.
            path.addLineTo(FloatPoint(fromRect->x(), effectiveY) + verticalRadius);

            // Arc the outer corner.
            path.addArcTo(FloatPoint(fromRect->x(), effectiveY), FloatPoint(fromRect->x(), effectiveY) - horizontalRadius, radius);

            // Across the bottom, towards the left.
            path.addLineTo(toRectMinXMaxYCorner + horizontalRadius);

            // Arc the inner corner.
            path.addArcTo(toRectMinXMaxYCorner, toRectMinXMaxYCorner - verticalRadius, radius);
        }
    }
}

static void addShrinkWrappedPathForRects(Path& path, Vector<FloatRect>& rects, float radius)
{
    size_t rectCount = rects.size();

    std::sort(rects.begin(), rects.end(), [](FloatRect a, FloatRect b) { return b.y() > a.y(); });

    for (size_t i = 0; i <= rectCount; ++i)
        addShrinkWrapRightCorner(path, i ? &rects[i - 1] : nullptr, i < rectCount ? &rects[i] : nullptr, radius);

    for (size_t i = 0; i <= rectCount; ++i) {
        size_t reverseIndex = rectCount - i;
        addShrinkWrapLeftCorner(path, reverseIndex < rectCount ? &rects[reverseIndex] : nullptr, reverseIndex ? &rects[reverseIndex - 1] : nullptr, radius);
    }

    path.closeSubpath();
}

static bool rectsIntersectOrTouch(const FloatRect& a, const FloatRect& b)
{
    return !a.isEmpty() && !b.isEmpty()
        && a.x() <= b.maxX() && b.x() <= a.maxX()
        && a.y() <= b.maxY() && b.y() <= a.maxY();
}

static Vector<FloatRect>* findSetContainingRect(Vector<Vector<FloatRect>>& sets, FloatRect rect)
{
    for (auto& set : sets) {
        if (set.contains(rect))
            return &set;
    }

    return nullptr;
}

static Vector<Vector<FloatRect>> contiguousRectGroupsFromRects(const Vector<FloatRect>& rects)
{
    Vector<std::pair<FloatRect, FloatRect>> intersections;
    Vector<FloatRect> soloRects = rects;

    for (auto& rectA : rects) {
        for (auto& rectB : rects) {
            if (rectA == rectB)
                continue;

            if (rectsIntersectOrTouch(rectA, rectB)) {
                intersections.append(std::make_pair(rectA, rectB));
                soloRects.removeAllMatching([rectA, rectB](FloatRect q) { return q == rectA || q == rectB; });
            }
        }
    }

    Vector<Vector<FloatRect>> rectSets;

    for (auto& intersectingPair : intersections) {
        if (Vector<FloatRect>* rectContainingFirst = findSetContainingRect(rectSets, intersectingPair.first)) {
            if (!rectContainingFirst->contains(intersectingPair.second))
                rectContainingFirst->append(intersectingPair.second);
            continue;
        }

        if (Vector<FloatRect>* rectContainingSecond = findSetContainingRect(rectSets, intersectingPair.second)) {
            if (!rectContainingSecond->contains(intersectingPair.first))
                rectContainingSecond->append(intersectingPair.first);
            continue;
        }

        // We didn't find a set including either of our rects, so start a new one.
        rectSets.append(Vector<FloatRect>({intersectingPair.first, intersectingPair.second}));

        continue;
    }

    for (auto& rect : soloRects) {
        ASSERT(!findSetContainingRect(rectSets, rect));
        rectSets.append(Vector<FloatRect>({rect}));
    }

    return rectSets;
}

Path PathUtilities::pathWithShrinkWrappedRects(const Vector<FloatRect>& rects, float radius)
{
    Path path;

    if (rects.isEmpty())
        return path;

    Vector<Vector<FloatRect>> rectSets = contiguousRectGroupsFromRects(rects);

    for (auto& set : rectSets)
        addShrinkWrappedPathForRects(path, set, radius);
    
    return path;
}

}
