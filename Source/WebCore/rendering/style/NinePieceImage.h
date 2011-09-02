/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef NinePieceImage_h
#define NinePieceImage_h

#include "LengthBox.h"
#include "StyleImage.h"

namespace WebCore {

enum ENinePieceImageRule {
    StretchImageRule, RoundImageRule, SpaceImageRule, RepeatImageRule
};

class NinePieceImage {
public:
    NinePieceImage()
        : m_image(0)
        , m_slices(Length(100, Percent), Length(100, Percent), Length(100, Percent), Length(100, Percent))
        , m_fill(false)
        , m_horizontalRule(StretchImageRule)
        , m_verticalRule(StretchImageRule)
    {
    }

    NinePieceImage(StyleImage* image, LengthBox slices, bool fill, ENinePieceImageRule h, ENinePieceImageRule v) 
      : m_image(image)
      , m_slices(slices)
      , m_fill(fill)
      , m_horizontalRule(h)
      , m_verticalRule(v)
    {
    }

    bool operator==(const NinePieceImage& o) const;
    bool operator!=(const NinePieceImage& o) const { return !(*this == o); }

    bool hasImage() const { return m_image != 0; }
    StyleImage* image() const { return m_image.get(); }
    void setImage(PassRefPtr<StyleImage> image) { m_image = image; }
    
    const LengthBox& slices() const { return m_slices; }
    void setSlices(const LengthBox& l) { m_slices = l; }

    bool fill() const { return m_fill; }
    void setFill(bool fill) { m_fill = fill; }

    ENinePieceImageRule horizontalRule() const { return static_cast<ENinePieceImageRule>(m_horizontalRule); }
    void setHorizontalRule(ENinePieceImageRule rule) { m_horizontalRule = rule; }
    
    ENinePieceImageRule verticalRule() const { return static_cast<ENinePieceImageRule>(m_verticalRule); }
    void setVerticalRule(ENinePieceImageRule rule) { m_verticalRule = rule; }

    void copySlicesFrom(const NinePieceImage& other)
    {
        m_slices = other.m_slices;
        m_fill = other.m_fill;
    }

    void copyRepeatFrom(const NinePieceImage& other)
    {
        m_horizontalRule = other.m_horizontalRule;
        m_verticalRule = other.m_verticalRule;
    }

private:
    RefPtr<StyleImage> m_image;
    LengthBox m_slices;
    bool m_fill : 1;
    unsigned m_horizontalRule : 2; // ENinePieceImageRule
    unsigned m_verticalRule : 2; // ENinePieceImageRule
};

} // namespace WebCore

#endif // NinePieceImage_h
