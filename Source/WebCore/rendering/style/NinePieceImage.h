/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2017 Apple Inc. All rights reserved.
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

#include "DataRef.h"
#include "LengthBox.h"
#include "StyleImage.h"
#include <wtf/Vector.h>

namespace WebCore {

class LayoutSize;
class LayoutRect;
class RenderStyle;

enum ENinePieceImageRule { StretchImageRule, RoundImageRule, SpaceImageRule, RepeatImageRule };

enum ImagePiece {
    MinPiece = 0,
    TopLeftPiece = MinPiece,
    LeftPiece,
    BottomLeftPiece,
    TopRightPiece,
    RightPiece,
    BottomRightPiece,
    TopPiece,
    BottomPiece,
    MiddlePiece,
    MaxPiece
};

inline ImagePiece& operator++(ImagePiece& piece)
{
    piece = static_cast<ImagePiece>(static_cast<int>(piece) + 1);
    return piece;
}

inline bool isCornerPiece(ImagePiece piece)
{
    return piece == TopLeftPiece || piece == TopRightPiece || piece == BottomLeftPiece || piece == BottomRightPiece;
}

inline bool isMiddlePiece(ImagePiece piece)
{
    return piece == MiddlePiece;
}

inline bool isHorizontalPiece(ImagePiece piece)
{
    return piece == TopPiece || piece == BottomPiece || piece == MiddlePiece;
}

inline bool isVerticalPiece(ImagePiece piece)
{
    return piece == LeftPiece || piece == RightPiece || piece == MiddlePiece;
}

inline Optional<PhysicalBoxSide> imagePieceHorizontalSide(ImagePiece piece)
{
    if (piece == TopLeftPiece || piece == TopPiece || piece == TopRightPiece)
        return PhysicalBoxSide::Top;

    if (piece == BottomLeftPiece || piece == BottomPiece || piece == BottomRightPiece)
        return PhysicalBoxSide::Bottom;

    return WTF::nullopt;
}

inline Optional<PhysicalBoxSide> imagePieceVerticalSide(ImagePiece piece)
{
    if (piece == TopLeftPiece || piece == LeftPiece || piece == BottomLeftPiece)
        return PhysicalBoxSide::Left;

    if (piece == TopRightPiece || piece == RightPiece || piece == BottomRightPiece)
        return PhysicalBoxSide::Right;

    return WTF::nullopt;
}

class NinePieceImage {
public:
    NinePieceImage();
    NinePieceImage(RefPtr<StyleImage>&&, LengthBox imageSlices, bool fill, LengthBox borderSlices, LengthBox outset, ENinePieceImageRule horizontalRule, ENinePieceImageRule verticalRule);

    bool operator==(const NinePieceImage& other) const { return m_data == other.m_data; }
    bool operator!=(const NinePieceImage& other) const { return m_data != other.m_data; }

    bool hasImage() const { return m_data->image; }
    StyleImage* image() const { return m_data->image.get(); }
    void setImage(RefPtr<StyleImage>&& image) { m_data.access().image = WTFMove(image); }

    const LengthBox& imageSlices() const { return m_data->imageSlices; }
    void setImageSlices(LengthBox slices) { m_data.access().imageSlices = WTFMove(slices); }

    bool fill() const { return m_data->fill; }
    void setFill(bool fill) { m_data.access().fill = fill; }

    const LengthBox& borderSlices() const { return m_data->borderSlices; }
    void setBorderSlices(LengthBox slices) { m_data.access().borderSlices = WTFMove(slices); }

    const LengthBox& outset() const { return m_data->outset; }
    void setOutset(LengthBox outset) { m_data.access().outset = WTFMove(outset); }

    ENinePieceImageRule horizontalRule() const { return static_cast<ENinePieceImageRule>(m_data->horizontalRule); }
    void setHorizontalRule(ENinePieceImageRule rule) { m_data.access().horizontalRule = rule; }
    
    ENinePieceImageRule verticalRule() const { return static_cast<ENinePieceImageRule>(m_data->verticalRule); }
    void setVerticalRule(ENinePieceImageRule rule) { m_data.access().verticalRule = rule; }

