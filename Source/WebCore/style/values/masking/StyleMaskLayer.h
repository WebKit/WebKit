/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008, 2014 Apple Inc. All rights reserved.
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

#include <WebCore/GraphicsTypes.h>
#include <WebCore/RenderStyleConstants.h>
#include <WebCore/StyleBackgroundSize.h>
#include <WebCore/StyleFillLayers.h>
#include <WebCore/StyleImageOrNone.h>
#include <WebCore/StylePosition.h>
#include <WebCore/StyleRepeatStyle.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class RenderElement;

namespace Style {

struct MaskLayer {
    static constexpr FillLayerType type() { return FillLayerType::Mask; }

    MaskLayer(CSS::Keyword::None);
    MaskLayer(ImageOrNone&&);
    MaskLayer(RefPtr<StyleImage>&&);

    const ImageOrNone& image() const { return m_image; }
    const Position& position() const { return m_position; }
    const PositionX& xPosition() const { return m_position.x; }
    const PositionY& yPosition() const { return m_position.y; }
    const BackgroundSize& size() const { return m_size; }
    FillBox clip() const { return static_cast<FillBox>(m_clip); }
    FillBox origin() const { return static_cast<FillBox>(m_origin); }
    RepeatStyle repeat() const { return m_repeat; }
    CompositeOperator composite() const { return static_cast<CompositeOperator>(m_composite); }
    MaskMode maskMode() const { return static_cast<MaskMode>(m_maskMode); }

    static constexpr FillAttachment attachment() { return FillAttachment::ScrollBackground; }
    static constexpr BlendMode blendMode() { return BlendMode::Normal; }

    // https://drafts.fxtf.org/css-masking/#the-mask-composite
    // If there is no further mask layer, the compositing operator must be ignored.
    CompositeOperator compositeForPainting(bool isLastLayer) const
    {
        if (isLastLayer)
            return CompositeOperator::SourceOver;
        return composite();
    }

    bool isEmpty() const { return m_size.isEmpty(); }

    void setImage(ImageOrNone&& image) { m_image = WTFMove(image); }
    void setXPosition(PositionX&& positionX) { m_position.x = WTFMove(positionX); }
    void setYPosition(PositionY&& positionY) { m_position.y = WTFMove(positionY); }
    void setSize(BackgroundSize&& size) { m_size = WTFMove(size); }
    void setClip(FillBox b) { m_clip = static_cast<unsigned>(b); }
    void setOrigin(FillBox b) { m_origin = static_cast<unsigned>(b); }
    void setRepeat(RepeatStyle r) { m_repeat = r; }
    void setComposite(CompositeOperator c) { m_composite = static_cast<unsigned>(c); }
    void setMaskMode(MaskMode m) { m_maskMode = static_cast<unsigned>(m); }

    static ImageOrNone initialFillImage() { return CSS::Keyword::None { }; }
    static FillBox initialFillClip() { return FillBox::BorderBox; }
    static FillBox initialFillOrigin() { return FillBox::BorderBox; }
    static RepeatStyle initialFillRepeat() { return { .values { FillRepeat::Repeat, FillRepeat::Repeat } }; }
    static CompositeOperator initialFillComposite() { return CompositeOperator::SourceOver; }
    static BackgroundSize initialFillSize() { return CSS::Keyword::Auto { }; }
    static PositionX initialFillXPosition() { using namespace CSS::Literals; return 0_css_percentage; }
    static PositionY initialFillYPosition() { using namespace CSS::Literals; return 0_css_percentage; }
    static MaskMode initialFillMaskMode() { return MaskMode::MatchSource; }

    bool hasImage() const { return m_image.isImage(); }
    bool hasOpaqueImage(const RenderElement&) const;
    bool hasRepeatXY() const { return repeat() == FillRepeat::Repeat; }

    bool clipOccludesNextLayers() const { return m_clip == m_clipMax; }
    void setClipMax(FillBox clipMax) const { m_clipMax = static_cast<unsigned>(clipMax); }

    bool operator==(const MaskLayer&) const;

private:
    ImageOrNone m_image;
    Position m_position;
    BackgroundSize m_size;
    RepeatStyle m_repeat;

    PREFERRED_TYPE(FillBox) unsigned m_clip : FillBoxBitWidth;
    PREFERRED_TYPE(FillBox) unsigned m_origin : FillBoxBitWidth;
    PREFERRED_TYPE(CompositeOperator) unsigned m_composite : 4;
    PREFERRED_TYPE(MaskMode) unsigned m_maskMode : 2;

    PREFERRED_TYPE(FillBox) mutable unsigned m_clipMax : FillBoxBitWidth; // maximum m_clip value from this to bottom layer
};

using MaskLayers = FillLayers<MaskLayer>;

WTF::TextStream& operator<<(WTF::TextStream&, const MaskLayer&);

} // namespace Style
} // namespace WebCore
