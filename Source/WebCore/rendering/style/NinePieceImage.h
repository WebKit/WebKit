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

#include "LayoutTypes.h"
#include "LengthBox.h"
#include "StyleImage.h"

namespace WebCore {

enum ENinePieceImageRule {
    StretchImageRule, RoundImageRule, SpaceImageRule, RepeatImageRule
};

class NinePieceImageData {
public:
    NinePieceImageData()
        : m_image(0)
        , m_imageSlices(Length(100, Percent), Length(100, Percent), Length(100, Percent), Length(100, Percent))
        , m_borderSlices(Length(1, Relative), Length(1, Relative), Length(1, Relative), Length(1, Relative))
        , m_outset(0)
        , m_fill(false)
        , m_horizontalRule(StretchImageRule)
        , m_verticalRule(StretchImageRule)
    {
    }

    NinePieceImageData(PassRefPtr<StyleImage> image, LengthBox imageSlices, bool fill, LengthBox borderSlices, LengthBox outset, ENinePieceImageRule h, ENinePieceImageRule v)
      : m_image(image)
      , m_imageSlices(imageSlices)
      , m_borderSlices(borderSlices)
      , m_outset(outset)
      , m_fill(fill)
      , m_horizontalRule(h)
      , m_verticalRule(v)
    {
    }

    bool operator==(const NinePieceImageData&) const;
    bool operator!=(const NinePieceImageData& o) const { return !(*this == o); }

    RefPtr<StyleImage> m_image;
    LengthBox m_imageSlices;
    LengthBox m_borderSlices;
    LengthBox m_outset;
    bool m_fill : 1;
    unsigned m_horizontalRule : 2; // ENinePieceImageRule
    unsigned m_verticalRule : 2; // ENinePieceImageRule
};

class NinePieceImage {
public:
    NinePieceImage() { }

    NinePieceImage(PassRefPtr<StyleImage> image, LengthBox imageSlices, bool fill, LengthBox borderSlices, LengthBox outset, ENinePieceImageRule h, ENinePieceImageRule v)
        : m_data(adoptPtr(new NinePieceImageData(image, imageSlices, fill, borderSlices, outset, h, v)))
    { }

    NinePieceImage(const NinePieceImage& other)
    {
        *this = other;
    }

    void operator=(const NinePieceImage& other)
    {
        if (!other.m_data) {
            m_data.clear();
            return;
        }
        const NinePieceImageData& otherData = other.data();
        m_data = adoptPtr(new NinePieceImageData(otherData.m_image, otherData.m_imageSlices, otherData.m_fill, otherData.m_borderSlices, otherData.m_outset, static_cast<ENinePieceImageRule>(otherData.m_horizontalRule), static_cast<ENinePieceImageRule>(otherData.m_verticalRule)));
    }

    bool operator==(const NinePieceImage& other) const { return data() == other.data(); }
    bool operator!=(const NinePieceImage& other) const { return !(*this == other); }

    bool hasImage() const { return m_data && m_data->m_image; }
    StyleImage* image() const { return m_data ? m_data->m_image.get() : 0; }
    void setImage(PassRefPtr<StyleImage> image) { ensureData(); m_data->m_image = image; }
    
    const LengthBox& imageSlices() const { return data().m_imageSlices; }
    void setImageSlices(const LengthBox& slices) { ensureData(); m_data->m_imageSlices = slices; }

    bool fill() const { return data().m_fill; }
    void setFill(bool fill) { ensureData(); m_data->m_fill = fill; }

    const LengthBox& borderSlices() const { return data().m_borderSlices; }
    void setBorderSlices(const LengthBox& slices) { ensureData(); m_data->m_borderSlices = slices; }

    const LengthBox& outset() const { return data().m_outset; }
    void setOutset(const LengthBox& outset) { ensureData(); m_data->m_outset = outset; }
    
    static LayoutUnit computeOutset(Length outsetSide, LayoutUnit borderSide)
    {
        if (outsetSide.isRelative())
            return outsetSide.value() * borderSide;
        return outsetSide.value();
    }

    ENinePieceImageRule horizontalRule() const { return static_cast<ENinePieceImageRule>(data().m_horizontalRule); }
    void setHorizontalRule(ENinePieceImageRule rule) { ensureData(); m_data->m_horizontalRule = rule; }
    
    ENinePieceImageRule verticalRule() const { return static_cast<ENinePieceImageRule>(data().m_verticalRule); }
    void setVerticalRule(ENinePieceImageRule rule) { ensureData(); m_data->m_verticalRule = rule; }

    void copyImageSlicesFrom(const NinePieceImage& other)
    {
        ensureData();
        m_data->m_imageSlices = other.data().m_imageSlices;
        m_data->m_fill = other.data().m_fill;
    }

    void copyBorderSlicesFrom(const NinePieceImage& other)
    {
        ensureData();
        m_data->m_borderSlices = other.data().m_borderSlices;
    }
    
    void copyOutsetFrom(const NinePieceImage& other)
    {
        ensureData();
        m_data->m_outset = other.data().m_outset;
    }

    void copyRepeatFrom(const NinePieceImage& other)
    {
        ensureData();
        m_data->m_horizontalRule = other.data().m_horizontalRule;
        m_data->m_verticalRule = other.data().m_verticalRule;
    }

    void setMaskDefaults()
    {
        ensureData();
        m_data->m_imageSlices = LengthBox(0);
        m_data->m_fill = true;
        m_data->m_borderSlices = LengthBox();
    }

private:
    static const NinePieceImageData& defaultData();

    void ensureData()
    {
        if (!m_data)
            m_data = adoptPtr(new NinePieceImageData);
    }

    const NinePieceImageData& data() const
    {
        if (m_data)
            return *m_data;
        return defaultData();
    }

    OwnPtr<NinePieceImageData> m_data;
};

} // namespace WebCore

#endif // NinePieceImage_h
