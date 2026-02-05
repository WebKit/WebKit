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

#include "AnchorPositionEvaluator.h"
#include "CSSCounterStyleRegistry.h"
#include "CSSCounterStyleRule.h"
#include "CSSCounterValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyParserConsumer+Font.h"
#include "CSSRegisteredCustomProperty.h"
#include "CSSValuePair.h"
#include "DocumentQuirks.h"
#include "DocumentView.h"
#include "ElementAncestorIteratorInlines.h"
#include "FontSelectionValueInlines.h"
#include "FrameDestructionObserverInlines.h"
#include "HTMLElement.h"
#include "LocalFrame.h"
#include "SVGElement.h"
#include "SVGElementTypeHelpers.h"
#include "SVGPathElement.h"
#include "Settings.h"
#include "StyleBuilderChecking.h"
#include "StyleBuilderStateInlines.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StyleComputedStyle+SettersInlines.h"
#include "StyleFontSizeFunctions.h"
#include "StyleLengthWrapper+CSSValueConversion.h"
#include "StylePrimitiveKeyword+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StyleResolveForFont.h"
#include "StyleResolver.h"
#include "StyleTextEdge+CSSValueConversion.h"
#include "StyleValueTypes+CSSValueConversion.h"
#include "TextSpacing.h"
#include "ViewTimeline.h"
#include <ranges>

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
inline BorderImageSlice forwardInheritedValue(const BorderImageSlice& value) { auto copy = value; return copy; }
inline BorderImageWidth forwardInheritedValue(const BorderImageWidth& value) { auto copy = value; return copy; }
inline BorderImageOutset forwardInheritedValue(const BorderImageOutset& value) { auto copy = value; return copy; }
inline BorderImageRepeat forwardInheritedValue(const BorderImageRepeat& value) { auto copy = value; return copy; }
inline BorderRadiusValue forwardInheritedValue(const BorderRadiusValue& value) { auto copy = value; return copy; }
inline BoxShadows forwardInheritedValue(const BoxShadows& value) { auto copy = value; return copy; }
inline CaretColor forwardInheritedValue(const CaretColor& value) { auto copy = value; return copy; }
inline ContainIntrinsicSize forwardInheritedValue(const ContainIntrinsicSize& value) { auto copy = value; return copy; }
inline ContainerNames forwardInheritedValue(const ContainerNames& value) { auto copy = value; return copy; }
inline CounterIncrement forwardInheritedValue(const CounterIncrement& value) { auto copy = value; return copy; }
inline CounterReset forwardInheritedValue(const CounterReset& value) { auto copy = value; return copy; }
inline CounterSet forwardInheritedValue(const CounterSet& value) { auto copy = value; return copy; }
inline Content forwardInheritedValue(const Content& value) { auto copy = value; return copy; }
inline WebCore::Color forwardInheritedValue(const WebCore::Color& value) { auto copy = value; return copy; }
inline Color forwardInheritedValue(const Color& value) { auto copy = value; return copy; }
inline EasingFunction forwardInheritedValue(const EasingFunction& value) { auto copy = value; return copy; }
inline GapGutter forwardInheritedValue(const GapGutter& value) { auto copy = value; return copy; }
inline FontFamilies forwardInheritedValue(const FontFamilies& value) { auto copy = value; return copy; }
inline ScrollMarginEdge forwardInheritedValue(const ScrollMarginEdge& value) { auto copy = value; return copy; }
inline ScrollPaddingEdge forwardInheritedValue(const ScrollPaddingEdge& value) { auto copy = value; return copy; }
inline LineWidth forwardInheritedValue(const LineWidth& value) { auto copy = value; return copy; }
inline MaskBorderSource forwardInheritedValue(const MaskBorderSource& value) { auto copy = value; return copy; }
inline MaskBorderSlice forwardInheritedValue(const MaskBorderSlice& value) { auto copy = value; return copy; }
inline MaskBorderWidth forwardInheritedValue(const MaskBorderWidth& value) { auto copy = value; return copy; }
inline MaskBorderOutset forwardInheritedValue(const MaskBorderOutset& value) { auto copy = value; return copy; }
inline MaskBorderRepeat forwardInheritedValue(const MaskBorderRepeat& value) { auto copy = value; return copy; }
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
inline FlowTolerance forwardInheritedValue(const FlowTolerance& value) { auto copy = value; return copy; }
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
    DECLARE_PROPERTY_CUSTOM_HANDLERS(FontFamily);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(FontSize);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(LetterSpacing);
#if ENABLE(TEXT_AUTOSIZING)
    DECLARE_PROPERTY_CUSTOM_HANDLERS(LineHeight);
#endif
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
    static void applyValueColor(BuilderState&, CSSValue&);
    static void applyValueWebkitLocale(BuilderState&, CSSValue&);
    static void applyValueTextOrientation(BuilderState&, CSSValue&);
#if ENABLE(TEXT_AUTOSIZING)
    static void applyValueWebkitTextSizeAdjust(BuilderState&, CSSValue&);
