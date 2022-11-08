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

#include "BorderValue.h"
#include "LengthSize.h"
#include "NinePieceImage.h"

namespace WebCore {

class OutlineValue;

class BorderData {
friend class RenderStyle;
public:
    struct Radii {
        LengthSize topLeft { { 0, LengthType::Fixed }, { 0, LengthType::Fixed } };
        LengthSize topRight { { 0, LengthType::Fixed }, { 0, LengthType::Fixed } };
        LengthSize bottomLeft { { 0, LengthType::Fixed }, { 0, LengthType::Fixed } };
        LengthSize bottomRight { { 0, LengthType::Fixed }, { 0, LengthType::Fixed } };
    };

    bool hasBorder() const
    {
        return m_left.nonZero() || m_right.nonZero() || m_top.nonZero() || m_bottom.nonZero();
    }

    bool hasVisibleBorder() const
    {
        return m_left.isVisible() || m_right.isVisible() || m_top.isVisible() || m_bottom.isVisible();
    }

    bool hasBorderImage() const
    {
        return m_image.hasImage();
    }

    bool hasBorderRadius() const
    {
        return !m_radii.topLeft.isEmpty()
            || !m_radii.topRight.isEmpty()
            || !m_radii.bottomLeft.isEmpty()
            || !m_radii.bottomRight.isEmpty();
    }

    float borderLeftWidth() const
    {
        if (m_left.style() == BorderStyle::None || m_left.style() == BorderStyle::Hidden)
            return 0;
        if (m_image.overridesBorderWidths() && m_image.borderSlices().left().isFixed())
            return m_image.borderSlices().left().value();
        return m_left.width();
    }

    float borderRightWidth() const
    {
        if (m_right.style() == BorderStyle::None || m_right.style() == BorderStyle::Hidden)
            return 0;
        if (m_image.overridesBorderWidths() && m_image.borderSlices().right().isFixed())
            return m_image.borderSlices().right().value();
        return m_right.width();
    }

    float borderTopWidth() const
    {
        if (m_top.style() == BorderStyle::None || m_top.style() == BorderStyle::Hidden)
            return 0;
        if (m_image.overridesBorderWidths() && m_image.borderSlices().top().isFixed())
            return m_image.borderSlices().top().value();
        return m_top.width();
    }

    float borderBottomWidth() const
    {
        if (m_bottom.style() == BorderStyle::None || m_bottom.style() == BorderStyle::Hidden)
            return 0;
        if (m_image.overridesBorderWidths() && m_image.borderSlices().bottom().isFixed())
            return m_image.borderSlices().bottom().value();
        return m_bottom.width();
    }

    FloatBoxExtent borderWidth() const
    {
        return FloatBoxExtent(borderTopWidth(), borderRightWidth(), borderBottomWidth(), borderLeftWidth());
    }

    bool isEquivalentForPainting(const BorderData& other, bool currentColorDiffers) const;

    bool operator==(const BorderData& o) const
    {
        return m_left == o.m_left && m_right == o.m_right && m_top == o.m_top && m_bottom == o.m_bottom && m_image == o.m_image
            && m_radii.topLeft == o.m_radii.topLeft && m_radii.topRight == o.m_radii.topRight && m_radii.bottomLeft == o.m_radii.bottomLeft && m_radii.bottomRight == o.m_radii.bottomRight;
    }

    bool operator!=(const BorderData& o) const
    {
        return !(*this == o);
    }

    const BorderValue& left() const { return m_left; }
    const BorderValue& right() const { return m_right; }
    const BorderValue& top() const { return m_top; }
    const BorderValue& bottom() const { return m_bottom; }

    const NinePieceImage& image() const { return m_image; }

    const LengthSize& topLeftRadius() const { return m_radii.topLeft; }
    const LengthSize& topRightRadius() const { return m_radii.topRight; }
    const LengthSize& bottomLeftRadius() const { return m_radii.bottomLeft; }
    const LengthSize& bottomRightRadius() const { return m_radii.bottomRight; }

    void dump(TextStream&, DumpStyleValues = DumpStyleValues::All) const;

private:
    BorderValue m_left;
    BorderValue m_right;
    BorderValue m_top;
    BorderValue m_bottom;

    NinePieceImage m_image;

    Radii m_radii;
};

WTF::TextStream& operator<<(WTF::TextStream&, const BorderValue&);
WTF::TextStream& operator<<(WTF::TextStream&, const OutlineValue&);
WTF::TextStream& operator<<(WTF::TextStream&, const BorderData&);

} // namespace WebCore
