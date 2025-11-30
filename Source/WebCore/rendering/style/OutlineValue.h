/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/RenderStyleConstants.h>
#include <WebCore/StyleColor.h>
#include <WebCore/StyleLineWidth.h>

namespace WebCore {

class RenderStyleBase;

class OutlineValue final {
    friend class RenderStyleProperties;
public:
    OutlineValue()
        : m_style(static_cast<unsigned>(OutlineStyle::None))
    {
    }

    const Style::Color& color() const { return m_color; }
    Style::LineWidth width() const { return m_width; }
    Style::Length<> offset() const { return m_offset; }
    OutlineStyle style() const { return static_cast<OutlineStyle>(m_style); }

    bool isVisible() const;
    bool nonZero() const;
    bool isTransparent() const;

    bool operator==(const OutlineValue&) const = default;

private:
    Style::Color m_color { Style::Color::currentColor() };
    Style::LineWidth m_width { Style::LineWidth::Length { 3.0f } };
    Style::Length<> m_offset { 0 };
    PREFERRED_TYPE(OutlineStyle) unsigned m_style : 4;
};

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