#endif
    static void applyValueWebkitTextZoom(BuilderState&, CSSValue&);
    static void applyValueWritingMode(BuilderState&, CSSValue&);
    static void applyValueFontSizeAdjust(BuilderState&, CSSValue&);

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
    static float determineMathDepthScale(BuilderState&);
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

template<auto propertyID, auto listMutableGetter, typename ItemType, typename ListType>
void applyValueCoordinatedValueListProperty(BuilderState& builderState, CSSValue& value)
{
    using PropertyAccessor = CoordinatedValueListPropertyAccessor<propertyID>;

    auto& list = (builderState.style().*listMutableGetter)();

    auto set = [&](auto i, auto& item) {
        if (item.valueID() == CSSValueInitial)
            PropertyAccessor { list[i] }.set(PropertyAccessor::initial());
        else
            PropertyAccessor { list[i] }.set(toStyleFromCSSValue<ItemType>(builderState, item));
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

inline void BuilderCustom::resetUsedZoom(BuilderState& builderState)
{
    // Reset the zoom in effect. This allows the setZoom method to accurately compute a new zoom in effect.
    builderState.setUsedZoom(builderState.parentStyle().usedZoom());
}

inline void BuilderCustom::applyInitialZoom(BuilderState& builderState)
{
    resetUsedZoom(builderState);
    builderState.setZoom(Style::ComputedStyle::initialZoom());
}

inline void BuilderCustom::applyInheritZoom(BuilderState& builderState)
{
    resetUsedZoom(builderState);
    builderState.setZoom(forwardInheritedValue(builderState.parentStyle().zoom()));
}

inline void BuilderCustom::applyValueZoom(BuilderState& builderState, CSSValue& value)
{
    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return;

    if (primitiveValue->valueID() == CSSValueNormal) {
        resetUsedZoom(builderState);
        builderState.setZoom(Style::ComputedStyle::initialZoom());
    } else {
        resetUsedZoom(builderState);

        auto zoom = toStyleFromCSSValue<Zoom>(builderState, *primitiveValue);
        // FIXME: The spec says that zoom values of 0 should be treated as 1, not ignored entirely. https://drafts.csswg.org/css-viewport/#valdef-zoom-number
        if (!isZero(zoom))
            builderState.setZoom(zoom);
    }
}

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
    builderState.style().setWordSpacing(Style::ComputedStyle::initialWordSpacing());
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
    builderState.style().setLetterSpacing(Style::ComputedStyle::initialLetterSpacing());
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
    builderState.style().setLineHeight(Style::ComputedStyle::initialLineHeight());
    builderState.style().setSpecifiedLineHeight(Style::ComputedStyle::initialSpecifiedLineHeight());
}

static inline float computeBaseSpecifiedFontSize(const Document& document, const ComputedStyle& style, bool percentageAutosizingEnabled)
{
    float result = style.specifiedFontSize();
    RefPtr frame = document.frame();
    if (frame && style.textZoom() != TextZoom::Reset)
        result *= frame->textZoomFactor();
    result *= style.usedZoom();
    if (percentageAutosizingEnabled
        && (!document.settings().textAutosizingUsesIdempotentMode() || document.settings().idempotentModeAutosizingOnlyHonorsPercentages()))
        result *= style.textSizeAdjust().multiplier();
    return result;
}

static inline float computeLineHeightMultiplierDueToFontSize(const Document& document, const ComputedStyle& style, const CSSPrimitiveValue& value)
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

    builderState.style().setLineHeight(WTF::move(computedLineHeight));
    builderState.style().setSpecifiedLineHeight(WTF::move(lineHeight));
}

#endif

inline void BuilderCustom::applyValueWebkitLocale(BuilderState& builderState, CSSValue& value)
{
    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
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
    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return;

    if (primitiveValue->valueID() == CSSValueNormal)
        builderState.style().setTextZoom(TextZoom::Normal);
    else if (primitiveValue->valueID() == CSSValueReset)
        builderState.style().setTextZoom(TextZoom::Reset);
    builderState.setFontDirty();
}

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
    builderState.style().setBorderTopWidth(Style::LineWidth::snapLengthAsBorderWidth(3.0f * builderState.style().usedZoom(), builderState.document().deviceScaleFactor()));
}

inline void BuilderCustom::applyInitialBorderRightWidth(BuilderState& builderState)
{
    builderState.style().setBorderRightWidth(Style::LineWidth::snapLengthAsBorderWidth(3.0f * builderState.style().usedZoom(), builderState.document().deviceScaleFactor()));
}

inline void BuilderCustom::applyInitialBorderBottomWidth(BuilderState& builderState)
{
    builderState.style().setBorderBottomWidth(Style::LineWidth::snapLengthAsBorderWidth(3.0f * builderState.style().usedZoom(), builderState.document().deviceScaleFactor()));
}

