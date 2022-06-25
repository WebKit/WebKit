/*
 * Copyright (C) 2003, 2006, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013 Xidorn Quan (quanxunzhen@gmail.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RoundedRect.h"

#include "FloatRoundedRect.h"
#include "GeometryUtilities.h"
#include "LayoutRect.h"
#include "LayoutUnit.h"
#include "Region.h"
#include <algorithm>
#include <wtf/MathExtras.h>

namespace WebCore {

bool RoundedRect::Radii::isZero() const
{
    return m_topLeft.isZero() && m_topRight.isZero() && m_bottomLeft.isZero() && m_bottomRight.isZero();
}

void RoundedRect::Radii::scale(float factor)
{
    if (factor == 1)
        return;

    // If either radius on a corner becomes zero, reset both radii on that corner.
    m_topLeft.scale(factor);
    if (!m_topLeft.width() || !m_topLeft.height())
        m_topLeft = LayoutSize();
    m_topRight.scale(factor);
    if (!m_topRight.width() || !m_topRight.height())
        m_topRight = LayoutSize();
    m_bottomLeft.scale(factor);
    if (!m_bottomLeft.width() || !m_bottomLeft.height())
        m_bottomLeft = LayoutSize();
    m_bottomRight.scale(factor);
    if (!m_bottomRight.width() || !m_bottomRight.height())
        m_bottomRight = LayoutSize();
}

void RoundedRect::Radii::expand(const LayoutUnit& topWidth, const LayoutUnit& bottomWidth, const LayoutUnit& leftWidth, const LayoutUnit& rightWidth)
{
    if (m_topLeft.width() > 0 && m_topLeft.height() > 0) {
        m_topLeft.setWidth(std::max<LayoutUnit>(0, m_topLeft.width() + leftWidth));
        m_topLeft.setHeight(std::max<LayoutUnit>(0, m_topLeft.height() + topWidth));
    }
    if (m_topRight.width() > 0 && m_topRight.height() > 0) {
        m_topRight.setWidth(std::max<LayoutUnit>(0, m_topRight.width() + rightWidth));
        m_topRight.setHeight(std::max<LayoutUnit>(0, m_topRight.height() + topWidth));
    }
    if (m_bottomLeft.width() > 0 && m_bottomLeft.height() > 0) {
        m_bottomLeft.setWidth(std::max<LayoutUnit>(0, m_bottomLeft.width() + leftWidth));
        m_bottomLeft.setHeight(std::max<LayoutUnit>(0, m_bottomLeft.height() + bottomWidth));
    }
    if (m_bottomRight.width() > 0 && m_bottomRight.height() > 0) {
        m_bottomRight.setWidth(std::max<LayoutUnit>(0, m_bottomRight.width() + rightWidth));
        m_bottomRight.setHeight(std::max<LayoutUnit>(0, m_bottomRight.height() + bottomWidth));
    }
}

void RoundedRect::inflateWithRadii(const LayoutUnit& size)
{
    LayoutRect old = m_rect;

    m_rect.inflate(size);
    // Considering the inflation factor of shorter size to scale the radii seems appropriate here
    float factor;
    if (m_rect.width() < m_rect.height())
        factor = old.width() ? (float)m_rect.width() / old.width() : int(0);
    else
        factor = old.height() ? (float)m_rect.height() / old.height() : int(0);

    m_radii.scale(factor);
}

void RoundedRect::Radii::includeLogicalEdges(const RoundedRect::Radii& edges, bool isHorizontal, bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    if (includeLogicalLeftEdge) {
        if (isHorizontal)
            m_bottomLeft = edges.bottomLeft();
        else
            m_topRight = edges.topRight();
        m_topLeft = edges.topLeft();
    }

    if (includeLogicalRightEdge) {
        if (isHorizontal)
            m_topRight = edges.topRight();
        else
            m_bottomLeft = edges.bottomLeft();
        m_bottomRight = edges.bottomRight();
    }
}

void RoundedRect::Radii::excludeLogicalEdges(bool isHorizontal, bool excludeLogicalLeftEdge, bool excludeLogicalRightEdge)
{
    if (excludeLogicalLeftEdge) {
        if (isHorizontal)
            m_bottomLeft = IntSize();
        else
            m_topRight = IntSize();
        m_topLeft = IntSize();
    }
        
    if (excludeLogicalRightEdge) {
        if (isHorizontal)
            m_topRight = IntSize();
        else
            m_bottomLeft = IntSize();
        m_bottomRight = IntSize();
    }
}

RoundedRect::RoundedRect(const LayoutUnit& x, const LayoutUnit& y, const LayoutUnit& width, const LayoutUnit& height)
    : m_rect(x, y, width, height)
{
}

RoundedRect::RoundedRect(const LayoutRect& rect, const Radii& radii)
    : m_rect(rect)
    , m_radii(radii)
{
}

RoundedRect::RoundedRect(const LayoutRect& rect, const LayoutSize& topLeft, const LayoutSize& topRight, const LayoutSize& bottomLeft, const LayoutSize& bottomRight)
    : m_rect(rect)
    , m_radii(topLeft, topRight, bottomLeft, bottomRight)
{
}

void RoundedRect::includeLogicalEdges(const Radii& edges, bool isHorizontal, bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    m_radii.includeLogicalEdges(edges, isHorizontal, includeLogicalLeftEdge, includeLogicalRightEdge);
}

void RoundedRect::excludeLogicalEdges(bool isHorizontal, bool excludeLogicalLeftEdge, bool excludeLogicalRightEdge)
{
    m_radii.excludeLogicalEdges(isHorizontal, excludeLogicalLeftEdge, excludeLogicalRightEdge);
}

bool RoundedRect::isRenderable() const
{
    return m_radii.topLeft().width() >= 0 && m_radii.topLeft().height() >= 0
        && m_radii.bottomLeft().width() >= 0 && m_radii.bottomLeft().height() >= 0
        && m_radii.topRight().width() >= 0 && m_radii.topRight().height() >= 0
        && m_radii.bottomRight().width() >= 0 && m_radii.bottomRight().height() >= 0
        && m_radii.topLeft().width() + m_radii.topRight().width() <= m_rect.width()
        && m_radii.bottomLeft().width() + m_radii.bottomRight().width() <= m_rect.width()
        && m_radii.topLeft().height() + m_radii.bottomLeft().height() <= m_rect.height()
        && m_radii.topRight().height() + m_radii.bottomRight().height() <= m_rect.height();
}

void RoundedRect::adjustRadii()
{
    int maxRadiusWidth = std::max(m_radii.topLeft().width() + m_radii.topRight().width(), m_radii.bottomLeft().width() + m_radii.bottomRight().width());
    int maxRadiusHeight = std::max(m_radii.topLeft().height() + m_radii.bottomLeft().height(), m_radii.topRight().height() + m_radii.bottomRight().height());

    if (maxRadiusWidth <= 0 || maxRadiusHeight <= 0) {
        m_radii.scale(0.0f);
        return;
    }
    float widthRatio = static_cast<float>(m_rect.width()) / maxRadiusWidth;
    float heightRatio = static_cast<float>(m_rect.height()) / maxRadiusHeight;
    m_radii.scale(widthRatio < heightRatio ? widthRatio : heightRatio);
}

bool RoundedRect::intersectsQuad(const FloatQuad& quad) const
{
    FloatRect rect(m_rect);
    if (!quad.intersectsRect(rect))
        return false;

    const LayoutSize& topLeft = m_radii.topLeft();
    if (!topLeft.isEmpty()) {
        FloatRect rect(m_rect.x(), m_rect.y(), topLeft.width(), topLeft.height());
        if (quad.intersectsRect(rect)) {
            FloatPoint center(m_rect.x() + topLeft.width(), m_rect.y() + topLeft.height());
            FloatSize size(topLeft.width(), topLeft.height());
            if (!quad.intersectsEllipse(center, size))
                return false;
        }
    }

    const LayoutSize& topRight = m_radii.topRight();
    if (!topRight.isEmpty()) {
        FloatRect rect(m_rect.maxX() - topRight.width(), m_rect.y(), topRight.width(), topRight.height());
        if (quad.intersectsRect(rect)) {
            FloatPoint center(m_rect.maxX() - topRight.width(), m_rect.y() + topRight.height());
            FloatSize size(topRight.width(), topRight.height());
            if (!quad.intersectsEllipse(center, size))
                return false;
        }
    }

    const LayoutSize& bottomLeft = m_radii.bottomLeft();
    if (!bottomLeft.isEmpty()) {
        FloatRect rect(m_rect.x(), m_rect.maxY() - bottomLeft.height(), bottomLeft.width(), bottomLeft.height());
        if (quad.intersectsRect(rect)) {
            FloatPoint center(m_rect.x() + bottomLeft.width(), m_rect.maxY() - bottomLeft.height());
            FloatSize size(bottomLeft.width(), bottomLeft.height());
            if (!quad.intersectsEllipse(center, size))
                return false;
        }
    }

    const LayoutSize& bottomRight = m_radii.bottomRight();
    if (!bottomRight.isEmpty()) {
        FloatRect rect(m_rect.maxX() - bottomRight.width(), m_rect.maxY() - bottomRight.height(), bottomRight.width(), bottomRight.height());
        if (quad.intersectsRect(rect)) {
            FloatPoint center(m_rect.maxX() - bottomRight.width(), m_rect.maxY() - bottomRight.height());
            FloatSize size(bottomRight.width(), bottomRight.height());
            if (!quad.intersectsEllipse(center, size))
                return false;
        }
    }

    return true;
}

bool RoundedRect::contains(const LayoutRect& otherRect) const
{
    if (!rect().contains(otherRect) || !isRenderable())
        return false;

    const LayoutSize& topLeft = m_radii.topLeft();
    if (!topLeft.isEmpty()) {
        FloatPoint center = { m_rect.x() + topLeft.width(), m_rect.y() + topLeft.height() };
        if (otherRect.x() <= center.x() && otherRect.y() <= center.y()) {
            if (!ellipseContainsPoint(center, topLeft, otherRect.minXMinYCorner()))
                return false;
        }
    }

    const LayoutSize& topRight = m_radii.topRight();
    if (!topRight.isEmpty()) {
        FloatPoint center = { m_rect.maxX() - topRight.width(), m_rect.y() + topRight.height() };
        if (otherRect.maxX() >= center.x() && otherRect.y() <= center.y()) {
            if (!ellipseContainsPoint(center, topRight, otherRect.maxXMinYCorner()))
                return false;
        }
    }

    const LayoutSize& bottomLeft = m_radii.bottomLeft();
    if (!bottomLeft.isEmpty()) {
        FloatPoint center = { m_rect.x() + bottomLeft.width(), m_rect.maxY() - bottomLeft.height() };
        if (otherRect.x() <= center.x() && otherRect.maxY() >= center.y()) {
            if (!ellipseContainsPoint(center, bottomLeft, otherRect.minXMaxYCorner()))
                return false;
        }
    }

    const LayoutSize& bottomRight = m_radii.bottomRight();
    if (!bottomRight.isEmpty()) {
        FloatPoint center = { m_rect.maxX() - bottomRight.width(), m_rect.maxY() - bottomRight.height() };
        if (otherRect.maxX() >= center.x() && otherRect.maxY() >= center.y()) {
            if (!ellipseContainsPoint(center, bottomRight, otherRect.maxXMaxYCorner()))
                return false;
        }
    }

    return true;
}

FloatRoundedRect RoundedRect::pixelSnappedRoundedRectForPainting(float deviceScaleFactor) const
{
    LayoutRect originalRect = rect();
    if (originalRect.isEmpty())
        return FloatRoundedRect(originalRect, radii());

    FloatRect pixelSnappedRect = snapRectToDevicePixels(originalRect, deviceScaleFactor);

    if (!isRenderable())
        return FloatRoundedRect(pixelSnappedRect, radii());

    // Snapping usually does not alter size, but when it does, we need to make sure that the final rect is still renderable by distributing the size delta proportionally.
    FloatRoundedRect::Radii adjustedRadii = radii();
    adjustedRadii.scale(pixelSnappedRect.width() / originalRect.width().toFloat(), pixelSnappedRect.height() / originalRect.height().toFloat());
    FloatRoundedRect snappedRoundedRect = FloatRoundedRect(pixelSnappedRect, adjustedRadii);
    if (!snappedRoundedRect.isRenderable()) {
        // Floating point mantissa overflow can produce a non-renderable rounded rect.
        adjustedRadii.shrink(1 / deviceScaleFactor);
        snappedRoundedRect.setRadii(adjustedRadii);
    }
    ASSERT(snappedRoundedRect.isRenderable());
    return snappedRoundedRect;
}

Region approximateAsRegion(const RoundedRect& roundedRect, unsigned stepLength)
{
    Region region;

    if (roundedRect.isEmpty())
        return region;

    auto& rect = roundedRect.rect();
    region.unite(enclosingIntRect(rect));

    if (!roundedRect.isRounded())
        return region;

    auto& radii = roundedRect.radii();

    auto makeIntRect = [] (LayoutPoint a, LayoutPoint b) {
        return enclosingIntRect(LayoutRect {
            LayoutPoint { std::min(a.x(), b.x()), std::min(a.y(), b.y()) },
            LayoutPoint { std::max(a.x(), b.x()), std::max(a.y(), b.y()) }
        });
    };

    auto subtractCornerRects = [&] (LayoutPoint corner, LayoutPoint ellipsisCenter, LayoutSize axes, double fromAngle) {
        double toAngle = fromAngle + piDouble / 2;

        // Substract more rects for longer, more rounded arcs.
        auto arcLengthFactor = roundToInt(std::min(axes.width(), axes.height()));
        auto count = (arcLengthFactor + (stepLength / 2)) / stepLength;

        constexpr auto maximumCount = 20u;
        count = std::min(maximumCount, count);

        for (auto i = 0u; i < count; ++i) {
            auto angle = fromAngle + (i + 1) * (toAngle - fromAngle) / (count + 1);
            auto ellipsisPoint = LayoutPoint { axes.width() * cos(angle), axes.height() * sin(angle) };
            auto cornerRect = makeIntRect(corner, ellipsisCenter + ellipsisPoint);
            region.subtract(cornerRect);
        }
    };

    {
        auto corner = rect.maxXMaxYCorner();
        auto axes = radii.bottomRight();
        auto ellipsisCenter = LayoutPoint(corner.x() - axes.width(), corner.y() - axes.height());
        subtractCornerRects(corner, ellipsisCenter, axes, 0);
    }

    {
        auto corner = rect.minXMaxYCorner();
        auto axes = radii.bottomLeft();
        auto ellipsisCenter = LayoutPoint(corner.x() + axes.width(), corner.y() - axes.height());
        subtractCornerRects(corner, ellipsisCenter, axes, piDouble / 2);
    }

    {
        auto corner = rect.minXMinYCorner();
        auto axes = radii.topLeft();
        auto ellipsisCenter = LayoutPoint(corner.x() + axes.width(), corner.y() + axes.height());
        subtractCornerRects(corner, ellipsisCenter, axes, piDouble);
    }

    {
        auto corner = rect.maxXMinYCorner();
        auto axes = radii.topRight();
        auto ellipsisCenter = LayoutPoint(corner.x() - axes.width(), corner.y() + axes.height());
        subtractCornerRects(corner, ellipsisCenter, axes, piDouble * 3 / 2);
    }

    return region;
}

} // namespace WebCore
