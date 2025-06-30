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

#include "RenderStyleConstants.h"
#include "StyleColor.h"
#include "StyleLineWidth.h"

namespace WebCore {

using namespace CSS::Literals;

class RenderStyle;

class OutlineValue final {
    friend class RenderStyle;
public:
    OutlineValue()
        : m_style(static_cast<unsigned>(OutlineStyle::None))
    {
    }

    bool isVisible() const;

    Style::Length<> size() const { return { m_width.value.value + m_offset.value }; }

    const Style::Color& color() const { return m_color; }
    Style::LineWidth width() const { return m_width; }
    Style::Length<> offset() const { return m_offset; }
    OutlineStyle style() const { return static_cast<OutlineStyle>(m_style); }

    bool operator==(const OutlineValue&) const = default;

private:
    bool nonZero() const;
    bool isTransparent() const;

    Style::Color m_color { Style::Color::currentColor() };
    Style::LineWidth m_width { CSS::Keyword::Medium { } };
    Style::Length<> m_offset { 0_css_px };
    PREFERRED_TYPE(OutlineStyle) unsigned m_style : 4;
};

inline bool OutlineValue::nonZero() const
{
    return !Style::isZero(width()) && style() != OutlineStyle::None;
}

inline std::optional<BorderStyle> toBorderStyle(OutlineStyle outlineStyle)
{
    switch (outlineStyle) {
    case OutlineStyle::Auto:
        break;
    case OutlineStyle::None:
        return BorderStyle::None;
    case OutlineStyle::Inset:
        return BorderStyle::Inset;
    case OutlineStyle::Groove:
        return BorderStyle::Groove;
    case OutlineStyle::Outset:
        return BorderStyle::Outset;
    case OutlineStyle::Ridge:
        return BorderStyle::Ridge;
    case OutlineStyle::Dotted:
        return BorderStyle::Dotted;
    case OutlineStyle::Dashed:
        return BorderStyle::Dashed;
    case OutlineStyle::Solid:
        return BorderStyle::Solid;
    case OutlineStyle::Double:
        return BorderStyle::Double;
    }
    return { };
}

TextStream& operator<<(TextStream&, const OutlineValue&);

} // namespace WebCore
