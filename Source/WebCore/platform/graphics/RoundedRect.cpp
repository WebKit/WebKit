/*
 * Copyright (C) 2003, 2006, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include <algorithm>

using namespace std;

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

void RoundedRect::Radii::expand(LayoutUnit topWidth, LayoutUnit bottomWidth, LayoutUnit leftWidth, LayoutUnit rightWidth)
{
    m_topLeft.setWidth(max<LayoutUnit>(0, m_topLeft.width() + leftWidth));
    m_topLeft.setHeight(max<LayoutUnit>(0, m_topLeft.height() + topWidth));

    m_topRight.setWidth(max<LayoutUnit>(0, m_topRight.width() + rightWidth));
    m_topRight.setHeight(max<LayoutUnit>(0, m_topRight.height() + topWidth));

    m_bottomLeft.setWidth(max<LayoutUnit>(0, m_bottomLeft.width() + leftWidth));
    m_bottomLeft.setHeight(max<LayoutUnit>(0, m_bottomLeft.height() + bottomWidth));

    m_bottomRight.setWidth(max<LayoutUnit>(0, m_bottomRight.width() + rightWidth));
    m_bottomRight.setHeight(max<LayoutUnit>(0, m_bottomRight.height() + bottomWidth));
}

void RoundedRect::inflateWithRadii(LayoutUnit size)
{
    LayoutRect old = m_rect;

    m_rect.inflate(size);
    // Considering the inflation factor of shorter size to scale the radii seems appropriate here
    float factor;
    if (m_rect.width() < m_rect.height())
        factor = old.width() ? (float)m_rect.width() / old.width() : 0;
    else
        factor = old.height() ? (float)m_rect.height() / old.height() : 0;

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
            m_bottomLeft = LayoutSize();
        else
            m_topRight = LayoutSize();
        m_topLeft = LayoutSize();
    }
        
    if (excludeLogicalRightEdge) {
        if (isHorizontal)
            m_topRight = LayoutSize();
        else
            m_bottomLeft = LayoutSize();
        m_bottomRight = LayoutSize();
    }
}

RoundedRect::RoundedRect(LayoutUnit x, LayoutUnit y, LayoutUnit width, LayoutUnit height)
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
    return m_radii.topLeft().width() + m_radii.topRight().width() <= m_rect.width()
        && m_radii.bottomLeft().width() + m_radii.bottomRight().width() <= m_rect.width()
        && m_radii.topLeft().height() + m_radii.topRight().height() <= m_rect.height()
        && m_radii.bottomLeft().height() + m_radii.bottomRight().height() <= m_rect.height();
}

} // namespace WebCore
