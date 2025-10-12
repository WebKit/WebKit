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

struct BackgroundLayer {
    static constexpr FillLayerType type() { return FillLayerType::Background; }

    BackgroundLayer(CSS::Keyword::None);
    BackgroundLayer(ImageOrNone&&);
    BackgroundLayer(RefPtr<StyleImage>&&);

    const ImageOrNone& image() const { return m_image; }
    const Position& position() const { return m_position; }
    const PositionX& xPosition() const { return m_position.x; }
    const PositionY& yPosition() const { return m_position.y; }
    const BackgroundSize& size() const { return m_size; }
    FillAttachment attachment() const { return static_cast<FillAttachment>(m_attachment); }
    FillBox clip() const { return static_cast<FillBox>(m_clip); }
    FillBox origin() const { return static_cast<FillBox>(m_origin); }
    RepeatStyle repeat() const { return m_repeat; }
    BlendMode blendMode() const { return static_cast<BlendMode>(m_blendMode); }

    static constexpr CompositeOperator composite() { return CompositeOperator::SourceOver; }
    static constexpr MaskMode maskMode() { return MaskMode::MatchSource; }

    CompositeOperator compositeForPainting(bool /* isLastLayer */) const
    {
        return composite();
    }

    bool isEmpty() const { return m_size.isEmpty(); }

    void setImage(ImageOrNone&& image) { m_image = WTFMove(image); }
    void setXPosition(PositionX&& positionX) { m_position.x = WTFMove(positionX);}
    void setYPosition(PositionY&& positionY) { m_position.y = WTFMove(positionY); }
    void setSize(BackgroundSize&& size) { m_size = WTFMove(size); }
    void setAttachment(FillAttachment attachment) { m_attachment = static_cast<unsigned>(attachment); }
    void setClip(FillBox b) { m_clip = static_cast<unsigned>(b); }
    void setOrigin(FillBox b) { m_origin = static_cast<unsigned>(b); }
    void setRepeat(RepeatStyle r) { m_repeat = r; }
    void setBlendMode(BlendMode b) { m_blendMode = static_cast<unsigned>(b); }

    static ImageOrNone initialFillImage() { return CSS::Keyword::None { }; }
    static FillAttachment initialFillAttachment() { return FillAttachment::ScrollBackground; }
    static FillBox initialFillClip() { return FillBox::BorderBox; }
    static FillBox initialFillOrigin() { return FillBox::PaddingBox; }
    static RepeatStyle initialFillRepeat() { return { .values { FillRepeat::Repeat, FillRepeat::Repeat } }; }
    static BlendMode initialFillBlendMode() { return BlendMode::Normal; }
    static BackgroundSize initialFillSize() { return CSS::Keyword::Auto { }; }
    static PositionX initialFillXPosition() { using namespace CSS::Literals; return 0_css_percentage; }
    static PositionY initialFillYPosition() { using namespace CSS::Literals; return 0_css_percentage; }

    bool hasImage() const { return m_image.isImage(); }
    bool hasOpaqueImage(const RenderElement&) const;
    bool hasRepeatXY() const { return repeat() == FillRepeat::Repeat; }

    bool clipOccludesNextLayers() const { return m_clip == m_clipMax; }
    void setClipMax(FillBox clipMax) const { m_clipMax = static_cast<unsigned>(clipMax); }

    bool operator==(const BackgroundLayer&) const;

private:
    ImageOrNone m_image;
    Position m_position;
    BackgroundSize m_size;
    RepeatStyle m_repeat;

    PREFERRED_TYPE(FillAttachment) unsigned m_attachment : 2;
    PREFERRED_TYPE(FillBox) unsigned m_clip : FillBoxBitWidth;
    PREFERRED_TYPE(FillBox) unsigned m_origin : FillBoxBitWidth;
    PREFERRED_TYPE(BlendMode) unsigned m_blendMode : 5;

    PREFERRED_TYPE(FillBox) mutable unsigned m_clipMax : FillBoxBitWidth; // maximum m_clip value from this to bottom layer
};

using BackgroundLayers = FillLayers<BackgroundLayer>;

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream&, const BackgroundLayer&);

} // namespace Style
} // namespace WebCore
