/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#include "Color.h"
#include "FloatRect.h"
#include "LayoutRect.h"

namespace WebCore {

enum class ShadowStyle : uint8_t { Normal, Inset };

// This class holds information about shadows for the text-shadow and box-shadow properties.

class ShadowData {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ShadowData() = default;

    ShadowData(const LayoutPoint& location, int radius, LayoutUnit spread, ShadowStyle style, bool isWebkitBoxShadow, const Color& color)
        : m_location(location)
        , m_spread(spread)
        , m_radius(radius)
        , m_color(color)
        , m_style(style)
        , m_isWebkitBoxShadow(isWebkitBoxShadow)
    {
    }

    ShadowData(const ShadowData&);
    static Optional<ShadowData> clone(const ShadowData*);

    ShadowData& operator=(ShadowData&&) = default;

    bool operator==(const ShadowData& o) const;
    bool operator!=(const ShadowData& o) const
    {
        return !(*this == o);
    }
    
    LayoutUnit x() const { return m_location.x(); }
    LayoutUnit y() const { return m_location.y(); }
    LayoutPoint location() const { return m_location; }
    int radius() const { return m_radius; }
    LayoutUnit paintingExtent() const
    {
        // Blurring uses a Gaussian function whose std. deviation is m_radius/2, and which in theory
        // extends to infinity. In 8-bit contexts, however, rounding causes the effect to become
        // undetectable at around 1.4x the radius.
        const float radiusExtentMultiplier = 1.4;
        return LayoutUnit(ceilf(m_radius * radiusExtentMultiplier));
    }
    LayoutUnit spread() const { return m_spread; }
    ShadowStyle style() const { return m_style; }
    const Color& color() const { return m_color; }
    bool isWebkitBoxShadow() const { return m_isWebkitBoxShadow; }

    const ShadowData* next() const { return m_next.get(); }
    void setNext(std::unique_ptr<ShadowData> shadow) { m_next = WTFMove(shadow); }

    void adjustRectForShadow(LayoutRect&, int additionalOutlineSize = 0) const;
    void adjustRectForShadow(FloatRect&, int additionalOutlineSize = 0) const;

private:
    LayoutPoint m_location;
    LayoutUnit m_spread;
    int m_radius { 0 }; // This is the "blur radius", or twice the standard deviation of the Gaussian blur.
    Color m_color;
    ShadowStyle m_style { ShadowStyle::Normal };
    bool m_isWebkitBoxShadow { false };
    std::unique_ptr<ShadowData> m_next;
};

WTF::TextStream& operator<<(WTF::TextStream&, const ShadowData&);

} // namespace WebCore
