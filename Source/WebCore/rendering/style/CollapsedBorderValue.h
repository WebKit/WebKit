/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef CollapsedBorderValue_h
#define CollapsedBorderValue_h

#include "BorderValue.h"
#include "LayoutUnit.h"

namespace WebCore {

class CollapsedBorderValue {
public:
    CollapsedBorderValue()
        : m_colorIsValid(false)
        , m_style(BNONE)
        , m_precedence(BOFF)
        , m_transparent(false)
    {
    }

    CollapsedBorderValue(const BorderValue& border, const Color& color, EBorderPrecedence precedence)
        : m_width(LayoutUnit(border.nonZero() ? border.width() : 0))
        , m_color(color.rgb())
        , m_colorIsValid(color.isValid())
        , m_style(border.style())
        , m_precedence(precedence)
        , m_transparent(border.isTransparent())
    {
    }

    LayoutUnit width() const { return m_style > BHIDDEN ? m_width : LayoutUnit::fromPixel(0); }
    EBorderStyle style() const { return static_cast<EBorderStyle>(m_style); }
    bool exists() const { return m_precedence != BOFF; }
    Color color() const { return Color(m_color, m_colorIsValid); }
    bool isTransparent() const { return m_transparent; }
    EBorderPrecedence precedence() const { return static_cast<EBorderPrecedence>(m_precedence); }

    bool isSameIgnoringColor(const CollapsedBorderValue& o) const
    {
        return width() == o.width() && style() == o.style() && precedence() == o.precedence();
    }

    static LayoutUnit adjustedCollapsedBorderWidth(float borderWidth, float deviceScaleFactor, bool roundUp);

private:
    LayoutUnit m_width;
    RGBA32 m_color { 0 };
    unsigned m_colorIsValid : 1;
    unsigned m_style : 4; // EBorderStyle
    unsigned m_precedence : 3; // EBorderPrecedence
    unsigned m_transparent : 1;
};

inline LayoutUnit CollapsedBorderValue::adjustedCollapsedBorderWidth(float borderWidth, float deviceScaleFactor, bool roundUp)
{
    float halfCollapsedBorderWidth = (borderWidth + (roundUp ? (1 / deviceScaleFactor) : 0)) / 2;
    return floorToDevicePixel(halfCollapsedBorderWidth, deviceScaleFactor);
}

} // namespace WebCore

#endif // CollapsedBorderValue_h
