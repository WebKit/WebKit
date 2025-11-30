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

class BorderData;
class RenderStyleBase;

class BorderValue final {
    friend class BorderData;
    friend class RenderStyleProperties;
public:
    BorderValue()
        : m_style(static_cast<unsigned>(BorderStyle::None))
    {
    }

    const Style::Color& color() const { return m_color; }
    Style::LineWidth width() const { return m_width; }
    BorderStyle style() const { return static_cast<BorderStyle>(m_style); }

    bool isVisible() const;
    bool nonZero() const;
    bool isTransparent() const;

    bool operator==(const BorderValue&) const = default;

private:
    Style::Color m_color { Style::Color::currentColor() };
    Style::LineWidth m_width { Style::LineWidth::Length { 3.0f } };
    PREFERRED_TYPE(BorderStyle) unsigned m_style : 4;
};

inline bool BorderValue::nonZero() const
{
    return width() && style() != BorderStyle::None;
}

TextStream& operator<<(TextStream&, const BorderValue&);

} // namespace WebCore