inline void BuilderCustom::applyInitialBorderLeftWidth(BuilderState& builderState)
{
    builderState.style().setBorderLeftWidth(Style::LineWidth::snapLengthAsBorderWidth(3.0f * builderState.style().usedZoom(), builderState.document().deviceScaleFactor()));
}

inline void BuilderCustom::applyInitialOutlineWidth(BuilderState& builderState)
{
    builderState.style().setOutlineWidth(Style::LineWidth::snapLengthAsBorderWidth(3.0f * builderState.style().usedZoom(), builderState.document().deviceScaleFactor()));
}

inline void BuilderCustom::applyInitialColumnRuleWidth(BuilderState& builderState)
{
    builderState.style().setColumnRuleWidth(Style::LineWidth::snapLengthAsBorderWidth(3.0f * builderState.style().usedZoom(), builderState.document().deviceScaleFactor()));
}

inline void BuilderCustom::applyInitialFontSize(BuilderState& builderState)
{
    auto fontDescription = builderState.fontDescription();
    float size = Style::fontSizeForKeyword(CSSValueMedium, fontDescription.useFixedDefaultSize(), builderState.document());

    if (size < 0)
        return;

    fontDescription.setKeywordSizeFromIdentifier(CSSValueMedium);
    builderState.setFontSize(fontDescription, size);
    builderState.setFontDescription(WTF::move(fontDescription));
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
    switch (builderState.style().rubyPosition()) {
    case RubyPosition::Over:
    case RubyPosition::Under:
        return 0.5f;

    case RubyPosition::InterCharacter:
        // If the writing mode of the enclosing ruby container is vertical, 'inter-character' value has the same effect as over.
        return !builderState.parentStyle().writingMode().isVerticalTypographic() ? 0.3f : 0.5f;

    case RubyPosition::LegacyInterCharacter:
        // FIXME: This hack is to ensure tone marks are the same size as
        // the bopomofo. This code will go away if we make a special renderer
        // for the tone marks eventually.
        if (RefPtr element = builderState.element()) {
            for (Ref ancestor : ancestorsOfType<HTMLElement>(*element)) {
                if (ancestor->hasTagName(HTMLNames::rtTag))
                    return 1.0f;
            }
        }
        return 0.25f;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

// https://w3c.github.io/mathml-core/#the-math-script-level-property
inline float BuilderCustom::determineMathDepthScale(BuilderState& builderState)
{
    // Step 1.
    auto inherited = builderState.parentStyle().mathDepth();
    auto computed = builderState.style().mathDepth();
    float scale = 1.0f;
    float scaleDown = 0.71f;

    // Step 2.
    if (inherited == computed)
        return scale;
    bool invertScaleFactor = false;
    if (computed < inherited) {
        std::swap(computed, inherited);
        invertScaleFactor = true;
    }

    // Step 3.
    int exponent = computed.value - inherited.value;

    // Step 4.
#if ENABLE(MATHML)
    Ref primaryFont = builderState.style().fontCascade().primaryFont();
    if (RefPtr mathData = primaryFont->mathData()) {
        float scriptPercentScaleDown = mathData->getMathConstant(primaryFont, OpenTypeMathData::ScriptPercentScaleDown);
        if (!scriptPercentScaleDown)
            scriptPercentScaleDown = 0.71;

        float scriptScriptPercentScaleDown = mathData->getMathConstant(primaryFont, OpenTypeMathData::ScriptScriptPercentScaleDown);
        if (!scriptScriptPercentScaleDown)
            scriptScriptPercentScaleDown = 0.5041;

        if (inherited <= 0 && computed >= 2) {
            scale *= scriptScriptPercentScaleDown;
            exponent -= 2;
        } else if (inherited == 1) {
            scale *= scriptScriptPercentScaleDown / scriptPercentScaleDown;
            exponent--;
        } else if (computed == 1) {
            scale *= scriptPercentScaleDown;
            exponent--;
        }
    }
#endif

    // Step 5.
    scale *= std::pow(scaleDown, exponent);

    // Step 6.
    return invertScaleFactor ? 1.f / scale : scale;
}

inline void BuilderCustom::applyValueFontSize(BuilderState& builderState, CSSValue& value)
{
    auto& fontDescription = builderState.fontDescription();
    builderState.setFontDescriptionKeywordSizeFromIdentifier(CSSValueInvalid);

    float parentSize = builderState.parentStyle().fontDescription().specifiedSize();
    bool parentIsAbsoluteSize = builderState.parentStyle().fontDescription().isAbsoluteSize();

    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return;

    float size = 0;
    if (CSSValueID ident = primitiveValue->valueID()) {
        builderState.setFontDescriptionIsAbsoluteSize((parentIsAbsoluteSize && (ident == CSSValueLarger || ident == CSSValueSmaller || ident == CSSValueWebkitRubyText || ident == CSSValueMath)) || CSSPropertyParserHelpers::isSystemFontShorthand(ident));

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
        case CSSValueMath:
            size = determineMathDepthScale(builderState) * parentSize;
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

} // namespace Style
} // namespace WebCore
