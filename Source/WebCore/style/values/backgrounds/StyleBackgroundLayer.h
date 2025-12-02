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

#include <WebCore/CSSPropertyNames.h>
#include <WebCore/GraphicsTypes.h>
#include <WebCore/RenderStyleConstants.h>
#include <WebCore/StyleBackgroundSize.h>
#include <WebCore/StyleCoordinatedValueListValue.h>
#include <WebCore/StyleImageOrNone.h>
#include <WebCore/StyleMaskMode.h>
#include <WebCore/StylePosition.h>
#include <WebCore/StyleRepeatStyle.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class RenderElement;

namespace Style {

// macro(ownerType, property, type, lowercaseName, uppercaseName)

#define FOR_EACH_BACKGROUND_LAYER_REFERENCE(macro) \
    macro(BackgroundLayer, BackgroundImage, ImageOrNone, image, Image) \
    macro(BackgroundLayer, BackgroundPositionX, PositionX, positionX, PositionX) \
    macro(BackgroundLayer, BackgroundPositionY, PositionY, positionY, PositionY) \
    macro(BackgroundLayer, BackgroundSize, BackgroundSize, size, Size) \
    macro(BackgroundLayer, BackgroundRepeat, RepeatStyle, repeat, Repeat) \
\

#define FOR_EACH_BACKGROUND_LAYER_ENUM(macro) \
    macro(BackgroundLayer, BackgroundAttachment, FillAttachment, attachment, Attachment) \
    macro(BackgroundLayer, BackgroundClip, FillBox, clip, Clip) \
    macro(BackgroundLayer, BackgroundOrigin, FillBox, origin, Origin) \
    macro(BackgroundLayer, BackgroundBlendMode, BlendMode, blendMode, BlendMode) \
\

#define FOR_EACH_BACKGROUND_LAYER_SHORTHAND(macro) \
    macro(BackgroundLayer, BackgroundPosition, Position, position, Position) \
\

#define FOR_EACH_BACKGROUND_LAYER_PROPERTY(macro) \
    FOR_EACH_BACKGROUND_LAYER_REFERENCE(macro) \
    FOR_EACH_BACKGROUND_LAYER_ENUM(macro) \
\

struct BackgroundLayer {
    BackgroundLayer();
    BackgroundLayer(CSS::Keyword::None);
    BackgroundLayer(ImageOrNone&&);
    BackgroundLayer(RefPtr<StyleImage>&&);

    const ImageOrNone& image() const { return m_image; }
    const PositionX& positionX() const { return m_positionX; }
    const PositionY& positionY() const { return m_positionY; }
    const BackgroundSize& size() const { return m_size; }
    const RepeatStyle& repeat() const { return m_repeat; }
    FillAttachment attachment() const { return static_cast<FillAttachment>(m_attachment); }
    FillBox clip() const { return static_cast<FillBox>(m_clip); }
    FillBox origin() const { return static_cast<FillBox>(m_origin); }
    BlendMode blendMode() const { return static_cast<BlendMode>(m_blendMode); }

    static constexpr CompositeOperator composite() { return CompositeOperator::SourceOver; }
    static constexpr MaskMode maskMode() { return MaskMode::MatchSource; }

    CompositeOperator compositeForPainting(bool /* isLastLayer */) const
    {
        return composite();
    }

    static ImageOrNone initialImage() { return CSS::Keyword::None { }; }
    static PositionX initialPositionX() { using namespace CSS::Literals; return 0_css_percentage; }
    static PositionY initialPositionY() { using namespace CSS::Literals; return 0_css_percentage; }
    static BackgroundSize initialSize() { return CSS::Keyword::Auto { }; }
    static constexpr RepeatStyle initialRepeat() { return { .values { FillRepeat::Repeat, FillRepeat::Repeat } }; }
    static constexpr FillAttachment initialAttachment() { return FillAttachment::ScrollBackground; }
    static constexpr FillBox initialClip() { return FillBox::BorderBox; }
    static constexpr FillBox initialOrigin() { return FillBox::PaddingBox; }
    static constexpr BlendMode initialBlendMode() { return BlendMode::Normal; }

