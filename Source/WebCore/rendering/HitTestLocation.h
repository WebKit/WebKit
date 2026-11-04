/*
 * Copyright (C) 2006 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
*/

#pragma once

#include <WebCore/FloatQuad.h>
#include <WebCore/LayoutRoundedRect.h>

namespace WebCore {

class HitTestLocation {
public:
    enum class RectBased : bool { No, Yes };

    NODELETE WEBCORE_EXPORT HitTestLocation();
    NODELETE HitTestLocation(const LayoutPoint&);
    HitTestLocation(const FloatPoint&, const FloatQuad&, RectBased = RectBased::Yes);

    NODELETE WEBCORE_EXPORT HitTestLocation(const LayoutRect&);

    // Make a copy the HitTestLocation in a new region by applying given offset to internal point and area.
    HitTestLocation(const HitTestLocation&, const LayoutSize& offset);
    NODELETE WEBCORE_EXPORT HitTestLocation(const HitTestLocation&);
    NODELETE WEBCORE_EXPORT ~HitTestLocation();
    NODELETE HitTestLocation& operator=(const HitTestLocation&);

    const LayoutPoint& point() const LIFETIME_BOUND { return m_point; }
    IntPoint roundedPoint() const { return roundedIntPoint(m_point); }

    // Rect-based hit test related methods.
    bool isRectBasedTest() const { return m_isRectBased; }
    bool isRectilinear() const { return m_isRectilinear; }
    LayoutRect boundingBox() const { return m_boundingBox; }

    NODELETE WEBCORE_EXPORT bool intersects(const LayoutRect&) const;
    NODELETE bool intersects(const FloatRect&) const;
    NODELETE bool intersects(const LayoutRoundedRect&) const;

    const FloatPoint& transformedPoint() const LIFETIME_BOUND { return m_transformedPoint; }
    const FloatQuad& transformedRect() const LIFETIME_BOUND { return m_transformedRect; }

private:
    template<typename RectType> NODELETE bool intersectsRect(const RectType&) const;

    void move(const LayoutSize&);

    // These are the cached forms of the more accurate point and rect below.
    LayoutPoint m_point;
    LayoutRect m_boundingBox;

    FloatPoint m_transformedPoint;
    FloatQuad m_transformedRect;

    bool m_isRectBased { false };
    bool m_isRectilinear { true };
};

} // namespace WebCore
