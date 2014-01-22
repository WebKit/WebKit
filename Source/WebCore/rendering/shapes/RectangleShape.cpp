/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RectangleShape.h"

#include <wtf/MathExtras.h>

namespace WebCore {

static inline float ellipseXIntercept(float y, float rx, float ry)
{
    ASSERT(ry > 0);
    return rx * sqrt(1 - (y * y) / (ry * ry));
}

static inline float ellipseYIntercept(float x, float rx, float ry)
{
    ASSERT(rx > 0);
    return ry * sqrt(1 - (x * x) / (rx * rx));
}

FloatRect RectangleShape::shapePaddingBounds() const
{
    ASSERT(shapePadding() >= 0);
    if (!shapePadding() || isEmpty())
        return m_bounds;

    float boundsX = x() + std::min(width() / 2, shapePadding());
    float boundsY = y() + std::min(height() / 2, shapePadding());
    float boundsWidth = std::max(0.0f, width() - shapePadding() * 2);
    float boundsHeight = std::max(0.0f, height() - shapePadding() * 2);

    return FloatRect(boundsX, boundsY, boundsWidth, boundsHeight);
}

FloatRect RectangleShape::shapeMarginBounds() const
{
    ASSERT(shapeMargin() >= 0);
    if (!shapeMargin())
        return m_bounds;

    float boundsX = x() - shapeMargin();
    float boundsY = y() - shapeMargin();
    float boundsWidth = width() + shapeMargin() * 2;
    float boundsHeight = height() + shapeMargin() * 2;
    return FloatRect(boundsX, boundsY, boundsWidth, boundsHeight);
}

void RectangleShape::getExcludedIntervals(LayoutUnit logicalTop, LayoutUnit logicalHeight, SegmentList& result) const
{
    const FloatRect& bounds = shapeMarginBounds();
    if (bounds.isEmpty())
        return;

    float y1 = logicalTop;
    float y2 = logicalTop + logicalHeight;

    if (y2 < bounds.y() || y1 >= bounds.maxY())
        return;

    float x1 = bounds.x();
    float x2 = bounds.maxX();

    float marginRadiusX = rx() + shapeMargin();
    float marginRadiusY = ry() + shapeMargin();

    if (marginRadiusY > 0) {
        if (y2 < bounds.y() + marginRadiusY) {
            float yi = y2 - bounds.y() - marginRadiusY;
            float xi = ellipseXIntercept(yi, marginRadiusX, marginRadiusY);
            x1 = bounds.x() + marginRadiusX - xi;
            x2 = bounds.maxX() - marginRadiusX + xi;
        } else if (y1 > bounds.maxY() - marginRadiusY) {
            float yi =  y1 - (bounds.maxY() - marginRadiusY);
            float xi = ellipseXIntercept(yi, marginRadiusX, marginRadiusY);
            x1 = bounds.x() + marginRadiusX - xi;
            x2 = bounds.maxX() - marginRadiusX + xi;
        }
    }

    result.append(LineSegment(x1, x2));
}

void RectangleShape::getIncludedIntervals(LayoutUnit logicalTop, LayoutUnit logicalHeight, SegmentList& result) const
{
    const FloatRect& bounds = shapePaddingBounds();
    if (bounds.isEmpty())
        return;

    float y1 = logicalTop;
    float y2 = logicalTop + logicalHeight;

    if (y1 < bounds.y() || y2 > bounds.maxY())
        return;

    float x1 = bounds.x();
    float x2 = bounds.maxX();

    float paddingRadiusX = std::max(0.0f, rx() - shapePadding());
    float paddingRadiusY = std::max(0.0f, ry() - shapePadding());

    if (paddingRadiusX > 0) {
        bool y1InterceptsCorner = y1 < bounds.y() + paddingRadiusY;
        bool y2InterceptsCorner = y2 > bounds.maxY() - paddingRadiusY;
        float xi = 0;

        if (y1InterceptsCorner && y2InterceptsCorner) {
            if  (y1 < bounds.height() + 2 * bounds.y() - y2) {
                float yi = y1 - bounds.y() - paddingRadiusY;
                xi = ellipseXIntercept(yi, paddingRadiusX, paddingRadiusY);
            } else {
                float yi =  y2 - (bounds.maxY() - paddingRadiusY);
                xi = ellipseXIntercept(yi, paddingRadiusX, paddingRadiusY);
            }
        } else if (y1InterceptsCorner) {
            float yi = y1 - bounds.y() - paddingRadiusY;
            xi = ellipseXIntercept(yi, paddingRadiusX, paddingRadiusY);
        } else if (y2InterceptsCorner) {
            float yi =  y2 - (bounds.maxY() - paddingRadiusY);
            xi = ellipseXIntercept(yi, paddingRadiusX, paddingRadiusY);
        }

        if (y1InterceptsCorner || y2InterceptsCorner) {
            x1 = bounds.x() + paddingRadiusX - xi;
            x2 = bounds.maxX() - paddingRadiusX + xi;
        }
    }

    result.append(LineSegment(x1, x2));
}

static FloatPoint cornerInterceptForWidth(float width, float widthAtIntercept, float rx, float ry)
{
    float xi = (width - widthAtIntercept) / 2;
    float yi = ry - ellipseYIntercept(rx - xi, rx, ry);
    return FloatPoint(xi, yi);
}

bool RectangleShape::firstIncludedIntervalLogicalTop(LayoutUnit minLogicalIntervalTop, const FloatSize& minLogicalIntervalSize, LayoutUnit& result) const
{
    float minIntervalTop = minLogicalIntervalTop;
    float minIntervalHeight = minLogicalIntervalSize.height();
    float minIntervalWidth = minLogicalIntervalSize.width();

    const FloatRect& bounds = shapePaddingBounds();
    if (bounds.isEmpty() || minIntervalWidth > bounds.width())
        return false;

    float minY = std::max(bounds.y(), minIntervalTop);
    float maxY = minY + minIntervalHeight;

    if (maxY > bounds.maxY())
        return false;

    float paddingRadiusX = std::max(0.0f, rx() - shapePadding());
    float paddingRadiusY = std::max(0.0f, ry() - shapePadding());

    bool intervalOverlapsMinCorner = minY < bounds.y() + paddingRadiusY;
    bool intervalOverlapsMaxCorner = maxY > bounds.maxY() - paddingRadiusY;

    if (!intervalOverlapsMinCorner && !intervalOverlapsMaxCorner) {
        result = ceiledLayoutUnit(minY);
        return true;
    }

    float centerY = bounds.y() + bounds.height() / 2;
    bool minCornerDefinesX = fabs(centerY - minY) > fabs(centerY - maxY);
    bool intervalFitsWithinCorners = minIntervalWidth + 2 * paddingRadiusX <= bounds.width();
    FloatPoint cornerIntercept = cornerInterceptForWidth(bounds.width(), minIntervalWidth, paddingRadiusX, paddingRadiusY);

    if (intervalOverlapsMinCorner && (!intervalOverlapsMaxCorner || minCornerDefinesX)) {
        if (intervalFitsWithinCorners || bounds.y() + cornerIntercept.y() < minY) {
            result = ceiledLayoutUnit(minY);
            return true;
        }
        if (minIntervalHeight < bounds.height() - (2 * cornerIntercept.y())) {
            result = ceiledLayoutUnit(bounds.y() + cornerIntercept.y());
            return true;
        }
    }

    if (intervalOverlapsMaxCorner && (!intervalOverlapsMinCorner || !minCornerDefinesX)) {
        if (intervalFitsWithinCorners || minY <=  bounds.maxY() - cornerIntercept.y() - minIntervalHeight) {
            result = ceiledLayoutUnit(minY);
            return true;
        }
    }

    return false;
}

void RectangleShape::buildDisplayPaths(DisplayPaths& paths) const
{
    paths.shape.addRoundedRect(m_bounds, FloatSize(m_radii.width(), m_radii.height()), Path::PreferBezierRoundedRect);
    if (shapeMargin())
        paths.marginShape.addRoundedRect(shapeMarginBounds(), FloatSize(m_radii.width() + shapeMargin(), m_radii.height() + shapeMargin()), Path::PreferBezierRoundedRect);
}

} // namespace WebCore
