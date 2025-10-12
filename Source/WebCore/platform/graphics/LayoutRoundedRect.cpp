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
#include "LayoutRoundedRect.h"

#include "FloatQuad.h"
#include "FloatRoundedRect.h"
#include "GeometryUtilities.h"
#include "LayoutRect.h"
#include "LayoutUnit.h"
#include "Region.h"
#include <algorithm>
#include <wtf/MathExtras.h>

namespace WebCore {

bool LayoutRoundedRect::Radii::isZero() const
{
    return m_topLeft.isZero() && m_topRight.isZero() && m_bottomLeft.isZero() && m_bottomRight.isZero();
}

void LayoutRoundedRect::Radii::scale(float factor)
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

void LayoutRoundedRect::Radii::expand(LayoutUnit topWidth, LayoutUnit bottomWidth, LayoutUnit leftWidth, LayoutUnit rightWidth)
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

void LayoutRoundedRect::inflateWithRadii(LayoutUnit amount)
{
    LayoutRect old = m_rect;

    if (amount < 0) {
        m_rect.inflateX(std::max(-m_rect.width() / 2, amount));
        m_rect.inflateY(std::max(-m_rect.height() / 2, amount));
    } else
        m_rect.inflate(amount);

    // Considering the inflation factor of shorter size to scale the radii seems appropriate here
    float factor;
    if (m_rect.width() < m_rect.height())
        factor = old.width() ? (float)m_rect.width() / old.width() : 0.0f;
    else
        factor = old.height() ? (float)m_rect.height() / old.height() : 0.0f;

    m_radii.scale(factor);
}

void LayoutRoundedRect::Radii::setRadiiForEdges(const LayoutRoundedRect::Radii& radii, RectEdges<bool> includeEdges)
{
    if (includeEdges.top()) {
        if (includeEdges.left())
            m_topLeft = radii.topLeft();
        if (includeEdges.right())
            m_topRight = radii.topRight();
    }
    if (includeEdges.bottom()) {
        if (includeEdges.left())
            m_bottomLeft = radii.bottomLeft();
        if (includeEdges.right())
            m_bottomRight = radii.bottomRight();
    }
}

bool LayoutRoundedRect::Radii::areRenderableInRect(const LayoutRect& rect) const
{
    return topLeft().width() >= 0 && topLeft().height() >= 0
        && bottomLeft().width() >= 0 && bottomLeft().height() >= 0
        && topRight().width() >= 0 && topRight().height() >= 0
        && bottomRight().width() >= 0 && bottomRight().height() >= 0
        && topLeft().width() + topRight().width() <= rect.width()
        && bottomLeft().width() + bottomRight().width() <= rect.width()
        && topLeft().height() + bottomLeft().height() <= rect.height()
        && topRight().height() + bottomRight().height() <= rect.height();
}

void LayoutRoundedRect::Radii::makeRenderableInRect(const LayoutRect& rect)
{
    auto maxRadiusWidth = std::max(topLeft().width() + topRight().width(), bottomLeft().width() + bottomRight().width());
    auto maxRadiusHeight = std::max(topLeft().height() + bottomLeft().height(), topRight().height() + bottomRight().height());

    if (maxRadiusWidth <= 0 || maxRadiusHeight <= 0) {
        scale(0);
        return;
    }

    float widthRatio = static_cast<float>(rect.width()) / maxRadiusWidth;
    float heightRatio = static_cast<float>(rect.height()) / maxRadiusHeight;
    scale(widthRatio < heightRatio ? widthRatio : heightRatio);

    if (!areRenderableInRect(rect)) {
        maxRadiusWidth = std::max(topLeft().width() + topRight().width(), bottomLeft().width() + bottomRight().width());
        maxRadiusHeight = std::max(topLeft().height() + bottomLeft().height(), topRight().height() + bottomRight().height());

        widthRatio = static_cast<float>(rect.width()) / maxRadiusWidth;
        heightRatio = static_cast<float>(rect.height()) / maxRadiusHeight;
        scale(widthRatio < heightRatio ? widthRatio : heightRatio);
    }
}

LayoutRoundedRect::LayoutRoundedRect(LayoutUnit x, LayoutUnit y, LayoutUnit width, LayoutUnit height)
    : m_rect(x, y, width, height)
{
}

LayoutRoundedRect::LayoutRoundedRect(const LayoutRect& rect, const Radii& radii)
    : m_rect(rect)
    , m_radii(radii)
{
}

LayoutRoundedRect::LayoutRoundedRect(const LayoutRect& rect, const LayoutSize& topLeft, const LayoutSize& topRight, const LayoutSize& bottomLeft, const LayoutSize& bottomRight)
    : m_rect(rect)
    , m_radii(topLeft, topRight, bottomLeft, bottomRight)
{
}

bool LayoutRoundedRect::isRenderable() const
{
    return m_radii.areRenderableInRect(m_rect);
}

void LayoutRoundedRect::adjustRadii()
{
    m_radii.makeRenderableInRect(m_rect);
}

bool LayoutRoundedRect::intersectsQuad(const FloatQuad& quad) const
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

bool LayoutRoundedRect::contains(const LayoutRect& otherRect) const
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

FloatRoundedRect LayoutRoundedRect::pixelSnappedRoundedRectForPainting(float deviceScaleFactor) const
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

TextStream& operator<<(TextStream& ts, const LayoutRoundedRect& roundedRect)
{
    ts << roundedRect.rect();
    ts.dumpProperty("top-left"_s, roundedRect.radii().topLeft());
    ts.dumpProperty("top-right"_s, roundedRect.radii().topRight());
    ts.dumpProperty("bottom-left"_s, roundedRect.radii().bottomLeft());
    ts.dumpProperty("bottom-right"_s, roundedRect.radii().bottomRight());
    return ts;
}

} // namespace WebCore