    void copyImageSlicesFrom(const NinePieceImage& other)
    {
        m_data.access().imageSlices = other.m_data->imageSlices;
        m_data.access().fill = other.m_data->fill;
    }

    void copyBorderSlicesFrom(const NinePieceImage& other)
    {
        m_data.access().borderSlices = other.m_data->borderSlices;
    }
    
    void copyOutsetFrom(const NinePieceImage& other)
    {
        m_data.access().outset = other.m_data->outset;
    }

    void copyRepeatFrom(const NinePieceImage& other)
    {
        m_data.access().horizontalRule = other.m_data->horizontalRule;
        m_data.access().verticalRule = other.m_data->verticalRule;
    }

    void setMaskDefaults()
    {
        m_data.access().imageSlices = LengthBox(0);
        m_data.access().fill = true;
        m_data.access().borderSlices = LengthBox();
    }

    static LayoutUnit computeOutset(const Length& outsetSide, LayoutUnit borderSide)
    {
        if (outsetSide.isRelative())
            return LayoutUnit(outsetSide.value() * borderSide);
        return LayoutUnit(outsetSide.value());
    }

    static LayoutUnit computeSlice(Length, LayoutUnit width, LayoutUnit slice, LayoutUnit extent);
    static LayoutBoxExtent computeSlices(const LayoutSize&, const LengthBox& lengths, int scaleFactor);
    static LayoutBoxExtent computeSlices(const LayoutSize&, const LengthBox& lengths, const FloatBoxExtent& widths, const LayoutBoxExtent& slices);

    static bool isEmptyPieceRect(ImagePiece, const LayoutBoxExtent& slices);
    static bool isEmptyPieceRect(ImagePiece, const Vector<FloatRect>& destinationRects, const Vector<FloatRect>& sourceRects);

    static Vector<FloatRect> computeNineRects(const FloatRect& outer, const LayoutBoxExtent& slices, float deviceScaleFactor);

    static void scaleSlicesIfNeeded(const LayoutSize&, LayoutBoxExtent& slices, float deviceScaleFactor);

    static FloatSize computeSideTileScale(ImagePiece, const Vector<FloatRect>& destinationRects, const Vector<FloatRect>& sourceRects);
    static FloatSize computeMiddleTileScale(const Vector<FloatSize>& scales, const Vector<FloatRect>& destinationRects, const Vector<FloatRect>& sourceRects, ENinePieceImageRule hRule, ENinePieceImageRule vRule);
    static Vector<FloatSize> computeTileScales(const Vector<FloatRect>& destinationRects, const Vector<FloatRect>& sourceRects, ENinePieceImageRule hRule, ENinePieceImageRule vRule);

    void paint(GraphicsContext&, RenderElement*, const RenderStyle&, const LayoutRect& destination, const LayoutSize& source, float deviceScaleFactor, CompositeOperator) const;

private:
    struct Data : RefCounted<Data> {
        static Ref<Data> create();
        static Ref<Data> create(RefPtr<StyleImage>&&, LengthBox imageSlices, bool fill, LengthBox borderSlices, LengthBox outset, ENinePieceImageRule horizontalRule, ENinePieceImageRule verticalRule);
        Ref<Data> copy() const;

        bool operator==(const Data&) const;
        bool operator!=(const Data& other) const { return !(*this == other); }

        bool fill : 1;
        unsigned horizontalRule : 2; // ENinePieceImageRule
        unsigned verticalRule : 2; // ENinePieceImageRule
        RefPtr<StyleImage> image;
        LengthBox imageSlices { { 100, Percent }, { 100, Percent }, { 100, Percent }, { 100, Percent } };
        LengthBox borderSlices { { 1, Relative }, { 1, Relative }, { 1, Relative }, { 1, Relative } };
        LengthBox outset { 0 };

    private:
        Data();
        Data(RefPtr<StyleImage>&&, LengthBox imageSlices, bool fill, LengthBox borderSlices, LengthBox outset, ENinePieceImageRule horizontalRule, ENinePieceImageRule verticalRule);
        Data(const Data&);
    };

    static DataRef<Data>& defaultData();

    DataRef<Data> m_data;
};

WTF::TextStream& operator<<(WTF::TextStream&, const NinePieceImage&);

} // namespace WebCore
