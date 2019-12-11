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

#pragma once

#include "Color.h"
#include "RenderStyleConstants.h"

namespace WebCore {

class BorderValue {
friend class RenderStyle;
public:
    BorderValue()
        : m_style(static_cast<unsigned>(BorderStyle::None))
        , m_isAuto(static_cast<unsigned>(OutlineIsAuto::Off))
    {
    }

    bool nonZero(bool checkStyle = true) const
    {
        return width() && (!checkStyle || style() != BorderStyle::None);
    }

    bool isTransparent() const
    {
        return m_color.isValid() && !m_color.isVisible();
    }

    bool isVisible(bool checkStyle = true) const
    {
        return nonZero(checkStyle) && !isTransparent() && (!checkStyle || style() != BorderStyle::Hidden);
    }

    bool operator==(const BorderValue& o) const
    {
        return m_width == o.m_width && m_style == o.m_style && m_color == o.m_color;
    }

    bool operator!=(const BorderValue& o) const
    {
        return !(*this == o);
    }

    void setColor(const Color& color)
    {
        m_color = color;
    }

    const Color& color() const { return m_color; }

    float width() const { return m_width; }
    BorderStyle style() const { return static_cast<BorderStyle>(m_style); }
    
    float boxModelWidth() const;

protected:
    Color m_color;

    float m_width { 3 };

    unsigned m_style : 4; // BorderStyle

    // This is only used by OutlineValue but moved here to keep the bits packed.
    unsigned m_isAuto : 1; // OutlineIsAuto
};

inline float BorderValue::boxModelWidth() const
{
    auto style = this->style();
    if (style == BorderStyle::None || style == BorderStyle::Hidden)
        return 0;

    return width();
}

} // namespace WebCore
