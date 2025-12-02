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

#define FOR_EACH_MASK_LAYER_REFERENCE(macro) \
    macro(MaskLayer, MaskImage, ImageOrNone, image, Image) \
    macro(MaskLayer, WebkitMaskPositionX, PositionX, positionX, PositionX) \
    macro(MaskLayer, WebkitMaskPositionY, PositionY, positionY, PositionY) \
    macro(MaskLayer, MaskSize, BackgroundSize, size, Size) \
    macro(MaskLayer, MaskRepeat, RepeatStyle, repeat, Repeat) \
\

#define FOR_EACH_MASK_LAYER_ENUM(macro) \
    macro(MaskLayer, MaskClip, FillBox, clip, Clip) \
    macro(MaskLayer, MaskOrigin, FillBox, origin, Origin) \
    macro(MaskLayer, MaskComposite, CompositeOperator, composite, Composite) \
    macro(MaskLayer, MaskMode, MaskMode, maskMode, MaskMode) \
\

#define FOR_EACH_MASK_LAYER_SHORTHAND(macro) \
    macro(MaskLayer, MaskPosition, Position, position, Position) \
\

#define FOR_EACH_MASK_LAYER_PROPERTY(macro) \
    FOR_EACH_MASK_LAYER_REFERENCE(macro) \
    FOR_EACH_MASK_LAYER_ENUM(macro) \
\

struct MaskLayer {
    MaskLayer();
    MaskLayer(CSS::Keyword::None);
    MaskLayer(ImageOrNone&&);
    MaskLayer(RefPtr<StyleImage>&&);

    const ImageOrNone& image() const { return m_image; }
    const PositionX& positionX() const { return m_positionX; }
    const PositionY& positionY() const { return m_positionY; }
    const BackgroundSize& size() const { return m_size; }
    FillBox clip() const { return static_cast<FillBox>(m_clip); }
    FillBox origin() const { return static_cast<FillBox>(m_origin); }
    const RepeatStyle& repeat() const { return m_repeat; }
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

    static ImageOrNone initialImage() { return CSS::Keyword::None { }; }
    static PositionX initialPositionX() { using namespace CSS::Literals; return 0_css_percentage; }
    static PositionY initialPositionY() { using namespace CSS::Literals; return 0_css_percentage; }
    static BackgroundSize initialSize() { return CSS::Keyword::Auto { }; }
    static constexpr RepeatStyle initialRepeat() { return { .values { FillRepeat::Repeat, FillRepeat::Repeat } }; }
    static constexpr FillBox initialClip() { return FillBox::BorderBox; }
    static constexpr FillBox initialOrigin() { return FillBox::BorderBox; }
    static constexpr CompositeOperator initialComposite() { return CompositeOperator::SourceOver; }
    static constexpr MaskMode initialMaskMode() { return MaskMode::MatchSource; }

    bool hasImage() const { return m_image.isImage(); }
    bool hasOpaqueImage(const RenderElement&) const;
    bool hasRepeatXY() const { return repeat() == FillRepeat::Repeat; }

    bool clipOccludesNextLayers() const { return m_clip == m_clipMax; }
    void setClipMax(FillBox clipMax) const { m_clipMax = static_cast<unsigned>(clipMax); }

    bool operator==(const MaskLayer&) const;

    FOR_EACH_MASK_LAYER_REFERENCE(DECLARE_COORDINATED_VALUE_LIST_GETTER_AND_SETTERS_REFERENCE)
    FOR_EACH_MASK_LAYER_ENUM(DECLARE_COORDINATED_VALUE_LIST_GETTER_AND_SETTERS_ENUM)

    // Support for the `mask-position` shorthand.
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
    static constexpr auto baseProperty = PropertyNameConstant<CSSPropertyMaskImage> { };
    static constexpr auto properties =  std::tuple { FOR_EACH_MASK_LAYER_PROPERTY(DECLARE_COORDINATED_VALUE_LIST_PROPERTY) };
    static MaskLayer clone(const MaskLayer& other) { return other; }
    bool isInitial() const { return m_image.isNone(); }

private:
    ImageOrNone m_image;
    PositionX m_positionX;
    PositionY m_positionY;
    BackgroundSize m_size;
    RepeatStyle m_repeat;

    PREFERRED_TYPE(FillBox) unsigned m_clip : FillBoxBitWidth;
    PREFERRED_TYPE(FillBox) unsigned m_origin : FillBoxBitWidth;
    PREFERRED_TYPE(CompositeOperator) unsigned m_composite : 4;
    PREFERRED_TYPE(MaskMode) unsigned m_maskMode : 2;

    PREFERRED_TYPE(FillBox) mutable unsigned m_clipMax : FillBoxBitWidth; // maximum m_clip value from this to bottom layer

    FOR_EACH_MASK_LAYER_PROPERTY(DECLARE_COORDINATED_VALUE_LIST_IS_SET_AND_IS_FILLED_MEMBERS)

    // Needed by macros to access members.
    MaskLayer& data() { return *this; }
    const MaskLayer& data() const { return *this; }
};

FOR_EACH_MASK_LAYER_REFERENCE(DECLARE_COORDINATED_VALUE_LIST_PROPERTY_ACCESSOR_REFERENCE)
FOR_EACH_MASK_LAYER_ENUM(DECLARE_COORDINATED_VALUE_LIST_PROPERTY_ACCESSOR_ENUM)
FOR_EACH_MASK_LAYER_SHORTHAND(DECLARE_COORDINATED_VALUE_LIST_PROPERTY_ACCESSOR_SHORTHAND)

// MARK: - Blending

template<> struct Blending<MaskLayer> {
    auto canBlend(const MaskLayer&, const MaskLayer&) -> bool;
};

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream&, const MaskLayer&);

#undef FOR_EACH_MASK_LAYER_REFERENCE
#undef FOR_EACH_MASK_LAYER_ENUM
#undef FOR_EACH_MASK_LAYER_SHORTHAND
#undef FOR_EACH_MASK_LAYER_PROPERTY

} // namespace Style
} // namespace WebCore
