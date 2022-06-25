/*
 * Copyright (c) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LayoutRect.h"

#include <algorithm>
#include <wtf/text/TextStream.h>

namespace WebCore {

LayoutRect::LayoutRect(const FloatRect& r)
    : m_location(LayoutPoint(r.location()))
    , m_size(LayoutSize(r.size()))
{
}

bool LayoutRect::intersects(const LayoutRect& other) const
{
    // Checking emptiness handles negative widths as well as zero.
    return !isEmpty() && !other.isEmpty()
        && x() < other.maxX() && other.x() < maxX()
        && y() < other.maxY() && other.y() < maxY();
}

bool LayoutRect::contains(const LayoutRect& other) const
{
    return x() <= other.x() && maxX() >= other.maxX()
        && y() <= other.y() && maxY() >= other.maxY();
}

void LayoutRect::intersect(const LayoutRect& other)
{
    LayoutPoint newLocation(std::max(x(), other.x()), std::max(y(), other.y()));
    LayoutPoint newMaxPoint(std::min(maxX(), other.maxX()), std::min(maxY(), other.maxY()));

    // Return a clean empty rectangle for non-intersecting cases.
    if (newLocation.x() >= newMaxPoint.x() || newLocation.y() >= newMaxPoint.y()) {
        newLocation = LayoutPoint(0, 0);
        newMaxPoint = LayoutPoint(0, 0);
    }

    m_location = newLocation;
    m_size = newMaxPoint - newLocation;
}

bool LayoutRect::edgeInclusiveIntersect(const LayoutRect& other)
{
    LayoutPoint newLocation(std::max(x(), other.x()), std::max(y(), other.y()));
    LayoutPoint newMaxPoint(std::min(maxX(), other.maxX()), std::min(maxY(), other.maxY()));

    bool intersects = true;

    // Return a clean empty rectangle for non-intersecting cases.
    if (newLocation.x() > newMaxPoint.x() || newLocation.y() > newMaxPoint.y()) {
        newLocation = { };
        newMaxPoint = { };
        intersects = false;
    }

    m_location = newLocation;
    m_size = newMaxPoint - newLocation;
    return intersects;
}

void LayoutRect::unite(const LayoutRect& other)
{
    // Handle empty special cases first.
    if (other.isEmpty())
        return;
    if (isEmpty()) {
        *this = other;
        return;
    }

    uniteEvenIfEmpty(other);
}

void LayoutRect::uniteEvenIfEmpty(const LayoutRect& other)
{
    auto minX = std::min(x(), other.x());
    auto minY = std::min(y(), other.y());
    auto maxX = std::max(this->maxX(), other.maxX());
    auto maxY = std::max(this->maxY(), other.maxY());

    setLocationAndSizeFromEdges(minX, minY, maxX, maxY);
}

bool LayoutRect::checkedUnite(const LayoutRect& other)
{
    if (other.isEmpty())
        return true;
    if (isEmpty()) {
        *this = other;
        return true;
    }
    if (!isMaxXMaxYRepresentable() || !other.isMaxXMaxYRepresentable())
        return false;
    FloatPoint topLeft = FloatPoint(std::min<float>(x(), other.x()), std::min<float>(y(), other.y()));
    FloatPoint bottomRight = FloatPoint(std::max<float>(maxX(), other.maxX()), std::max<float>(maxY(), other.maxY()));
    FloatSize size = bottomRight - topLeft;
    
    if (size.width() >= LayoutUnit::nearlyMax() || size.height() >= LayoutUnit::nearlyMax())
        return false;
    m_location = LayoutPoint(topLeft);
    m_size = LayoutSize(size);
    return true;
}

void LayoutRect::uniteIfNonZero(const LayoutRect& other)
{
    // Handle empty special cases first.
    if (!other.width() && !other.height())
        return;
    if (!width() && !height()) {
        *this = other;
        return;
    }

    LayoutPoint newLocation(std::min(x(), other.x()), std::min(y(), other.y()));
    LayoutPoint newMaxPoint(std::max(maxX(), other.maxX()), std::max(maxY(), other.maxY()));

    m_location = newLocation;
    m_size = newMaxPoint - newLocation;
}

void LayoutRect::scale(float scaleValue)
{
    scale(scaleValue, scaleValue);
}

void LayoutRect::scale(float xScale, float yScale)
{
    if (isInfinite())
        return;
    m_location.scale(xScale, yScale);
    m_size.scale(xScale, yScale);
}

LayoutRect unionRect(const Vector<LayoutRect>& rects)
{
    LayoutRect result;

    size_t count = rects.size();
    for (size_t i = 0; i < count; ++i)
        result.unite(rects[i]);

    return result;
}

IntRect enclosingIntRect(const LayoutRect& rect)
{
    // Empty rects with fractional x, y values turn into non-empty rects when converting to enclosing.
    // We need to ensure that empty rects stay empty after the conversion, because the selection code expects them to be empty.
    IntPoint location = flooredIntPoint(rect.minXMinYCorner());
    IntPoint maxPoint = IntPoint(rect.width() ? rect.maxX().ceil() : location.x(), rect.height() ? rect.maxY().ceil() : location.y());
    return IntRect(location, maxPoint - location);
}

LayoutRect enclosingLayoutRect(const FloatRect& rect)
{
    LayoutPoint location = flooredLayoutPoint(rect.minXMinYCorner());
    LayoutPoint maxPoint = ceiledLayoutPoint(rect.maxXMaxYCorner());

    return LayoutRect(location, maxPoint - location);
}

FloatRect encloseRectToDevicePixels(const LayoutRect& rect, float pixelSnappingFactor)
{
    FloatPoint location = floorPointToDevicePixels(rect.minXMinYCorner(), pixelSnappingFactor);
    FloatPoint maxPoint = ceilPointToDevicePixels(rect.maxXMaxYCorner(), pixelSnappingFactor);

    return FloatRect(location, maxPoint - location);
}

TextStream& operator<<(TextStream& ts, const LayoutRect& r)
{
    if (ts.hasFormattingFlag(TextStream::Formatting::LayoutUnitsAsIntegers))
        return ts << snappedIntRect(r);
    
    return ts << FloatRect(r);
}

} // namespace WebCore
