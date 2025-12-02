/*
 * Copyright (C) 2013-2014 Google Inc. All rights reserved.
 * Copyright (C) 2014-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSBoxShadowPropertyValue.h"
#include "CSSCalcSymbolTable.h"
#include "CSSColorValue.h"
#include "CSSCounterStyleRegistry.h"
#include "CSSCounterStyleRule.h"
#include "CSSCounterValue.h"
#include "CSSCursorImageValue.h"
#include "CSSFontValue.h"
#include "CSSGradientValue.h"
#include "CSSPropertyParserConsumer+Font.h"
#include "CSSRatioValue.h"
#include "CSSRectValue.h"
#include "CSSRegisteredCustomProperty.h"
#include "CSSTextShadowPropertyValue.h"
#include "CSSURLValue.h"
#include "ElementAncestorIteratorInlines.h"
#include "HTMLElement.h"
#include "LocalFrame.h"
#include "SVGElement.h"
#include "StyleBoxShadow.h"
#include "StyleBuilderConverter.h"
#include "StyleBuilderStateInlines.h"
#include "StyleCachedImage.h"
#include "StyleCursorImage.h"
#include "StyleCustomPropertyData.h"
#include "StyleFontSizeFunctions.h"
#include "StyleGeneratedImage.h"
#include "StyleImageSet.h"
#include "StyleLineWidth.h"
#include "StyleRatio.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include "StyleTextShadow.h"
#include "TransformOperations.h"

namespace WebCore {
namespace Style {

#define DECLARE_PROPERTY_CUSTOM_HANDLERS(property) \
    static void applyInherit##property(BuilderState&); \
    static void applyInitial##property(BuilderState&); \
    static void applyValue##property(BuilderState&, CSSValue&)

template<typename T> inline T forwardInheritedValue(T&& value) { return std::forward<T>(value); }
template<auto R, typename V> inline Length<R, V> forwardInheritedValue(const Length<R, V>& value) { auto copy = value; return copy; }
inline AccentColor forwardInheritedValue(const AccentColor& value) { auto copy = value; return copy; }
inline AnchorNames forwardInheritedValue(const AnchorNames& value) { auto copy = value; return copy; }
inline AppleColorFilter forwardInheritedValue(const AppleColorFilter& value) { auto copy = value; return copy; }
inline AspectRatio forwardInheritedValue(const AspectRatio& value) { auto copy = value; return copy; }
inline BackgroundSize forwardInheritedValue(const BackgroundSize& value) { auto copy = value; return copy; }
inline BlockEllipsis forwardInheritedValue(const BlockEllipsis& value) { auto copy = value; return copy; }
inline BlockStepSize forwardInheritedValue(const BlockStepSize& value) { auto copy = value; return copy; }
inline BorderImageSource forwardInheritedValue(const BorderImageSource& value) { auto copy = value; return copy; }
inline BorderRadiusValue forwardInheritedValue(const BorderRadiusValue& value) { auto copy = value; return copy; }
inline BoxShadows forwardInheritedValue(const BoxShadows& value) { auto copy = value; return copy; }
inline ContainIntrinsicSize forwardInheritedValue(const ContainIntrinsicSize& value) { auto copy = value; return copy; }
inline ContainerNames forwardInheritedValue(const ContainerNames& value) { auto copy = value; return copy; }
inline Content forwardInheritedValue(const Content& value) { auto copy = value; return copy; }
inline WebCore::Color forwardInheritedValue(const WebCore::Color& value) { auto copy = value; return copy; }
inline Color forwardInheritedValue(const Color& value) { auto copy = value; return copy; }
inline EasingFunction forwardInheritedValue(const EasingFunction& value) { auto copy = value; return copy; }
inline GapGutter forwardInheritedValue(const GapGutter& value) { auto copy = value; return copy; }
inline FontFamilies forwardInheritedValue(const FontFamilies& value) { auto copy = value; return copy; }
inline FilterOperations forwardInheritedValue(const FilterOperations& value) { auto copy = value; return copy; }
inline TransformOperations forwardInheritedValue(const TransformOperations& value) { auto copy = value; return copy; }
inline ScrollMarginEdge forwardInheritedValue(const ScrollMarginEdge& value) { auto copy = value; return copy; }
inline ScrollPaddingEdge forwardInheritedValue(const ScrollPaddingEdge& value) { auto copy = value; return copy; }
inline MaskBorderSource forwardInheritedValue(const MaskBorderSource& value) { auto copy = value; return copy; }
inline MarginEdge forwardInheritedValue(const MarginEdge& value) { auto copy = value; return copy; }
inline PaddingEdge forwardInheritedValue(const PaddingEdge& value) { auto copy = value; return copy; }
inline ImageOrNone forwardInheritedValue(const ImageOrNone& value) { auto copy = value; return copy; }
inline InsetEdge forwardInheritedValue(const InsetEdge& value) { auto copy = value; return copy; }
inline Perspective forwardInheritedValue(const Perspective& value) { auto copy = value; return copy; }
inline Quotes forwardInheritedValue(const Quotes& value) { auto copy = value; return copy; }
inline Rotate forwardInheritedValue(const Rotate& value) { auto copy = value; return copy; }
inline Scale forwardInheritedValue(const Scale& value) { auto copy = value; return copy; }
inline Translate forwardInheritedValue(const Translate& value) { auto copy = value; return copy; }
inline PreferredSize forwardInheritedValue(const PreferredSize& value) { auto copy = value; return copy; }
inline MinimumSize forwardInheritedValue(const MinimumSize& value) { auto copy = value; return copy; }
inline MaximumSize forwardInheritedValue(const MaximumSize& value) { auto copy = value; return copy; }
inline Filter forwardInheritedValue(const Filter& value) { auto copy = value; return copy; }
inline FlexBasis forwardInheritedValue(const FlexBasis& value) { auto copy = value; return copy; }
inline DynamicRangeLimit forwardInheritedValue(const DynamicRangeLimit& value) { auto copy = value; return copy; }
inline Clip forwardInheritedValue(const Clip& value) { auto copy = value; return copy; }
inline ClipPath forwardInheritedValue(const ClipPath& value) { auto copy = value; return copy; }
inline CornerShapeValue forwardInheritedValue(const CornerShapeValue& value) { auto copy = value; return copy; }
inline GridPosition forwardInheritedValue(const GridPosition& value) { auto copy = value; return copy; }
inline GridTemplateAreas forwardInheritedValue(const GridTemplateAreas& value) { auto copy = value; return copy; }
inline GridTemplateList forwardInheritedValue(const GridTemplateList& value) { auto copy = value; return copy; }
inline GridTrackSizes forwardInheritedValue(const GridTrackSizes& value) { auto copy = value; return copy; }
inline HyphenateCharacter forwardInheritedValue(const HyphenateCharacter& value) { auto copy = value; return copy; }
inline ItemTolerance forwardInheritedValue(const ItemTolerance& value) { auto copy = value; return copy; }
inline LetterSpacing forwardInheritedValue(const LetterSpacing& value) { auto copy = value; return copy; }
inline LineHeight forwardInheritedValue(const LineHeight& value) { auto copy = value; return copy; }
inline ListStyleType forwardInheritedValue(const ListStyleType& value) { auto copy = value; return copy; }
inline NameScope forwardInheritedValue(const NameScope& value) { auto copy = value; return copy; }
inline OffsetAnchor forwardInheritedValue(const OffsetAnchor& value) { auto copy = value; return copy; }
inline OffsetDistance forwardInheritedValue(const OffsetDistance& value) { auto copy = value; return copy; }
inline OffsetPath forwardInheritedValue(const OffsetPath& value) { auto copy = value; return copy; }
inline OffsetPosition forwardInheritedValue(const OffsetPosition& value) { auto copy = value; return copy; }
inline OffsetRotate forwardInheritedValue(const OffsetRotate& value) { auto copy = value; return copy; }
inline Position forwardInheritedValue(const Position& value) { auto copy = value; return copy; }
inline PositionAnchor forwardInheritedValue(const PositionAnchor& value) { auto copy = value; return copy; }
inline PositionTryFallbacks forwardInheritedValue(const PositionTryFallbacks& value) { auto copy = value; return copy; }
inline PositionX forwardInheritedValue(const PositionX& value) { auto copy = value; return copy; }
inline PositionY forwardInheritedValue(const PositionY& value) { auto copy = value; return copy; }
inline RepeatStyle forwardInheritedValue(const RepeatStyle& value) { auto copy = value; return copy; }
inline SVGBaselineShift forwardInheritedValue(const SVGBaselineShift& value) { auto copy = value; return copy; }
inline SVGCenterCoordinateComponent forwardInheritedValue(const SVGCenterCoordinateComponent& value) { auto copy = value; return copy; }
inline SVGCoordinateComponent forwardInheritedValue(const SVGCoordinateComponent& value) { auto copy = value; return copy; }
inline SVGMarkerResource forwardInheritedValue(const SVGMarkerResource& value) { auto copy = value; return copy; }
inline SVGPathData forwardInheritedValue(const SVGPathData& value) { auto copy = value; return copy; }
inline SVGPaint forwardInheritedValue(const SVGPaint& value) { auto copy = value; return copy; }
inline SVGRadius forwardInheritedValue(const SVGRadius& value) { auto copy = value; return copy; }
inline SVGRadiusComponent forwardInheritedValue(const SVGRadiusComponent& value) { auto copy = value; return copy; }
inline SVGStrokeDasharray forwardInheritedValue(const SVGStrokeDasharray& value) { auto copy = value; return copy; }
inline SVGStrokeDashoffset forwardInheritedValue(const SVGStrokeDashoffset& value) { auto copy = value; return copy; }
inline ScrollSnapAlign forwardInheritedValue(const ScrollSnapAlign& value) { auto copy = value; return copy; }
inline ScrollSnapType forwardInheritedValue(const ScrollSnapType& value) { auto copy = value; return copy; }
inline ScrollbarColor forwardInheritedValue(const ScrollbarColor& value) { auto copy = value; return copy; }
inline ScrollbarGutter forwardInheritedValue(const ScrollbarGutter& value) { auto copy = value; return copy; }
inline ShapeMargin forwardInheritedValue(const ShapeMargin& value) { auto copy = value; return copy; }
inline ShapeOutside forwardInheritedValue(const ShapeOutside& value) { auto copy = value; return copy; }
inline SingleAnimationName forwardInheritedValue(const SingleAnimationName& value) { auto copy = value; return copy; }
inline SingleAnimationRangeStart forwardInheritedValue(const SingleAnimationRangeStart& value) { auto copy = value; return copy; }
inline SingleAnimationRangeEnd forwardInheritedValue(const SingleAnimationRangeEnd& value) { auto copy = value; return copy; }
inline SingleAnimationRange forwardInheritedValue(const SingleAnimationRange& value) { auto copy = value; return copy; }
inline SingleAnimationTimeline forwardInheritedValue(const SingleAnimationTimeline& value) { auto copy = value; return copy; }
inline SingleTransitionProperty forwardInheritedValue(const SingleTransitionProperty& value) { auto copy = value; return copy; }
inline StrokeWidth forwardInheritedValue(const StrokeWidth& value) { auto copy = value; return copy; }
inline TabSize forwardInheritedValue(const TabSize& value) { auto copy = value; return copy; }
inline TextDecorationLine forwardInheritedValue(const TextDecorationLine& value) { auto copy = value; return copy; }
inline TextDecorationThickness forwardInheritedValue(const TextDecorationThickness& value) { auto copy = value; return copy; }
inline TextEmphasisStyle forwardInheritedValue(const TextEmphasisStyle& value) { auto copy = value; return copy; }
inline TextIndent forwardInheritedValue(const TextIndent& value) { auto copy = value; return copy; }
inline TextShadows forwardInheritedValue(const TextShadows& value) { auto copy = value; return copy; }
inline TextUnderlineOffset forwardInheritedValue(const TextUnderlineOffset& value) { auto copy = value; return copy; }
inline URL forwardInheritedValue(const URL& value) { auto copy = value; return copy; }
inline FixedVector<PositionTryFallback> forwardInheritedValue(const FixedVector<PositionTryFallback>& value) { auto copy = value; return copy; }
inline ProgressTimelineAxes forwardInheritedValue(const ProgressTimelineAxes& value) { auto copy = value; return copy; }
inline ProgressTimelineNames forwardInheritedValue(const ProgressTimelineNames& value) { auto copy = value; return copy; }
inline ScrollTimelines forwardInheritedValue(const ScrollTimelines& value) { auto copy = value; return copy; }
inline Transform forwardInheritedValue(const Transform& value) { auto copy = value; return copy; }
inline VerticalAlign forwardInheritedValue(const VerticalAlign& value) { auto copy = value; return copy; }
inline ViewTimelineInsets forwardInheritedValue(const ViewTimelineInsets& value) { auto copy = value; return copy; }
inline ViewTimelines forwardInheritedValue(const ViewTimelines& value) { auto copy = value; return copy; }
inline ViewTransitionClasses forwardInheritedValue(const ViewTransitionClasses& value) { auto copy = value; return copy; }
inline ViewTransitionName forwardInheritedValue(const ViewTransitionName& value) { auto copy = value; return copy; }
inline WebkitBoxReflect forwardInheritedValue(const WebkitBoxReflect& value) { auto copy = value; return copy; }
inline WebkitInitialLetter forwardInheritedValue(const WebkitInitialLetter& value) { auto copy = value; return copy; }
inline WebkitLineClamp forwardInheritedValue(const WebkitLineClamp& value) { auto copy = value; return copy; }
inline WebkitLineGrid forwardInheritedValue(const WebkitLineGrid& value) { auto copy = value; return copy; }
inline WebkitMarqueeIncrement forwardInheritedValue(const WebkitMarqueeIncrement& value) { auto copy = value; return copy; }
inline WillChange forwardInheritedValue(const WillChange& value) { auto copy = value; return copy; }
inline WordSpacing forwardInheritedValue(const WordSpacing& value) { auto copy = value; return copy; }

// Note that we assume the CSS parser only allows valid CSSValue types.
class BuilderCustom {
public:
    // Custom handling of inherit, initial and value setting.
    // FIXME: <https://webkit.org/b/212506> Teach makeprop.pl to generate setters for hasExplicitlySet* flags
    DECLARE_PROPERTY_CUSTOM_HANDLERS(BorderBottomLeftRadius);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(BorderBottomRightRadius);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(BorderTopLeftRadius);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(BorderTopRightRadius);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(BorderImageOutset);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(BorderImageRepeat);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(BorderImageSlice);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(BorderImageWidth);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(CaretColor);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(Color);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(CounterIncrement);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(CounterReset);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(CounterSet);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(Fill);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(FontFamily);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(FontSize);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(LetterSpacing);
#if ENABLE(TEXT_AUTOSIZING)
    DECLARE_PROPERTY_CUSTOM_HANDLERS(LineHeight);
#endif
    DECLARE_PROPERTY_CUSTOM_HANDLERS(MaskBorderOutset);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(MaskBorderRepeat);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(MaskBorderSlice);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(MaskBorderWidth);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(PaddingBottom);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(PaddingLeft);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(PaddingRight);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(PaddingTop);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(Stroke);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(WordSpacing);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(Zoom);

    // Custom handling of initial setting only.
    static void applyInitialBorderTopWidth(BuilderState&);
    static void applyInitialBorderRightWidth(BuilderState&);
    static void applyInitialBorderBottomWidth(BuilderState&);
    static void applyInitialBorderLeftWidth(BuilderState&);
    static void applyInitialOutlineWidth(BuilderState&);
    static void applyInitialColumnRuleWidth(BuilderState&);

    // Custom handling of value setting only.
    static void applyValueDirection(BuilderState&, CSSValue&);
    static void applyValueWebkitLocale(BuilderState&, CSSValue&);
    static void applyValueTextOrientation(BuilderState&, CSSValue&);
#if ENABLE(TEXT_AUTOSIZING)
    static void applyValueWebkitTextSizeAdjust(BuilderState&, CSSValue&);
#endif
    static void applyValueWebkitTextZoom(BuilderState&, CSSValue&);
    static void applyValueWritingMode(BuilderState&, CSSValue&);
    static void applyValueFontSizeAdjust(BuilderState&, CSSValue&);

#if ENABLE(DARK_MODE_CSS)
    static void applyValueColorScheme(BuilderState&, CSSValue&);
#endif

    static void applyValueStrokeWidth(BuilderState&, CSSValue&);
    static void applyValueStrokeColor(BuilderState&, CSSValue&);

private:
    static void resetUsedZoom(BuilderState&);

    enum CounterBehavior { Increment, Reset, Set };
    template<CounterBehavior>
    static void applyInheritCounter(BuilderState&);
    template<CounterBehavior>
    static void applyValueCounter(BuilderState&, CSSValue&);

    static float largerFontSize(float size);
    static float smallerFontSize(float size);
    static float determineRubyTextSizeMultiplier(BuilderState&);
};

// MARK: - CoordinatedValueList Utilities

template<auto propertyID, auto listMutableGetter, typename ListType>
void applyInitialCoordinatedValueListProperty(BuilderState& builderState)
{
    using PropertyAccessor = CoordinatedValueListPropertyAccessor<propertyID>;

    auto& list = (builderState.style().*listMutableGetter)();
    ASSERT(list.computedLength() > 0);

    PropertyAccessor { list[0] }.set(PropertyAccessor::initial());

    for (size_t i = 0; i < list.computedLength(); ++i)
        PropertyAccessor { list[i] }.clear();
}

template<auto propertyID, auto listMutableGetter, auto listGetter, typename ListType>
void applyInheritCoordinatedValueListProperty(BuilderState& builderState)
{
    using PropertyAccessor = CoordinatedValueListPropertyAccessor<propertyID>;
    using ConstPropertyAccessor = CoordinatedValueListPropertyConstAccessor<propertyID>;

    auto& list = (builderState.style().*listMutableGetter)();
    auto& parentList = (builderState.parentStyle().*listGetter)();

    size_t i = 0;
    size_t parentSize = parentList.isInitial() ? 0 : parentList.computedLength();

    for (; i < parentSize && ConstPropertyAccessor { parentList[i] }.isSet(); ++i) {
        if (list.computedLength() <= i)
            list.append(typename ListType::value_type { });
        PropertyAccessor { list[i] }.set(forwardInheritedValue(ConstPropertyAccessor { parentList[i] }.get()));
    }

    for (; i < list.computedLength(); ++i)
        PropertyAccessor { list[i] }.clear();
}

template<auto propertyID, auto listMutableGetter, auto converter, typename ListType>
void applyValueCoordinatedValueListProperty(BuilderState& builderState, CSSValue& value)
{
    using PropertyAccessor = CoordinatedValueListPropertyAccessor<propertyID>;

    auto& list = (builderState.style().*listMutableGetter)();

    auto set = [&](auto i, auto& item) {
        if (item.valueID() == CSSValueInitial)
            PropertyAccessor { list[i] }.set(PropertyAccessor::initial());
        else
            PropertyAccessor { list[i] }.set(converter(builderState, item));
    };

    size_t i = 0;
    if (RefPtr valueList = dynamicDowncast<CSSValueList>(value)) {
        for (Ref item : *valueList) {
            if (i >= list.computedLength())
                list.append(typename ListType::value_type { });

            set(i, item.get());
            ++i;
        }
    } else {
        ASSERT(list.computedLength() > 0);
        set(0, value);
        i = 1;
    }

    for (; i < list.computedLength(); ++i)
        PropertyAccessor { list[i] }.clear();
}

// MARK: - Custom conversions

inline void BuilderCustom::applyValueDirection(BuilderState& builderState, CSSValue& value)
{
    builderState.style().setDirection(fromCSSValue<TextDirection>(value));
    builderState.style().setHasExplicitlySetDirection(true);
}

inline void BuilderCustom::resetUsedZoom(BuilderState& builderState)
{
    // Reset the zoom in effect. This allows the setZoom method to accurately compute a new zoom in effect.
    builderState.setUsedZoom(builderState.parentStyle().usedZoom());
}

inline void BuilderCustom::applyInitialZoom(BuilderState& builderState)
{
    resetUsedZoom(builderState);
    builderState.setZoom(RenderStyle::initialZoom());
}

inline void BuilderCustom::applyInheritZoom(BuilderState& builderState)
{
    resetUsedZoom(builderState);
    builderState.setZoom(forwardInheritedValue(builderState.parentStyle().zoom()));
}

inline void BuilderCustom::applyValueZoom(BuilderState& builderState, CSSValue& value)
{
    auto primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return;

    if (primitiveValue->valueID() == CSSValueNormal) {
        resetUsedZoom(builderState);
        builderState.setZoom(RenderStyle::initialZoom());
    } else if (primitiveValue->isPercentage()) {
        resetUsedZoom(builderState);
        if (float percent = primitiveValue->resolveAsPercentage<float>(builderState.cssToLengthConversionData()))
            builderState.setZoom(percent / 100.0f);
    } else if (primitiveValue->isNumber()) {
        resetUsedZoom(builderState);
        if (float number = primitiveValue->resolveAsNumber<float>(builderState.cssToLengthConversionData()))
            builderState.setZoom(number);
    }
}

enum BorderImageModifierType { Outset, Repeat, Slice, Width };

template<typename T, BorderImageModifierType modifier>
class ApplyPropertyBorderImageModifier {
public:
    static void applyInheritValue(BuilderState& builderState)
    {
        T image(getValue(builderState.style()));
        switch (modifier) {
        case Outset:
            image.copyOutsetFrom(getValue(builderState.parentStyle()));
            break;
        case Repeat:
            image.copyRepeatFrom(getValue(builderState.parentStyle()));
            break;
        case Slice:
            image.copySliceFrom(getValue(builderState.parentStyle()));
            break;
        case Width:
            image.copyWidthFrom(getValue(builderState.parentStyle()));
            break;
        }
        setValue(builderState.style(), WTFMove(image));
    }

    static void applyInitialValue(BuilderState& builderState)
    {
        T image(getValue(builderState.style()));
        switch (modifier) {
        case Outset:
            image.copyOutsetFrom(initialValue());
            break;
        case Repeat:
            image.copyRepeatFrom(initialValue());
            break;
        case Slice:
            image.copySliceFrom(initialValue());
            break;
        case Width:
            image.copyWidthFrom(initialValue());
            break;
        }
        setValue(builderState.style(), WTFMove(image));
    }

    static void applyValue(BuilderState& builderState, CSSValue& value)
    {
        T image(getValue(builderState.style()));
        switch (modifier) {
        case Outset:
            image.setOutset(toStyleFromCSSValue<typename T::Outset>(builderState, value));
            break;
        case Repeat:
            image.setRepeat(toStyleFromCSSValue<typename T::Repeat>(builderState, value));
            break;
        case Slice:
            image.setSlice(toStyleFromCSSValue<typename T::Slice>(builderState, value));
            break;
        case Width:
            image.setWidth(toStyleFromCSSValue<typename T::Width>(builderState, value));
            break;
        }
        setValue(builderState.style(), WTFMove(image));
    }

private:
    static T initialValue()
    {
        if constexpr (std::same_as<T, BorderImage>)
            return RenderStyle::initialBorderImage();
        else if constexpr (std::same_as<T, MaskBorder>)
            return RenderStyle::initialMaskBorder();
    }

    static const T& getValue(const RenderStyle& style)
    {
        if constexpr (std::same_as<T, BorderImage>)
            return style.borderImage();
        else if constexpr (std::same_as<T, MaskBorder>)
            return style.maskBorder();
    }

    static void setValue(RenderStyle& style, T&& value)
    {
        if constexpr (std::same_as<T, BorderImage>)
            style.setBorderImage(WTFMove(value));
        else if constexpr (std::same_as<T, MaskBorder>)
            style.setMaskBorder(WTFMove(value));
    }
};

#define DEFINE_BORDER_IMAGE_MODIFIER_HANDLER(type, modifier) \
inline void BuilderCustom::applyInherit##type##modifier(BuilderState& builderState) \
{ \
    ApplyPropertyBorderImageModifier<type, modifier>::applyInheritValue(builderState); \
} \
inline void BuilderCustom::applyInitial##type##modifier(BuilderState& builderState) \
{ \
    ApplyPropertyBorderImageModifier<type, modifier>::applyInitialValue(builderState); \
} \
inline void BuilderCustom::applyValue##type##modifier(BuilderState& builderState, CSSValue& value) \
{ \
    ApplyPropertyBorderImageModifier<type, modifier>::applyValue(builderState, value); \
}

DEFINE_BORDER_IMAGE_MODIFIER_HANDLER(BorderImage, Outset)
DEFINE_BORDER_IMAGE_MODIFIER_HANDLER(BorderImage, Repeat)
DEFINE_BORDER_IMAGE_MODIFIER_HANDLER(BorderImage, Slice)
DEFINE_BORDER_IMAGE_MODIFIER_HANDLER(BorderImage, Width)
DEFINE_BORDER_IMAGE_MODIFIER_HANDLER(MaskBorder, Outset)
DEFINE_BORDER_IMAGE_MODIFIER_HANDLER(MaskBorder, Repeat)
DEFINE_BORDER_IMAGE_MODIFIER_HANDLER(MaskBorder, Slice)
DEFINE_BORDER_IMAGE_MODIFIER_HANDLER(MaskBorder, Width)

void maybeUpdateFontForLetterSpacingOrWordSpacing(BuilderState& builderState, CSSValue& value)
{
    // This is unfortunate. It's related to https://github.com/w3c/csswg-drafts/issues/5498.
    //
    // From StyleBuilder's point of view, there's a dependency cycle:
    // letter-spacing accepts an arbitrary <length>, which must be resolved against a font, which must
    // be selected after all the properties that affect font selection are processed, but letter-spacing
    // itself affects font selection because it can disable font features. StyleBuilder has some (valid)
    // ASSERT()s which would fire because of this cycle.
    //
    // There isn't *actually* a dependency cycle, though, as none of the font-relative units are
    // actually sensitive to font features (luckily). The problem is that our StyleBuilder is only
    // smart enough to consider fonts as one indivisible thing, rather than having the deeper
    // understanding that different parts of fonts may or may not depend on each other.
    //
    // So, we update the font early here, so that if there is a font-relative unit inside the CSSValue,
    // its font is updated and ready to go. In the worst case there might be a second call to
    // updateFont() later, but that isn't bad for perf because 1. It only happens twice if there is
    // actually a font-relative unit passed to letter-spacing or word-spacing, and 2. updateFont() internally
    // has logic to only do work if the font is actually dirty.

    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        if (primitiveValue->isFontRelativeLength() || primitiveValue->isCalculated())
            builderState.updateFont();
    }
}

inline void BuilderCustom::applyInheritWordSpacing(BuilderState& builderState)
{
    builderState.style().setWordSpacing(forwardInheritedValue(builderState.parentStyle().computedWordSpacing()));
    builderState.setFontDirty();
}

inline void BuilderCustom::applyInitialWordSpacing(BuilderState& builderState)
{
    builderState.style().setWordSpacing(RenderStyle::initialWordSpacing());
    builderState.setFontDirty();
}

void BuilderCustom::applyValueWordSpacing(BuilderState& builderState, CSSValue& value)
{
    maybeUpdateFontForLetterSpacingOrWordSpacing(builderState, value);
    builderState.style().setWordSpacing(toStyleFromCSSValue<WordSpacing>(builderState, value));
    builderState.setFontDirty();
}

inline void BuilderCustom::applyInheritLetterSpacing(BuilderState& builderState)
{
    builderState.style().setLetterSpacing(forwardInheritedValue(builderState.parentStyle().computedLetterSpacing()));
    builderState.setFontDirty();
}

inline void BuilderCustom::applyInitialLetterSpacing(BuilderState& builderState)
{
    builderState.style().setLetterSpacing(RenderStyle::initialLetterSpacing());
    builderState.setFontDirty();
}

inline void BuilderCustom::applyValueLetterSpacing(BuilderState& builderState, CSSValue& value)
{
    maybeUpdateFontForLetterSpacingOrWordSpacing(builderState, value);
    builderState.style().setLetterSpacing(toStyleFromCSSValue<LetterSpacing>(builderState, value));
    builderState.setFontDirty();
}

#if ENABLE(TEXT_AUTOSIZING)

inline void BuilderCustom::applyInheritLineHeight(BuilderState& builderState)
{
    builderState.style().setLineHeight(forwardInheritedValue(builderState.parentStyle().lineHeight()));
    builderState.style().setSpecifiedLineHeight(forwardInheritedValue(builderState.parentStyle().specifiedLineHeight()));
}

inline void BuilderCustom::applyInitialLineHeight(BuilderState& builderState)
{
    builderState.style().setLineHeight(RenderStyle::initialLineHeight());
    builderState.style().setSpecifiedLineHeight(RenderStyle::initialSpecifiedLineHeight());
}

static inline float computeBaseSpecifiedFontSize(const Document& document, const RenderStyle& style, bool percentageAutosizingEnabled)
{
    float result = style.specifiedFontSize();
    auto* frame = document.frame();
    if (frame && style.textZoom() != TextZoom::Reset)
        result *= frame->textZoomFactor();
    result *= style.usedZoom();
    if (percentageAutosizingEnabled
        && (!document.settings().textAutosizingUsesIdempotentMode() || document.settings().idempotentModeAutosizingOnlyHonorsPercentages()))
        result *= style.textSizeAdjust().multiplier();
    return result;
}

static inline float computeLineHeightMultiplierDueToFontSize(const Document& document, const RenderStyle& style, const CSSPrimitiveValue& value)
{
    bool percentageAutosizingEnabled = document.settings().textAutosizingEnabled() && style.textSizeAdjust().isPercentage();

    if (value.isLength()) {
        auto minimumFontSize = document.settings().minimumFontSize();
        if (minimumFontSize > 0) {
            auto specifiedFontSize = computeBaseSpecifiedFontSize(document, style, percentageAutosizingEnabled);
            // Small font sizes cause a preposterously large (near infinity) line-height. Add a fuzz-factor of 1px which opts out of
            // boosted line-height.
            if (specifiedFontSize < minimumFontSize && specifiedFontSize >= 1) {
                // FIXME: There are two settings which are relevant here: minimum font size, and minimum logical font size (as
                // well as things like the zoom property, text zoom on the page, and text autosizing). The minimum logical font
                // size is nonzero by default, and already incorporated into the computed font size, so if we just use the ratio
                // of the computed : specified font size, it will be > 1 in the cases where the minimum logical font size kicks
                // in. In general, this is the right thing to do, however, this kind of blanket change is too risky to perform
                // right now. https://bugs.webkit.org/show_bug.cgi?id=174570 tracks turning this on. For now, we can just pretend
                // that the minimum font size is the only thing affecting the computed font size.

                // This calculation matches the line-height computed size calculation in
                // TextAutoSizing::Value::adjustTextNodeSizes().
                auto scaleChange = minimumFontSize / specifiedFontSize;
                return scaleChange;
            }
        }
    }

    if (percentageAutosizingEnabled && !document.settings().textAutosizingUsesIdempotentMode())
        return style.textSizeAdjust().multiplier();
    return 1;
}

inline void BuilderCustom::applyValueLineHeight(BuilderState& builderState, CSSValue& value)
{
    if (CSSPropertyParserHelpers::isSystemFontShorthand(value.valueID())) {
        applyInitialLineHeight(builderState);
        return;
    }

    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return;

    auto lineHeight = toStyleFromCSSValue<LineHeight>(builderState, *primitiveValue, 1.0f);

    auto computedLineHeight = [&] -> LineHeight {
        if (lineHeight.isNormal())
            return lineHeight;

        auto multiplier = computeLineHeightMultiplierDueToFontSize(builderState.document(), builderState.style(), *primitiveValue);
        if (multiplier == 1)
            return lineHeight;

        return toStyleFromCSSValue<LineHeight>(builderState, *primitiveValue, multiplier);
    }();

    builderState.style().setLineHeight(WTFMove(computedLineHeight));
    builderState.style().setSpecifiedLineHeight(WTFMove(lineHeight));
}

#endif

inline void BuilderCustom::applyInitialCaretColor(BuilderState& builderState)
{
    if (builderState.applyPropertyToRegularStyle())
        builderState.style().setHasAutoCaretColor();
    if (builderState.applyPropertyToVisitedLinkStyle())
        builderState.style().setHasVisitedLinkAutoCaretColor();
}

inline void BuilderCustom::applyInheritCaretColor(BuilderState& builderState)
{
    auto& color = builderState.parentStyle().caretColor();
    if (builderState.applyPropertyToRegularStyle()) {
        if (builderState.parentStyle().hasAutoCaretColor())
            builderState.style().setHasAutoCaretColor();
        else
            builderState.style().setCaretColor(forwardInheritedValue(color));
    }
    if (builderState.applyPropertyToVisitedLinkStyle()) {
        if (builderState.parentStyle().hasVisitedLinkAutoCaretColor())
            builderState.style().setHasVisitedLinkAutoCaretColor();
        else
            builderState.style().setVisitedLinkCaretColor(forwardInheritedValue(color));
    }
}

inline void BuilderCustom::applyValueCaretColor(BuilderState& builderState, CSSValue& value)
{
    if (builderState.applyPropertyToRegularStyle()) {
        if (value.valueID() == CSSValueAuto)
            builderState.style().setHasAutoCaretColor();
        else
            builderState.style().setCaretColor(toStyleFromCSSValue<Color>(builderState, value, ForVisitedLink::No));
    }
    if (builderState.applyPropertyToVisitedLinkStyle()) {
        if (value.valueID() == CSSValueAuto)
            builderState.style().setHasVisitedLinkAutoCaretColor();
        else
            builderState.style().setVisitedLinkCaretColor(toStyleFromCSSValue<Color>(builderState, value, ForVisitedLink::Yes));
    }
}

inline void BuilderCustom::applyValueWebkitLocale(BuilderState& builderState, CSSValue& value)
{
    auto primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return;

    if (primitiveValue->valueID() == CSSValueAuto)
        builderState.setFontDescriptionSpecifiedLocale(nullAtom());
    else
        builderState.setFontDescriptionSpecifiedLocale(AtomString { primitiveValue->stringValue() });
}

inline void BuilderCustom::applyValueWritingMode(BuilderState& builderState, CSSValue& value)
{
    builderState.setWritingMode(fromCSSValue<StyleWritingMode>(value));
    builderState.style().setHasExplicitlySetWritingMode(true);
}

inline void BuilderCustom::applyValueTextOrientation(BuilderState& builderState, CSSValue& value)
{
    builderState.setTextOrientation(fromCSSValue<TextOrientation>(value));
}

#if ENABLE(TEXT_AUTOSIZING)
inline void BuilderCustom::applyValueWebkitTextSizeAdjust(BuilderState& builderState, CSSValue& value)
{
    builderState.style().setTextSizeAdjust(toStyleFromCSSValue<TextSizeAdjust>(builderState, value));
    builderState.setFontDirty();
}
#endif

inline void BuilderCustom::applyValueWebkitTextZoom(BuilderState& builderState, CSSValue& value)
{
    auto primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return;

    if (primitiveValue->valueID() == CSSValueNormal)
        builderState.style().setTextZoom(TextZoom::Normal);
    else if (primitiveValue->valueID() == CSSValueReset)
        builderState.style().setTextZoom(TextZoom::Reset);
    builderState.setFontDirty();
}

#if ENABLE(DARK_MODE_CSS)
inline void BuilderCustom::applyValueColorScheme(BuilderState& builderState, CSSValue& value)
{
    builderState.style().setColorScheme(toStyleFromCSSValue<ColorScheme>(builderState, value));
    builderState.style().setHasExplicitlySetColorScheme(true);
}
#endif

inline void BuilderCustom::applyInitialFontFamily(BuilderState& builderState)
{
    auto& fontDescription = builderState.fontDescription();
    auto initialDesc = FontCascadeDescription();

    // We need to adjust the size to account for the generic family change from monospace to non-monospace.
    if (fontDescription.useFixedDefaultSize()) {
        if (CSSValueID sizeIdentifier = fontDescription.keywordSizeAsIdentifier())
            builderState.setFontDescriptionFontSize(Style::fontSizeForKeyword(sizeIdentifier, false, builderState.document()));
    }

    if (!initialDesc.firstFamily().isEmpty())
        builderState.setFontDescriptionFamilies(FontFamilies { initialDesc.families(), fontDescription.isSpecifiedFont() });
}

inline void BuilderCustom::applyInheritFontFamily(BuilderState& builderState)
{
    builderState.setFontDescriptionFamilies(forwardInheritedValue(builderState.parentStyle().fontFamily()));
}

inline void BuilderCustom::applyValueFontFamily(BuilderState& builderState, CSSValue& value)
{
    auto& fontDescription = builderState.fontDescription();

    // Before mapping in a new font-family property, we should reset the generic family.
    bool oldFamilyUsedFixedDefaultSize = fontDescription.useFixedDefaultSize();

    builderState.setFontDescriptionFamilies(toStyleFromCSSValue<FontFamilies>(builderState, value));

    if (fontDescription.useFixedDefaultSize() != oldFamilyUsedFixedDefaultSize) {
        if (CSSValueID sizeIdentifier = fontDescription.keywordSizeAsIdentifier())
            builderState.setFontDescriptionFontSize(Style::fontSizeForKeyword(sizeIdentifier, !oldFamilyUsedFixedDefaultSize, builderState.document()));
    }
}

inline void BuilderCustom::applyInitialBorderTopWidth(BuilderState& builderState)
{
    if (evaluationTimeZoomEnabled(builderState))
        builderState.style().setBorderTopWidth(RenderStyle::initialBorderWidth());
    else
        builderState.style().setBorderTopWidth(Style::LineWidth { RenderStyle::initialBorderWidth().value.unresolvedValue() * builderState.style().usedZoom() });
}

inline void BuilderCustom::applyInitialBorderRightWidth(BuilderState& builderState)
{
    if (evaluationTimeZoomEnabled(builderState))
        builderState.style().setBorderRightWidth(RenderStyle::initialBorderWidth());
    else
        builderState.style().setBorderRightWidth(Style::LineWidth { RenderStyle::initialBorderWidth().value.unresolvedValue() * builderState.style().usedZoom() });
}

inline void BuilderCustom::applyInitialBorderBottomWidth(BuilderState& builderState)
{
    if (evaluationTimeZoomEnabled(builderState))
        builderState.style().setBorderBottomWidth(RenderStyle::initialBorderWidth());
    else
        builderState.style().setBorderBottomWidth(Style::LineWidth { RenderStyle::initialBorderWidth().value.unresolvedValue() * builderState.style().usedZoom() });
}

inline void BuilderCustom::applyInitialBorderLeftWidth(BuilderState& builderState)
{
    if (evaluationTimeZoomEnabled(builderState))
        builderState.style().setBorderLeftWidth(RenderStyle::initialBorderWidth());
    else
        builderState.style().setBorderLeftWidth(Style::LineWidth { RenderStyle::initialBorderWidth().value.unresolvedValue() * builderState.style().usedZoom() });
}

inline void BuilderCustom::applyInitialOutlineWidth(BuilderState& builderState)
{
    if (evaluationTimeZoomEnabled(builderState))
        builderState.style().setOutlineWidth(RenderStyle::initialOutlineWidth());
    else
        builderState.style().setOutlineWidth(Style::LineWidth { RenderStyle::initialOutlineWidth().value.unresolvedValue() * builderState.style().usedZoom() });
}

inline void BuilderCustom::applyInitialColumnRuleWidth(BuilderState& builderState)
{
    if (evaluationTimeZoomEnabled(builderState))
        builderState.style().setColumnRuleWidth(RenderStyle::initialColumnRuleWidth());
    else
        builderState.style().setColumnRuleWidth(Style::LineWidth { RenderStyle::initialColumnRuleWidth().value.unresolvedValue() * builderState.style().usedZoom() });
}

inline void BuilderCustom::applyInitialBorderBottomLeftRadius(BuilderState& builderState)
{
    builderState.style().setBorderBottomLeftRadius(RenderStyle::initialBorderRadius());
    builderState.style().setHasExplicitlySetBorderBottomLeftRadius(false);
}

inline void BuilderCustom::applyInheritBorderBottomLeftRadius(BuilderState& builderState)
{
    builderState.style().setBorderBottomLeftRadius(forwardInheritedValue(builderState.parentStyle().borderBottomLeftRadius()));
    builderState.style().setHasExplicitlySetBorderBottomLeftRadius(builderState.parentStyle().hasExplicitlySetBorderBottomLeftRadius());
}

inline void BuilderCustom::applyValueBorderBottomLeftRadius(BuilderState& builderState, CSSValue& value)
{
    builderState.style().setBorderBottomLeftRadius(toStyleFromCSSValue<BorderRadiusValue>(builderState, value));
    builderState.style().setHasExplicitlySetBorderBottomLeftRadius(true);
}

inline void BuilderCustom::applyInitialBorderBottomRightRadius(BuilderState& builderState)
{
    builderState.style().setBorderBottomRightRadius(RenderStyle::initialBorderRadius());
    builderState.style().setHasExplicitlySetBorderBottomRightRadius(false);
}

inline void BuilderCustom::applyInheritBorderBottomRightRadius(BuilderState& builderState)
{
    builderState.style().setBorderBottomRightRadius(forwardInheritedValue(builderState.parentStyle().borderBottomRightRadius()));
    builderState.style().setHasExplicitlySetBorderBottomRightRadius(builderState.parentStyle().hasExplicitlySetBorderBottomRightRadius());
}

inline void BuilderCustom::applyValueBorderBottomRightRadius(BuilderState& builderState, CSSValue& value)
{
    builderState.style().setBorderBottomRightRadius(toStyleFromCSSValue<BorderRadiusValue>(builderState, value));
    builderState.style().setHasExplicitlySetBorderBottomRightRadius(true);
}

inline void BuilderCustom::applyInitialBorderTopLeftRadius(BuilderState& builderState)
{
    builderState.style().setBorderTopLeftRadius(RenderStyle::initialBorderRadius());
    builderState.style().setHasExplicitlySetBorderTopLeftRadius(false);
}

inline void BuilderCustom::applyInheritBorderTopLeftRadius(BuilderState& builderState)
{
    builderState.style().setBorderTopLeftRadius(forwardInheritedValue(builderState.parentStyle().borderTopLeftRadius()));
    builderState.style().setHasExplicitlySetBorderTopLeftRadius(builderState.parentStyle().hasExplicitlySetBorderTopLeftRadius());
}

inline void BuilderCustom::applyValueBorderTopLeftRadius(BuilderState& builderState, CSSValue& value)
{
    builderState.style().setBorderTopLeftRadius(toStyleFromCSSValue<BorderRadiusValue>(builderState, value));
    builderState.style().setHasExplicitlySetBorderTopLeftRadius(true);
}

inline void BuilderCustom::applyInitialBorderTopRightRadius(BuilderState& builderState)
{
    builderState.style().setBorderTopRightRadius(RenderStyle::initialBorderRadius());
    builderState.style().setHasExplicitlySetBorderTopRightRadius(false);
}

inline void BuilderCustom::applyInheritBorderTopRightRadius(BuilderState& builderState)
{
    builderState.style().setBorderTopRightRadius(forwardInheritedValue(builderState.parentStyle().borderTopRightRadius()));
    builderState.style().setHasExplicitlySetBorderTopRightRadius(builderState.parentStyle().hasExplicitlySetBorderTopRightRadius());
}

inline void BuilderCustom::applyValueBorderTopRightRadius(BuilderState& builderState, CSSValue& value)
{
    builderState.style().setBorderTopRightRadius(toStyleFromCSSValue<BorderRadiusValue>(builderState, value));
    builderState.style().setHasExplicitlySetBorderTopRightRadius(true);
}

template<BuilderCustom::CounterBehavior counterBehavior>
inline void BuilderCustom::applyInheritCounter(BuilderState& builderState)
{
    auto& map = builderState.style().accessCounterDirectives().map;
    for (auto& keyValue : builderState.parentStyle().counterDirectives().map) {
        auto& directives = map.add(keyValue.key, CounterDirectives { }).iterator->value;
        if (counterBehavior == Reset)
            directives.resetValue = keyValue.value.resetValue;
        else if (counterBehavior == Increment)
            directives.incrementValue = keyValue.value.incrementValue;
        else
            directives.setValue = keyValue.value.setValue;
    }
}

template<BuilderCustom::CounterBehavior counterBehavior>
inline void BuilderCustom::applyValueCounter(BuilderState& builderState, CSSValue& value)
{
    bool setCounterIncrementToNone = counterBehavior == Increment && value.valueID() == CSSValueNone;

    if (!is<CSSValueList>(value) && !setCounterIncrementToNone)
        return;

    auto& map = builderState.style().accessCounterDirectives().map;
    for (auto& keyValue : map) {
        if (counterBehavior == Reset)
            keyValue.value.resetValue = std::nullopt;
        else if (counterBehavior == Increment)
            keyValue.value.incrementValue = std::nullopt;
        else
            keyValue.value.setValue = std::nullopt;
    }

    if (setCounterIncrementToNone)
        return;

    auto& conversionData = builderState.cssToLengthConversionData();

    auto list = requiredListDowncast<CSSValueList, CSSValuePair>(builderState, value);
    if (!list)
        return;

    for (auto& pairValue : *list) {
        auto pair = requiredPairDowncast<CSSPrimitiveValue>(builderState, pairValue);
        if (!pair)
            return;
        AtomString identifier { pair->first->stringValue() };
        int value =  pair->second->resolveAsNumber<int>(conversionData);
        auto& directives = map.add(identifier, CounterDirectives { }).iterator->value;
        if (counterBehavior == Reset)
            directives.resetValue = value;
        else if (counterBehavior == Increment)
            directives.incrementValue = saturatedSum(directives.incrementValue.value_or(0), value);
        else
            directives.setValue = value;
    }
}

inline void BuilderCustom::applyInitialCounterIncrement(BuilderState&)
{
}

inline void BuilderCustom::applyInheritCounterIncrement(BuilderState& builderState)
{
    applyInheritCounter<Increment>(builderState);
}

inline void BuilderCustom::applyValueCounterIncrement(BuilderState& builderState, CSSValue& value)
{
    applyValueCounter<Increment>(builderState, value);
}

inline void BuilderCustom::applyInitialCounterReset(BuilderState&)
{
}

inline void BuilderCustom::applyInheritCounterReset(BuilderState& builderState)
{
    applyInheritCounter<Reset>(builderState);
}

inline void BuilderCustom::applyValueCounterReset(BuilderState& builderState, CSSValue& value)
{
    applyValueCounter<Reset>(builderState, value);
}

inline void BuilderCustom::applyInitialCounterSet(BuilderState&)
{
}

inline void BuilderCustom::applyInheritCounterSet(BuilderState& builderState)
{
    applyInheritCounter<Set>(builderState);
}

inline void BuilderCustom::applyValueCounterSet(BuilderState& builderState, CSSValue& value)
{
    applyValueCounter<Set>(builderState, value);
}

inline void BuilderCustom::applyInitialFill(BuilderState& builderState)
{
    auto& style = builderState.style();
    if (builderState.applyPropertyToRegularStyle())
        style.setFill(RenderStyle::initialFill());
    if (builderState.applyPropertyToVisitedLinkStyle())
        style.setVisitedLinkFill(RenderStyle::initialFill());
}

inline void BuilderCustom::applyInheritFill(BuilderState& builderState)
{
    auto& style = builderState.style();
    auto& parentStyle = builderState.parentStyle();

    if (builderState.applyPropertyToRegularStyle())
        style.setFill(forwardInheritedValue(parentStyle.fill()));
    if (builderState.applyPropertyToVisitedLinkStyle())
        style.setVisitedLinkFill(forwardInheritedValue(parentStyle.fill()));
}

inline void BuilderCustom::applyValueFill(BuilderState& builderState, CSSValue& value)
{
    auto& style = builderState.style();
    if (builderState.applyPropertyToRegularStyle())
        style.setFill(toStyleFromCSSValue<SVGPaint>(builderState, value, ForVisitedLink::No));
    if (builderState.applyPropertyToVisitedLinkStyle())
        style.setVisitedLinkFill(toStyleFromCSSValue<SVGPaint>(builderState, value, ForVisitedLink::Yes));
}

inline void BuilderCustom::applyInitialStroke(BuilderState& builderState)
{
    auto& style = builderState.style();
    if (builderState.applyPropertyToRegularStyle())
        style.setStroke(RenderStyle::initialStroke());
    if (builderState.applyPropertyToVisitedLinkStyle())
        style.setVisitedLinkStroke(RenderStyle::initialStroke());
}

inline void BuilderCustom::applyInheritStroke(BuilderState& builderState)
{
    auto& style = builderState.style();
    auto& parentStyle = builderState.parentStyle();

    if (builderState.applyPropertyToRegularStyle())
        style.setStroke(forwardInheritedValue(parentStyle.stroke()));
    if (builderState.applyPropertyToVisitedLinkStyle())
        style.setVisitedLinkStroke(forwardInheritedValue(parentStyle.stroke()));
}

inline void BuilderCustom::applyValueStroke(BuilderState& builderState, CSSValue& value)
{
    auto& style = builderState.style();
    if (builderState.applyPropertyToRegularStyle())
        style.setStroke(toStyleFromCSSValue<SVGPaint>(builderState, value, ForVisitedLink::No));
    if (builderState.applyPropertyToVisitedLinkStyle())
        style.setVisitedLinkStroke(toStyleFromCSSValue<SVGPaint>(builderState, value, ForVisitedLink::Yes));
}

inline void BuilderCustom::applyInitialFontSize(BuilderState& builderState)
{
    auto fontDescription = builderState.fontDescription();
    float size = Style::fontSizeForKeyword(CSSValueMedium, fontDescription.useFixedDefaultSize(), builderState.document());

    if (size < 0)
        return;

    fontDescription.setKeywordSizeFromIdentifier(CSSValueMedium);
    builderState.setFontSize(fontDescription, size);
    builderState.setFontDescription(WTFMove(fontDescription));
}

inline void BuilderCustom::applyInheritFontSize(BuilderState& builderState)
{
    const auto& parentFontDescription = builderState.parentStyle().fontDescription();
    float size = parentFontDescription.specifiedSize();

    if (size < 0)
        return;

    builderState.setFontDescriptionKeywordSize(parentFontDescription.keywordSize());
    builderState.setFontDescriptionFontSize(size);
}

// When the CSS keyword "larger" is used, this function will attempt to match within the keyword
// table, and failing that, will simply multiply by 1.2.
inline float BuilderCustom::largerFontSize(float size)
{
    // FIXME: Figure out where we fall in the size ranges (xx-small to xxx-large) and scale up to
    // the next size level.
    return size * 1.2f;
}

// Like the previous function, but for the keyword "smaller".
inline float BuilderCustom::smallerFontSize(float size)
{
    // FIXME: Figure out where we fall in the size ranges (xx-small to xxx-large) and scale down to
    // the next size level.
    return size / 1.2f;
}

inline float BuilderCustom::determineRubyTextSizeMultiplier(BuilderState& builderState)
{
    if (!builderState.style().isInterCharacterRubyPosition())
        return 0.5f;

    auto rubyPosition = builderState.style().rubyPosition();
    if (rubyPosition == RubyPosition::InterCharacter) {
        // If the writing mode of the enclosing ruby container is vertical, 'inter-character' value has the same effect as over.
        return !builderState.parentStyle().writingMode().isVerticalTypographic() ? 0.3f : 0.5f;
    }

    // Legacy inter-character behavior.
    // FIXME: This hack is to ensure tone marks are the same size as
    // the bopomofo. This code will go away if we make a special renderer
    // for the tone marks eventually.
    if (auto* element = builderState.element()) {
        for (auto& ancestor : ancestorsOfType<HTMLElement>(*element)) {
            if (ancestor.hasTagName(HTMLNames::rtTag))
                return 1.0f;
        }
    }
    return 0.25f;
}

inline void BuilderCustom::applyValueFontSize(BuilderState& builderState, CSSValue& value)
{
    auto& fontDescription = builderState.fontDescription();
    builderState.setFontDescriptionKeywordSizeFromIdentifier(CSSValueInvalid);

    float parentSize = builderState.parentStyle().fontDescription().specifiedSize();
    bool parentIsAbsoluteSize = builderState.parentStyle().fontDescription().isAbsoluteSize();

    auto primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return;

    float size = 0;
    if (CSSValueID ident = primitiveValue->valueID()) {
        builderState.setFontDescriptionIsAbsoluteSize((parentIsAbsoluteSize && (ident == CSSValueLarger || ident == CSSValueSmaller || ident == CSSValueWebkitRubyText)) || CSSPropertyParserHelpers::isSystemFontShorthand(ident));

        if (CSSPropertyParserHelpers::isSystemFontShorthand(ident))
            size = SystemFontDatabase::singleton().systemFontShorthandSize(CSSPropertyParserHelpers::lowerFontShorthand(ident));

        switch (ident) {
        case CSSValueXxSmall:
        case CSSValueXSmall:
        case CSSValueSmall:
        case CSSValueMedium:
        case CSSValueLarge:
        case CSSValueXLarge:
        case CSSValueXxLarge:
        case CSSValueXxxLarge:
            size = Style::fontSizeForKeyword(ident, fontDescription.useFixedDefaultSize(), builderState.document());
            builderState.setFontDescriptionKeywordSizeFromIdentifier(ident);
            break;
        case CSSValueLarger:
            size = largerFontSize(parentSize);
            break;
        case CSSValueSmaller:
            size = smallerFontSize(parentSize);
            break;
        case CSSValueWebkitRubyText:
            size = determineRubyTextSizeMultiplier(builderState) * parentSize;
            break;
        default:
            break;
        }
    } else {
        builderState.setFontDescriptionIsAbsoluteSize(parentIsAbsoluteSize || !primitiveValue->isParentFontRelativeLength());
        auto conversionData = builderState.cssToLengthConversionData().copyForFontSize();
        if (primitiveValue->isLength())
            size = primitiveValue->resolveAsLength<float>(conversionData);
        else if (primitiveValue->isPercentage())
            size = (primitiveValue->resolveAsPercentage<float>(conversionData) * parentSize) / 100.0f;
        else if (primitiveValue->isCalculatedPercentageWithLength())
            size = primitiveValue->cssCalcValue()->createCalculationValue(conversionData, CSSCalcSymbolTable { })->evaluate(parentSize, Style::ZoomNeeded { });
        else
            return;
    }

    if (size < 0)
        return;

    builderState.setFontDescriptionFontSize(std::min(maximumAllowedFontSize, size));
}

inline void BuilderCustom::applyValueStrokeWidth(BuilderState& builderState, CSSValue& value)
{
    builderState.style().setStrokeWidth(toStyleFromCSSValue<StrokeWidth>(builderState, value));
    builderState.style().setHasExplicitlySetStrokeWidth(true);
}

inline void BuilderCustom::applyValueStrokeColor(BuilderState& builderState, CSSValue& value)
{
    if (builderState.applyPropertyToRegularStyle())
        builderState.style().setStrokeColor(toStyleFromCSSValue<Color>(builderState, value, ForVisitedLink::No));
    if (builderState.applyPropertyToVisitedLinkStyle())
        builderState.style().setVisitedLinkStrokeColor(toStyleFromCSSValue<Color>(builderState, value, ForVisitedLink::Yes));
    builderState.style().setHasExplicitlySetStrokeColor(true);
}

// For the color property, "currentcolor" is actually the inherited computed color.
inline void BuilderCustom::applyValueColor(BuilderState& builderState, CSSValue& value)
{
    if (builderState.applyPropertyToRegularStyle()) {
        auto color = toStyleFromCSSValue<Color>(builderState, value, ForVisitedLink::No);
        builderState.style().setColor(color.resolveColor(builderState.parentStyle().color()));
    }
    if (builderState.applyPropertyToVisitedLinkStyle()) {
        auto color = toStyleFromCSSValue<Color>(builderState, value, ForVisitedLink::Yes);
        builderState.style().setVisitedLinkColor(color.resolveColor(builderState.parentStyle().visitedLinkColor()));
    }

    builderState.style().setDisallowsFastPathInheritance();
    builderState.style().setHasExplicitlySetColor(builderState.isAuthorOrigin());
}

inline void BuilderCustom::applyInitialColor(BuilderState& builderState)
{
    if (builderState.applyPropertyToRegularStyle())
        builderState.style().setColor(RenderStyle::initialColor());
    if (builderState.applyPropertyToVisitedLinkStyle())
        builderState.style().setVisitedLinkColor(RenderStyle::initialColor());

    builderState.style().setDisallowsFastPathInheritance();
    builderState.style().setHasExplicitlySetColor(builderState.isAuthorOrigin());
}

inline void BuilderCustom::applyInheritColor(BuilderState& builderState)
{
    if (builderState.applyPropertyToRegularStyle())
        builderState.style().setColor(forwardInheritedValue(builderState.parentStyle().color()));
    if (builderState.applyPropertyToVisitedLinkStyle())
        builderState.style().setVisitedLinkColor(forwardInheritedValue(builderState.parentStyle().color()));

    builderState.style().setDisallowsFastPathInheritance();
    builderState.style().setHasExplicitlySetColor(builderState.isAuthorOrigin());
}

inline void BuilderCustom::applyInitialPaddingBottom(BuilderState& builderState)
{
    builderState.style().setPaddingBottom(RenderStyle::initialPadding());
    builderState.style().setHasExplicitlySetPaddingBottom(builderState.isAuthorOrigin());
}

inline void BuilderCustom::applyInheritPaddingBottom(BuilderState& builderState)
{
    builderState.style().setPaddingBottom(forwardInheritedValue(builderState.parentStyle().paddingBottom()));
    builderState.style().setHasExplicitlySetPaddingBottom(builderState.isAuthorOrigin());
}

inline void BuilderCustom::applyValuePaddingBottom(BuilderState& builderState, CSSValue& value)
{
    builderState.style().setPaddingBottom(toStyleFromCSSValue<PaddingEdge>(builderState, value));
    builderState.style().setHasExplicitlySetPaddingBottom(builderState.isAuthorOrigin());
}

inline void BuilderCustom::applyInitialPaddingLeft(BuilderState& builderState)
{
    builderState.style().setPaddingLeft(RenderStyle::initialPadding());
    builderState.style().setHasExplicitlySetPaddingLeft(builderState.isAuthorOrigin());
}

inline void BuilderCustom::applyInheritPaddingLeft(BuilderState& builderState)
{
    builderState.style().setPaddingLeft(forwardInheritedValue(builderState.parentStyle().paddingLeft()));
    builderState.style().setHasExplicitlySetPaddingLeft(builderState.isAuthorOrigin());
}

inline void BuilderCustom::applyValuePaddingLeft(BuilderState& builderState, CSSValue& value)
{
    builderState.style().setPaddingLeft(toStyleFromCSSValue<PaddingEdge>(builderState, value));
    builderState.style().setHasExplicitlySetPaddingLeft(builderState.isAuthorOrigin());
}

inline void BuilderCustom::applyInitialPaddingRight(BuilderState& builderState)
{
    builderState.style().setPaddingRight(RenderStyle::initialPadding());
    builderState.style().setHasExplicitlySetPaddingRight(builderState.isAuthorOrigin());
}

inline void BuilderCustom::applyInheritPaddingRight(BuilderState& builderState)
{
    builderState.style().setPaddingRight(forwardInheritedValue(builderState.parentStyle().paddingRight()));
    builderState.style().setHasExplicitlySetPaddingRight(builderState.isAuthorOrigin());
}

inline void BuilderCustom::applyValuePaddingRight(BuilderState& builderState, CSSValue& value)
{
    builderState.style().setPaddingRight(toStyleFromCSSValue<PaddingEdge>(builderState, value));
    builderState.style().setHasExplicitlySetPaddingRight(builderState.isAuthorOrigin());
}

inline void BuilderCustom::applyInitialPaddingTop(BuilderState& builderState)
{
    builderState.style().setPaddingTop(RenderStyle::initialPadding());
    builderState.style().setHasExplicitlySetPaddingTop(builderState.isAuthorOrigin());
}

inline void BuilderCustom::applyInheritPaddingTop(BuilderState& builderState)
{
    builderState.style().setPaddingTop(forwardInheritedValue(builderState.parentStyle().paddingTop()));
    builderState.style().setHasExplicitlySetPaddingTop(builderState.isAuthorOrigin());
}

inline void BuilderCustom::applyValuePaddingTop(BuilderState& builderState, CSSValue& value)
{
    builderState.style().setPaddingTop(toStyleFromCSSValue<PaddingEdge>(builderState, value));
    builderState.style().setHasExplicitlySetPaddingTop(builderState.isAuthorOrigin());
}

} // namespace Style
} // namespace WebCore