    bool hasImage() const { return m_image.isImage(); }
    bool hasOpaqueImage(const RenderElement&) const;
    bool hasRepeatXY() const { return repeat() == FillRepeat::Repeat; }

    bool clipOccludesNextLayers() const { return m_clip == m_clipMax; }
    void setClipMax(FillBox clipMax) const { m_clipMax = static_cast<unsigned>(clipMax); }

    bool operator==(const BackgroundLayer&) const;

    FOR_EACH_BACKGROUND_LAYER_REFERENCE(DECLARE_COORDINATED_VALUE_LIST_GETTER_AND_SETTERS_REFERENCE)
    FOR_EACH_BACKGROUND_LAYER_ENUM(DECLARE_COORDINATED_VALUE_LIST_GETTER_AND_SETTERS_ENUM)

    // Support for the `background-position` shorthand.
    static Position initialPosition() { return { initialPositionX(), initialPositionY() }; }
    Position position() const { return { m_positionX, m_positionY }; }
    void setPosition(Position&& position) { setPositionX(WTFMove(position.x)); setPositionY(WTFMove(position.y)); }
    void fillPosition(Position&& position) { fillPositionX(WTFMove(position.x)); fillPositionY(WTFMove(position.y)); }
    void clearPosition() { clearPositionX(); clearPositionY(); }
    bool isPositionUnset() const { return isPositionXUnset() && isPositionYUnset(); }
    bool isPositionSet() const { return isPositionXSet() || isPositionYSet(); }
    bool isPositionFilled() const { return isPositionXFilled() || isPositionYFilled(); }

    // CoordinatedValueList interface.

    static constexpr auto computedValueUsesUsedValues = true;
    static constexpr auto baseProperty = PropertyNameConstant<CSSPropertyBackgroundImage> { };
    static constexpr auto properties = std::tuple { FOR_EACH_BACKGROUND_LAYER_PROPERTY(DECLARE_COORDINATED_VALUE_LIST_PROPERTY) };
    static BackgroundLayer clone(const BackgroundLayer& other) { return other; }
    bool isInitial() const { return m_image.isNone(); }

private:
    ImageOrNone m_image;
    PositionX m_positionX;
    PositionY m_positionY;
    BackgroundSize m_size;
    RepeatStyle m_repeat;

    PREFERRED_TYPE(FillAttachment) unsigned m_attachment : 2;
    PREFERRED_TYPE(FillBox) unsigned m_clip : FillBoxBitWidth;
    PREFERRED_TYPE(FillBox) unsigned m_origin : FillBoxBitWidth;
    PREFERRED_TYPE(BlendMode) unsigned m_blendMode : 5;

    PREFERRED_TYPE(FillBox) mutable unsigned m_clipMax : FillBoxBitWidth; // maximum m_clip value from this to bottom layer

    FOR_EACH_BACKGROUND_LAYER_PROPERTY(DECLARE_COORDINATED_VALUE_LIST_IS_SET_AND_IS_FILLED_MEMBERS)

    // Needed by macros to access members.
    BackgroundLayer& data() { return *this; }
    const BackgroundLayer& data() const { return *this; }
};

FOR_EACH_BACKGROUND_LAYER_REFERENCE(DECLARE_COORDINATED_VALUE_LIST_PROPERTY_ACCESSOR_REFERENCE)
FOR_EACH_BACKGROUND_LAYER_ENUM(DECLARE_COORDINATED_VALUE_LIST_PROPERTY_ACCESSOR_ENUM)
FOR_EACH_BACKGROUND_LAYER_SHORTHAND(DECLARE_COORDINATED_VALUE_LIST_PROPERTY_ACCESSOR_SHORTHAND)

// MARK: - Blending

template<> struct Blending<BackgroundLayer> {
    auto canBlend(const BackgroundLayer&, const BackgroundLayer&) -> bool;
};

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream&, const BackgroundLayer&);

#undef FOR_EACH_BACKGROUND_LAYER_REFERENCE
#undef FOR_EACH_BACKGROUND_LAYER_ENUM
#undef FOR_EACH_BACKGROUND_LAYER_SHORTHAND
#undef FOR_EACH_BACKGROUND_LAYER_PROPERTY

} // namespace Style
} // namespace WebCore
