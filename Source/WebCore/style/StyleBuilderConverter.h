/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2014-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2023 ChangSeok Oh <changseok@webkit.org>
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
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
#include "CSSBasicShapeValue.h"
#include "CSSCalcSymbolTable.h"
#include "CSSCalcValue.h"
#include "CSSContentDistributionValue.h"
#include "CSSCounterStyleRegistry.h"
#include "CSSDynamicRangeLimitValue.h"
#include "CSSFontFeatureValue.h"
#include "CSSFontStyleWithAngleValue.h"
#include "CSSFontVariationValue.h"
#include "CSSFunctionValue.h"
#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
#include "CSSOffsetRotateValue.h"
#include "CSSPathValue.h"
#include "CSSPositionValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyParserConsumer+Font.h"
#include "CSSReflectValue.h"
#include "CSSSubgridValue.h"
#include "CSSURLValue.h"
#include "CSSValuePair.h"
#include "CalculationValue.h"
#include "FontPalette.h"
#include "FontSelectionValueInlines.h"
#include "FontSizeAdjust.h"
#include "FrameDestructionObserverInlines.h"
#include "LineClampValue.h"
#include "LocalFrame.h"
#include "Quirks.h"
#include "QuotesData.h"
#include "RenderStyleInlines.h"
#include "RotateTransformOperation.h"
#include "SVGElementTypeHelpers.h"
#include "SVGPathElement.h"
#include "SVGRenderStyle.h"
#include "ScaleTransformOperation.h"
#include "ScopedName.h"
#include "ScrollAxis.h"
#include "ScrollbarColor.h"
#include "Settings.h"
#include "StyleBasicShape.h"
#include "StyleBuilderChecking.h"
#include "StyleClipPath.h"
#include "StyleColorScheme.h"
#include "StyleCornerShapeValue.h"
#include "StyleDynamicRangeLimit.h"
#include "StyleEasingFunction.h"
#include "StyleFlexBasis.h"
#include "StyleInset.h"
#include "StyleLengthWrapper+CSSValueConversion.h"
#include "StyleLineBoxContain.h"
#include "StyleMargin.h"
#include "StyleMaximumSize.h"
#include "StyleMinimumSize.h"
#include "StyleOffsetAnchor.h"
#include "StyleOffsetDistance.h"
#include "StyleOffsetPath.h"
#include "StyleOffsetPosition.h"
#include "StyleOffsetRotate.h"
#include "StylePadding.h"
#include "StylePathData.h"
#include "StylePerspective.h"
#include "StylePreferredSize.h"
#include "StylePrimitiveKeyword+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StyleRayFunction.h"
#include "StyleReflection.h"
#include "StyleResolveForFont.h"
#include "StyleRotate.h"
#include "StyleSVGPaint.h"
#include "StyleScale.h"
#include "StyleScrollMargin.h"
#include "StyleScrollPadding.h"
#include "StyleScrollSnapPoints.h"
#include "StyleTextEdge.h"
#include "StyleTranslate.h"
#include "StyleURL.h"
#include "StyleValueTypes+CSSValueConversion.h"
#include "TabSize.h"
#include "TextSpacing.h"
#include "TimelineRange.h"
#include "TouchAction.h"
#include "TransformOperationsBuilder.h"
#include "ViewTimeline.h"
#include "WillChangeData.h"
#include <ranges>
#include <wtf/text/MakeString.h>

namespace WebCore {
namespace Style {

// FIXME: Some of those functions assume the CSS parser only allows valid CSSValue types.
// This might not be true if we pass the CSSValue from js via CSS Typed OM.

class BuilderConverter {
public:
    template<typename T, typename... Rest> static T convertStyleType(BuilderState&, const CSSValue&, Rest&&...);

    static WebCore::Length convertLength(BuilderState&, const CSSValue&);
    static WebCore::Length convertLengthOrAuto(BuilderState&, const CSSValue&);
    static WebCore::Length convertLengthAllowingNumber(BuilderState&, const CSSValue&); // Assumes unit is 'px' if input is a number.
    static WebCore::Length convertTextLengthOrNormal(BuilderState&, const CSSValue&); // Converts length by text zoom factor, normal to zero
    static TabSize convertTabSize(BuilderState&, const CSSValue&);
    template<typename T> static T convertComputedLength(BuilderState&, const CSSValue&);
    template<typename T> static T convertLineWidth(BuilderState&, const CSSValue&);
    static OptionSet<TextDecorationLine> convertTextDecorationLine(BuilderState&, const CSSValue&);
    static OptionSet<TextTransform> convertTextTransform(BuilderState&, const CSSValue&);
    template<typename T> static T convertNumber(BuilderState&, const CSSValue&);
    template<typename T, CSSValueID> static T convertNumberOrKeyword(BuilderState&, const CSSValue&);
    static RefPtr<StyleImage> convertImageOrNone(BuilderState&, CSSValue&);
    static ImageOrientation convertImageOrientation(BuilderState&, const CSSValue&);
    static TransformOperations convertTransform(BuilderState&, const CSSValue&);
    static String convertString(BuilderState&, const CSSValue&);
    template<CSSValueID> static String convertStringOrKeyword(BuilderState&, const CSSValue&);
    template<CSSValueID> static String convertCustomIdentOrKeyword(BuilderState&, const CSSValue&);
    template<CSSValueID> static AtomString convertStringAtomOrKeyword(BuilderState&, const CSSValue&);
    template<CSSValueID> static AtomString convertCustomIdentAtomOrKeyword(BuilderState&, const CSSValue&);

    static OptionSet<TextEmphasisPosition> convertTextEmphasisPosition(BuilderState&, const CSSValue&);
    static TextAlignMode convertTextAlign(BuilderState&, const CSSValue&);
    static TextAlignLast convertTextAlignLast(BuilderState&, const CSSValue&);
    static RefPtr<StylePathData> convertDPath(BuilderState&, const CSSValue&);
    static Resize convertResize(BuilderState&, const CSSValue&);
    static int convertMarqueeRepetition(BuilderState&, const CSSValue&);
    static int convertMarqueeSpeed(BuilderState&, const CSSValue&);
    static RefPtr<QuotesData> convertQuotes(BuilderState&, const CSSValue&);
    static OptionSet<TextUnderlinePosition> convertTextUnderlinePosition(BuilderState&, const CSSValue&);
    static TextDecorationThickness convertTextDecorationThickness(BuilderState&, const CSSValue&);
    static RefPtr<StyleReflection> convertReflection(BuilderState&, const CSSValue&);
    static TextEdge convertTextEdge(BuilderState&, const CSSValue&);
    static IntSize convertInitialLetter(BuilderState&, const CSSValue&);
    static float convertTextStrokeWidth(BuilderState&, const CSSValue&);
    static OptionSet<LineBoxContain> convertLineBoxContain(BuilderState&, const CSSValue&);
    static RefPtr<ShapeValue> convertShapeValue(BuilderState&, const CSSValue&);
    static ScrollSnapType convertScrollSnapType(BuilderState&, const CSSValue&);
    static ScrollSnapAlign convertScrollSnapAlign(BuilderState&, const CSSValue&);
    static std::optional<ScrollbarColor> convertScrollbarColor(BuilderState&, const CSSValue&);
    // scrollbar-width converter is only needed for quirking.
    static ScrollbarWidth convertScrollbarWidth(BuilderState&, const CSSValue&);
    static GridAutoFlow convertGridAutoFlow(BuilderState&, const CSSValue&);
    static FilterOperations convertFilterOperations(BuilderState&, const CSSValue&);
    static FilterOperations convertAppleColorFilterOperations(BuilderState&, const CSSValue&);
#if PLATFORM(IOS_FAMILY)
    static bool convertTouchCallout(BuilderState&, const CSSValue&);
#endif
#if ENABLE(TOUCH_EVENTS)
    static Color convertTapHighlightColor(BuilderState&, const CSSValue&);
#endif
    static OptionSet<TouchAction> convertTouchAction(BuilderState&, const CSSValue&);
#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
    static bool convertOverflowScrolling(BuilderState&, const CSSValue&);
#endif
    static bool convertSmoothScrolling(BuilderState&, const CSSValue&);

    static FontSizeAdjust convertFontSizeAdjust(BuilderState&, const CSSValue&);
    static std::optional<FontSelectionValue> convertFontStyleFromValue(BuilderState&, const CSSValue&);
    static FontSelectionValue convertFontWeight(BuilderState&, const CSSValue&);
    static FontSelectionValue convertFontWidth(BuilderState&, const CSSValue&);
    static FontSelectionValue convertFontStyle(BuilderState&, const CSSValue&);
    static FontFeatureSettings convertFontFeatureSettings(BuilderState&, const CSSValue&);
    static FontVariationSettings convertFontVariationSettings(BuilderState&, const CSSValue&);
    static FixedVector<WebCore::Length> convertStrokeDashArray(BuilderState&, const CSSValue&);
    static PaintOrder convertPaintOrder(BuilderState&, const CSSValue&);
    static float convertOpacity(BuilderState&, const CSSValue&);
    static URL convertSVGURIReference(BuilderState&, const CSSValue&);
    static StyleSelfAlignmentData convertSelfOrDefaultAlignmentData(BuilderState&, const CSSValue&);
    static StyleContentAlignmentData convertContentAlignmentData(BuilderState&, const CSSValue&);
    static GlyphOrientation convertGlyphOrientation(BuilderState&, const CSSValue&);
    static GlyphOrientation convertGlyphOrientationOrAuto(BuilderState&, const CSSValue&);
    static WebCore::Length convertLineHeight(BuilderState&, const CSSValue&, float multiplier = 1.f);
    static FontPalette convertFontPalette(BuilderState&, const CSSValue&);
    
    static BreakBetween convertPageBreakBetween(BuilderState&, const CSSValue&);
    static BreakInside convertPageBreakInside(BuilderState&, const CSSValue&);
    static BreakBetween convertColumnBreakBetween(BuilderState&, const CSSValue&);
    static BreakInside convertColumnBreakInside(BuilderState&, const CSSValue&);

    static OptionSet<HangingPunctuation> convertHangingPunctuation(BuilderState&, const CSSValue&);

    static OptionSet<SpeakAs> convertSpeakAs(BuilderState&, const CSSValue&);

    static OptionSet<Containment> convertContain(BuilderState&, const CSSValue&);

    static OptionSet<MarginTrimType> convertMarginTrim(BuilderState&, const CSSValue&);

    static TextSpacingTrim convertTextSpacingTrim(BuilderState&, const CSSValue&);
    static TextAutospace convertTextAutospace(BuilderState&, const CSSValue&);

    static std::optional<WebCore::Length> convertBlockStepSize(BuilderState&, const CSSValue&);

    static ViewTransitionName convertViewTransitionName(BuilderState&, const CSSValue&);
    static RefPtr<WillChangeData> convertWillChange(BuilderState&, const CSSValue&);

    static std::optional<ScopedName> convertPositionAnchor(BuilderState&, const CSSValue&);
    static std::optional<PositionArea> convertPositionArea(BuilderState&, const CSSValue&);
    static OptionSet<PositionVisibility> convertPositionVisibility(BuilderState&, const CSSValue&);

    static size_t convertMaxLines(BuilderState&, const CSSValue&);

    static LineClampValue convertLineClamp(BuilderState&, const CSSValue&);

    static RefPtr<TimingFunction> convertTimingFunction(BuilderState&, const CSSValue&);

    static NameScope convertNameScope(BuilderState&, const CSSValue&);

    static SingleTimelineRange convertAnimationRangeStart(BuilderState&, const CSSValue&);
    static SingleTimelineRange convertAnimationRangeEnd(BuilderState&, const CSSValue&);

    static FixedVector<PositionTryFallback> convertPositionTryFallbacks(BuilderState&, const CSSValue&);

private:
    friend class BuilderCustom;

    static WebCore::Length parseSnapCoordinate(BuilderState&, const CSSValue&);

    static CSSToLengthConversionData cssToLengthConversionDataWithTextZoomFactor(BuilderState&);
};

template<typename T, typename... Rest> inline T BuilderConverter::convertStyleType(BuilderState& builderState, const CSSValue& value, Rest&&... rest)
{
    return toStyleFromCSSValue<T>(builderState, value, std::forward<Rest>(rest)...);
}

inline WebCore::Length BuilderConverter::convertLength(BuilderState& builderState, const CSSValue& value)
{
    auto* primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };

    CSSToLengthConversionData conversionData = builderState.useSVGZoomRulesForLength() ?
        builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
        : builderState.cssToLengthConversionData();

    if (primitiveValue->isLength()) {
        auto length = primitiveValue->resolveAsLength<WebCore::Length>(conversionData);
        length.setHasQuirk(primitiveValue->primitiveType() == CSSUnitType::CSS_QUIRKY_EM);
        return length;
    }

    if (primitiveValue->isPercentage())
        return WebCore::Length(primitiveValue->resolveAsPercentage(conversionData), LengthType::Percent);

    if (primitiveValue->isCalculatedPercentageWithLength())
        return WebCore::Length(primitiveValue->cssCalcValue()->createCalculationValue(conversionData, CSSCalcSymbolTable { }));

    ASSERT_NOT_REACHED();
    return WebCore::Length(0, LengthType::Fixed);
}

inline WebCore::Length BuilderConverter::convertLengthAllowingNumber(BuilderState& builderState, const CSSValue& value)
{
    CSSToLengthConversionData conversionData = builderState.useSVGZoomRulesForLength() ?
        builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
        : builderState.cssToLengthConversionData();

    auto* primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };

    if (primitiveValue->isNumberOrInteger())
        return WebCore::Length(primitiveValue->resolveAsNumber(conversionData), LengthType::Fixed);
    return convertLength(builderState, value);
}

inline WebCore::Length BuilderConverter::convertLengthOrAuto(BuilderState& builderState, const CSSValue& value)
{
    if (value.valueID() == CSSValueAuto)
        return WebCore::Length(LengthType::Auto);
    return convertLength(builderState, value);
}

inline TabSize BuilderConverter::convertTabSize(BuilderState& builderState, const CSSValue& value)
{
    auto* primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };
    if (primitiveValue->isNumber())
        return TabSize(primitiveValue->resolveAsNumber<float>(builderState.cssToLengthConversionData()), SpaceValueType);
    return TabSize(primitiveValue->resolveAsLength<float>(builderState.cssToLengthConversionData()), LengthValueType);
}

template<typename T>
inline T BuilderConverter::convertComputedLength(BuilderState& builderState, const CSSValue& value)
{
    auto* primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };
    return primitiveValue->resolveAsLength<T>(builderState.cssToLengthConversionData());
}

template<typename T>
inline T BuilderConverter::convertLineWidth(BuilderState& builderState, const CSSValue& value)
{
    auto* primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };
    switch (primitiveValue->valueID()) {
    case CSSValueThin:
        return 1;
    case CSSValueMedium:
        return 3;
    case CSSValueThick:
        return 5;
    case CSSValueInvalid: {
        // Any original result that was >= 1 should not be allowed to fall below 1.
        // This keeps border lines from vanishing.
        T result = convertComputedLength<T>(builderState, value);
        if (builderState.style().usedZoom() < 1.0f && result < 1.0) {
            T originalLength = primitiveValue->resolveAsLength<T>(builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0));
            if (originalLength >= 1.0)
                return 1;
        }
        float minimumLineWidth = 1 / builderState.document().deviceScaleFactor();
        if (result > 0 && result < minimumLineWidth)
            return minimumLineWidth;
        return floorToDevicePixel(result, builderState.document().deviceScaleFactor());
    }
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

inline OptionSet<TextDecorationLine> BuilderConverter::convertTextDecorationLine(BuilderState&, const CSSValue& value)
{
    auto result = RenderStyle::initialTextDecorationLine();
    if (auto* list = dynamicDowncast<CSSValueList>(value)) {
        for (auto& currentValue : *list)
            result.add(fromCSSValue<TextDecorationLine>(currentValue));
    }
    return result;
}

inline OptionSet<TextTransform> BuilderConverter::convertTextTransform(BuilderState&, const CSSValue& value)
{
    auto result = RenderStyle::initialTextTransform();
    if (auto* list = dynamicDowncast<CSSValueList>(value)) {
        for (auto& currentValue : *list)
            result.add(fromCSSValue<TextTransform>(currentValue));
    }
    return result;
}

template<typename T>
inline T BuilderConverter::convertNumber(BuilderState& builderState, const CSSValue& value)
{
    auto* primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };
    return primitiveValue->resolveAsNumber<T>(builderState.cssToLengthConversionData());
}

template<typename T, CSSValueID keyword>
inline T BuilderConverter::convertNumberOrKeyword(BuilderState& builderState, const CSSValue& value)
{
    if (value.valueID() == keyword)
        return -1;
    return convertNumber<T>(builderState, value);
}

inline RefPtr<StyleImage> BuilderConverter::convertImageOrNone(BuilderState& builderState, CSSValue& value)
{
    return builderState.createStyleImage(value);
}

inline ImageOrientation BuilderConverter::convertImageOrientation(BuilderState& builderState, const CSSValue& value)
{
    auto* primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };
    if (primitiveValue->valueID() == CSSValueFromImage)
        return ImageOrientation::Orientation::FromImage;
    return ImageOrientation::Orientation::None;
}

inline TransformOperations BuilderConverter::convertTransform(BuilderState& builderState, const CSSValue& value)
{
    CSSToLengthConversionData conversionData = builderState.useSVGZoomRulesForLength() ?
        builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
        : builderState.cssToLengthConversionData();
    return createTransformOperations(value, builderState);
}

inline String BuilderConverter::convertString(BuilderState& builderState, const CSSValue& value)
{
    auto* primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };
    return primitiveValue->stringValue();
}

template<CSSValueID keyword> inline String BuilderConverter::convertStringOrKeyword(BuilderState& builderState, const CSSValue& value)
{
    if (value.valueID() == keyword)
        return nullAtom();
    return convertString(builderState, value);
}

template<CSSValueID keyword> inline String BuilderConverter::convertCustomIdentOrKeyword(BuilderState& builderState, const CSSValue& value)
{
    if (value.valueID() == keyword)
        return nullAtom();
    return convertString(builderState, value);
}

template<CSSValueID keyword> inline AtomString BuilderConverter::convertStringAtomOrKeyword(BuilderState& builderState, const CSSValue& value)
{
    if (value.valueID() == keyword)
        return nullAtom();
    return AtomString { convertString(builderState, value) };
}

template<CSSValueID keyword> inline AtomString BuilderConverter::convertCustomIdentAtomOrKeyword(BuilderState& builderState, const CSSValue& value)
{
    if (value.valueID() == keyword)
        return nullAtom();
    return AtomString { convertString(builderState, value) };
}

inline static OptionSet<TextEmphasisPosition> valueToEmphasisPosition(const CSSPrimitiveValue& primitiveValue)
{
    ASSERT(primitiveValue.isValueID());

    switch (primitiveValue.valueID()) {
    case CSSValueOver:
        return TextEmphasisPosition::Over;
    case CSSValueUnder:
        return TextEmphasisPosition::Under;
    case CSSValueLeft:
        return TextEmphasisPosition::Left;
    case CSSValueRight:
        return TextEmphasisPosition::Right;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return RenderStyle::initialTextEmphasisPosition();
}

inline OptionSet<TextEmphasisPosition> BuilderConverter::convertTextEmphasisPosition(BuilderState& builderState, const CSSValue& value)
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value))
        return valueToEmphasisPosition(*primitiveValue);

    auto list = requiredListDowncast<CSSValueList, CSSPrimitiveValue>(builderState, value);
    if (!list)
        return { };

    OptionSet<TextEmphasisPosition> position;
    for (auto& currentValue : *list)
        position.add(valueToEmphasisPosition(currentValue));
    return position;
}

inline TextAlignMode BuilderConverter::convertTextAlign(BuilderState& builderState, const CSSValue& value)
{
    auto* primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };
    ASSERT(primitiveValue->isValueID());

    const auto& parentStyle = builderState.parentStyle();

    // User agents are expected to have a rule in their user agent stylesheet that matches th elements that have a parent
    // node whose computed value for the 'text-align' property is its initial value, whose declaration block consists of
    // just a single declaration that sets the 'text-align' property to the value 'center'.
    // https://html.spec.whatwg.org/multipage/rendering.html#rendering
    if (primitiveValue->valueID() == CSSValueInternalThCenter) {
        if (parentStyle.textAlign() == RenderStyle::initialTextAlign())
            return TextAlignMode::Center;
        return parentStyle.textAlign();
    }

    if (primitiveValue->valueID() == CSSValueWebkitMatchParent || primitiveValue->valueID() == CSSValueMatchParent) {
        const auto* element = builderState.element();

        if (element && element == builderState.document().documentElement())
            return TextAlignMode::Start;
        if (parentStyle.textAlign() == TextAlignMode::Start)
            return parentStyle.writingMode().isBidiLTR() ? TextAlignMode::Left : TextAlignMode::Right;
        if (parentStyle.textAlign() == TextAlignMode::End)
            return parentStyle.writingMode().isBidiLTR() ? TextAlignMode::Right : TextAlignMode::Left;

        return parentStyle.textAlign();
    }

    return fromCSSValue<TextAlignMode>(value);
}

inline TextAlignLast BuilderConverter::convertTextAlignLast(BuilderState& builderState, const CSSValue& value)
{
    auto* primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };
    ASSERT(primitiveValue->isValueID());

    if (primitiveValue->valueID() != CSSValueMatchParent)
        return fromCSSValue<TextAlignLast>(value);

    auto& parentStyle = builderState.parentStyle();
    if (parentStyle.textAlignLast() == TextAlignLast::Start)
        return parentStyle.writingMode().isBidiLTR() ? TextAlignLast::Left : TextAlignLast::Right;
    if (parentStyle.textAlignLast() == TextAlignLast::End)
        return parentStyle.writingMode().isBidiLTR() ? TextAlignLast::Right : TextAlignLast::Left;
    return parentStyle.textAlignLast();
}

inline RefPtr<StylePathData> BuilderConverter::convertDPath(BuilderState& builderState, const CSSValue& value)
{
    if (RefPtr pathValue = dynamicDowncast<CSSPathValue>(value))
        return StylePathData::create(toStyle(pathValue->path(), builderState));

    ASSERT(is<CSSPrimitiveValue>(value));
    ASSERT(downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone);
    return nullptr;
}

inline Resize BuilderConverter::convertResize(BuilderState& builderState, const CSSValue& value)
{
    auto* primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };

    auto resize = Resize::None;
    if (primitiveValue->valueID() == CSSValueInternalTextareaAuto)
        resize = builderState.document().settings().textAreasAreResizable() ? Resize::Both : Resize::None;
    else
        resize = fromCSSValue<Resize>(value);

    return resize;
}

inline int BuilderConverter::convertMarqueeRepetition(BuilderState& builderState, const CSSValue& value)
{
    auto* primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };
    if (primitiveValue->valueID() == CSSValueInfinite)
        return -1; // -1 means repeat forever.

    ASSERT(primitiveValue->isNumber());
    return primitiveValue->resolveAsNumber<int>(builderState.cssToLengthConversionData());
}

inline int BuilderConverter::convertMarqueeSpeed(BuilderState& builderState, const CSSValue& value)
{
    auto& conversionData = builderState.cssToLengthConversionData();

    auto* primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };
    if (primitiveValue->isTime())
        return primitiveValue->resolveAsTime<int, CSS::TimeUnit::Ms>(conversionData);

    // For scrollamount support.
    ASSERT(primitiveValue->isNumber());
    return primitiveValue->resolveAsNumber<int>(conversionData);
}

inline RefPtr<QuotesData> BuilderConverter::convertQuotes(BuilderState& builderState, const CSSValue& value)
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        if (primitiveValue->valueID() == CSSValueNone)
            return QuotesData::create({ });
        ASSERT(primitiveValue->valueID() == CSSValueAuto);
        return nullptr;
    }

    auto list = requiredListDowncast<CSSValueList, CSSPrimitiveValue>(builderState, value);
    if (!list)
        return { };

    Vector<std::pair<String, String>> quotes;
    quotes.reserveInitialCapacity(list->size() / 2);
    for (unsigned i = 0; i < list->size(); i += 2) {
        auto& first = list->item(i);
        if (list->size() <= i + 1)
            break;
        auto& second = list->item(i + 1);
        String startQuote = first.stringValue();
        String endQuote = second.stringValue();
        quotes.append(std::make_pair(startQuote, endQuote));
    }
    return QuotesData::create(quotes);
}

inline static OptionSet<TextUnderlinePosition> valueToUnderlinePosition(const CSSPrimitiveValue& primitiveValue)
{
    ASSERT(primitiveValue.isValueID());

    switch (primitiveValue.valueID()) {
    case CSSValueFromFont:
        return TextUnderlinePosition::FromFont;
    case CSSValueUnder:
        return TextUnderlinePosition::Under;
    case CSSValueLeft:
        return TextUnderlinePosition::Left;
    case CSSValueRight:
        return TextUnderlinePosition::Right;
    case CSSValueAuto:
        return RenderStyle::initialTextUnderlinePosition();
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return RenderStyle::initialTextUnderlinePosition();
}

inline OptionSet<TextUnderlinePosition> BuilderConverter::convertTextUnderlinePosition(BuilderState& builderState, const CSSValue& value)
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value))
        return valueToUnderlinePosition(*primitiveValue);

    auto pair = requiredPairDowncast<CSSPrimitiveValue>(builderState, value);
    if (!pair)
        return { };

    auto position = valueToUnderlinePosition(pair->first);
    position.add(valueToUnderlinePosition(pair->second));
    return position;
}

inline TextDecorationThickness BuilderConverter::convertTextDecorationThickness(BuilderState& builderState, const CSSValue& value)
{
    auto* primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };
    switch (primitiveValue->valueID()) {
    case CSSValueAuto:
        return TextDecorationThickness::createWithAuto();
    case CSSValueFromFont:
        return TextDecorationThickness::createFromFont();
    default: {
        auto conversionData = builderState.cssToLengthConversionData();

        if (primitiveValue->isPercentage())
            return TextDecorationThickness::createWithLength(WebCore::Length(clampTo<float>(primitiveValue->resolveAsPercentage(conversionData), minValueForCssLength, maxValueForCssLength), LengthType::Percent));

        if (primitiveValue->isCalculatedPercentageWithLength())
            return TextDecorationThickness::createWithLength(WebCore::Length(primitiveValue->cssCalcValue()->createCalculationValue(conversionData, CSSCalcSymbolTable { })));

        ASSERT(primitiveValue->isLength());
        return TextDecorationThickness::createWithLength(primitiveValue->resolveAsLength<WebCore::Length>(conversionData));
    }
    }
}

inline RefPtr<StyleReflection> BuilderConverter::convertReflection(BuilderState& builderState, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(value.valueID() == CSSValueNone);
        return nullptr;
    }

    auto* reflectValue = requiredDowncast<CSSReflectValue>(builderState, value);
    if (!reflectValue)
        return { };

    NinePieceImage mask(NinePieceImage::Type::Mask);
    mask.setFill(true);

    builderState.styleMap().mapNinePieceImage(reflectValue->mask(), mask);

    auto reflection = StyleReflection::create();
    reflection->setDirection(fromCSSValueID<ReflectionDirection>(reflectValue->direction()));
    reflection->setOffset(convertLength(builderState, reflectValue->offset()));
    reflection->setMask(mask);
    return reflection;
}

inline TextEdge BuilderConverter::convertTextEdge(BuilderState& builderState, const CSSValue& value)
{
    auto overValue = [&](CSSValueID valueID) {
        switch (valueID) {
        case CSSValueText:
            return TextEdgeType::Text;
        case CSSValueCap:
            return TextEdgeType::CapHeight;
        case CSSValueEx:
            return TextEdgeType::ExHeight;
        case CSSValueIdeographic:
            return TextEdgeType::CJKIdeographic;
        case CSSValueIdeographicInk:
            return TextEdgeType::CJKIdeographicInk;
        default:
            ASSERT_NOT_REACHED();
            return TextEdgeType::Auto;
        }
    };

    auto underValue = [&](CSSValueID valueID) {
        switch (valueID) {
        case CSSValueText:
            return TextEdgeType::Text;
        case CSSValueAlphabetic:
            return TextEdgeType::Alphabetic;
        case CSSValueIdeographic:
            return TextEdgeType::CJKIdeographic;
        case CSSValueIdeographicInk:
            return TextEdgeType::CJKIdeographicInk;
        default:
            ASSERT_NOT_REACHED();
            return TextEdgeType::Auto;
        }
    };

    // One value was given.
    if (is<CSSPrimitiveValue>(value)) {
        switch (value.valueID()) {
        case CSSValueAuto:
            return { TextEdgeType::Auto, TextEdgeType::Auto };
        case CSSValueLeading:
            return { TextEdgeType::Leading, TextEdgeType::Leading };
        // https://www.w3.org/TR/css-inline-3/#text-edges
        // "If only one value is specified, both edges are assigned that same keyword if possible; else text is assumed as the missing value."
        case CSSValueCap:
        case CSSValueEx:
            return { overValue(value.valueID()), TextEdgeType::Text };
        default:
            return { overValue(value.valueID()), underValue(value.valueID()) };
        }
    }

    // Two values were given.
    auto pair = requiredPairDowncast<CSSPrimitiveValue>(builderState, value);
    if (!pair)
        return { };

    return {
        overValue(pair->first->valueID()),
        underValue(pair->second->valueID())
    };
}

inline IntSize BuilderConverter::convertInitialLetter(BuilderState& builderState, const CSSValue& value)
{
    if (value.valueID() == CSSValueNormal)
        return IntSize();

    auto& conversionData = builderState.cssToLengthConversionData();

    auto pair = requiredPairDowncast<CSSPrimitiveValue>(builderState, value);
    if (!pair)
        return { };

    return {
        pair->second->resolveAsNumber<int>(conversionData),
        pair->first->resolveAsNumber<int>(conversionData)
    };
}

inline float BuilderConverter::convertTextStrokeWidth(BuilderState& builderState, const CSSValue& value)
{
    auto* primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };

    float width = 0;
    switch (primitiveValue->valueID()) {
    case CSSValueThin:
    case CSSValueMedium:
    case CSSValueThick: {
        double result = 1.0 / 48;
        if (primitiveValue->valueID() == CSSValueMedium)
            result *= 3;
        else if (primitiveValue->valueID() == CSSValueThick)
            result *= 5;
        auto emsValue = CSSPrimitiveValue::create(result, CSSUnitType::CSS_EM);
        width = convertComputedLength<float>(builderState, emsValue);
        break;
    }
    case CSSValueInvalid: {
        width = convertComputedLength<float>(builderState, *primitiveValue);
        break;
    }
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }

    return width;
}

inline OptionSet<LineBoxContain> BuilderConverter::convertLineBoxContain(BuilderState& builderState, const CSSValue& value)
{
    if (RefPtr primitive = dynamicDowncast<CSSPrimitiveValue>(value)) {
        switch (primitive->valueID()) {
        case CSSValueNone:
            return { };
        case CSSValueBlock:
            return LineBoxContain::Block;
        case CSSValueInline:
            return LineBoxContain::Inline;
        case CSSValueFont:
            return LineBoxContain::Font;
        case CSSValueGlyphs:
            return LineBoxContain::Glyphs;
        case CSSValueReplaced:
            return LineBoxContain::Replaced;
        case CSSValueInlineBox:
            return LineBoxContain::InlineBox;
        case CSSValueInitialLetter:
            return LineBoxContain::InitialLetter;
        default:
            builderState.setCurrentPropertyInvalidAtComputedValueTime();
            return { };
        }
    }

    auto list = requiredListDowncast<CSSValueList, CSSPrimitiveValue>(builderState, value);
    if (!list)
        return { };

    OptionSet<LineBoxContain> result;
    for (Ref primitive : *list) {
        switch (primitive->valueID()) {
        case CSSValueBlock:
            result.add(LineBoxContain::Block);
            break;
        case CSSValueInline:
            result.add(LineBoxContain::Inline);
            break;
        case CSSValueFont:
            result.add(LineBoxContain::Font);
            break;
        case CSSValueGlyphs:
            result.add(LineBoxContain::Glyphs);
            break;
        case CSSValueReplaced:
            result.add(LineBoxContain::Replaced);
            break;
        case CSSValueInlineBox:
            result.add(LineBoxContain::InlineBox);
            break;
        case CSSValueInitialLetter:
            result.add(LineBoxContain::InitialLetter);
            break;
        default:
            builderState.setCurrentPropertyInvalidAtComputedValueTime();
            return { };
        }
    }
    return result;
}

inline RefPtr<ShapeValue> BuilderConverter::convertShapeValue(BuilderState& builderState, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(value.valueID() == CSSValueNone);
        return nullptr;
    }

    if (value.isImage())
        return ShapeValue::create(builderState.createStyleImage(value).releaseNonNull());

    std::optional<BasicShape> shape;
    auto referenceBox = CSSBoxType::BoxMissing;
    auto processSingleValue = [&](const CSSValue& currentValue) {
        if (RefPtr shapeValue = dynamicDowncast<CSSBasicShapeValue>(currentValue))
            shape = toStyle(shapeValue->shape(), builderState, 1.0f);
        else
            referenceBox = fromCSSValue<CSSBoxType>(currentValue);
    };
    if (auto* list = dynamicDowncast<CSSValueList>(value)) {
        for (auto& currentValue : *list)
            processSingleValue(currentValue);
    } else
        processSingleValue(value);

    
    if (shape)
        return ShapeValue::create(WTFMove(*shape), referenceBox);

    if (referenceBox != CSSBoxType::BoxMissing)
        return ShapeValue::create(referenceBox);

    ASSERT_NOT_REACHED();
    return nullptr;
}

inline ScrollSnapType BuilderConverter::convertScrollSnapType(BuilderState& builderState, const CSSValue& value)
{
    ScrollSnapType type;
    auto list = requiredListDowncast<CSSValueList, CSSPrimitiveValue>(builderState, value);
    if (!list)
        return { };

    auto& firstValue = list->item(0);
    if (firstValue.valueID() == CSSValueNone)
        return type;

    type.axis = fromCSSValue<ScrollSnapAxis>(firstValue);
    if (list->size() == 2)
        type.strictness = fromCSSValue<ScrollSnapStrictness>(list->item(1));
    else
        type.strictness = ScrollSnapStrictness::Proximity;

    return type;
}

inline ScrollSnapAlign BuilderConverter::convertScrollSnapAlign(BuilderState& builderState, const CSSValue& value)
{
    auto pair = requiredPairDowncast<CSSPrimitiveValue>(builderState, value);
    if (!pair)
        return { };

    return {
        fromCSSValue<ScrollSnapAxisAlignType>(pair->first),
        fromCSSValue<ScrollSnapAxisAlignType>(pair->second)
    };
}

inline std::optional<ScrollbarColor> BuilderConverter::convertScrollbarColor(BuilderState& builderState, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(value.valueID() == CSSValueAuto);
        return std::nullopt;
    }

    auto* pair = requiredDowncast<CSSValuePair>(builderState, value);
    if (!pair)
        return { };

    return ScrollbarColor {
        convertStyleType<Color>(builderState, pair->first(), ForVisitedLink::No),
        convertStyleType<Color>(builderState, pair->second(), ForVisitedLink::No),
    };
}

inline ScrollbarWidth BuilderConverter::convertScrollbarWidth(BuilderState& builderState, const CSSValue& value)
{
    auto scrollbarWidth = fromCSSValue<ScrollbarWidth>(value);
    if (scrollbarWidth == ScrollbarWidth::Thin && builderState.document().quirks().needsScrollbarWidthThinDisabledQuirk())
        return ScrollbarWidth::Auto;

    return scrollbarWidth;
}

inline GridAutoFlow BuilderConverter::convertGridAutoFlow(BuilderState& builderState, const CSSValue& value)
{
    ASSERT(!is<CSSPrimitiveValue>(value) || downcast<CSSPrimitiveValue>(value).isValueID());

    auto* list = dynamicDowncast<CSSValueList>(value);
    if (list && !list->size())
        return RenderStyle::initialGridAutoFlow();

    auto* first = requiredDowncast<CSSPrimitiveValue>(builderState, list ? *(list->item(0)) : value);
    if (!first)
        return { };
    auto* second = dynamicDowncast<CSSPrimitiveValue>(list && list->size() == 2 ? list->item(1) : nullptr);

    GridAutoFlow autoFlow;
    switch (first->valueID()) {
    case CSSValueRow:
        if (second && second->valueID() == CSSValueDense)
            autoFlow = AutoFlowRowDense;
        else
            autoFlow = AutoFlowRow;
        break;
    case CSSValueColumn:
        if (second && second->valueID() == CSSValueDense)
            autoFlow = AutoFlowColumnDense;
        else
            autoFlow = AutoFlowColumn;
        break;
    case CSSValueDense:
        if (second && second->valueID() == CSSValueColumn)
            autoFlow = AutoFlowColumnDense;
        else
            autoFlow = AutoFlowRowDense;
        break;
    default:
        ASSERT_NOT_REACHED();
        autoFlow = RenderStyle::initialGridAutoFlow();
        break;
    }

    return autoFlow;
}

inline float zoomWithTextZoomFactor(BuilderState& builderState)
{
    if (auto* frame = builderState.document().frame()) {
        float textZoomFactor = builderState.style().textZoom() != TextZoom::Reset ? frame->textZoomFactor() : 1.0f;
        return builderState.style().usedZoom() * textZoomFactor;
    }
    return builderState.cssToLengthConversionData().zoom();
}

inline CSSToLengthConversionData BuilderConverter::cssToLengthConversionDataWithTextZoomFactor(BuilderState& builderState)
{
    float zoom = zoomWithTextZoomFactor(builderState);
    if (zoom == builderState.cssToLengthConversionData().zoom())
        return builderState.cssToLengthConversionData();

    return builderState.cssToLengthConversionData().copyWithAdjustedZoom(zoom);
}

inline WebCore::Length BuilderConverter::convertTextLengthOrNormal(BuilderState& builderState, const CSSValue& value)
{
    auto* primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };
    auto conversionData = (builderState.useSVGZoomRulesForLength())
        ? builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
        : cssToLengthConversionDataWithTextZoomFactor(builderState);

    if (primitiveValue->valueID() == CSSValueNormal)
        return RenderStyle::zeroLength();
    if (primitiveValue->isLength())
        return primitiveValue->resolveAsLength<WebCore::Length>(conversionData);
    if (primitiveValue->isPercentage())
        return WebCore::Length(clampTo<float>(primitiveValue->resolveAsPercentage(conversionData), minValueForCssLength, maxValueForCssLength), LengthType::Percent);
    if (primitiveValue->isCalculatedPercentageWithLength())
        return WebCore::Length(primitiveValue->cssCalcValue()->createCalculationValue(conversionData, CSSCalcSymbolTable { }));
    if (primitiveValue->isNumber())
        return WebCore::Length(primitiveValue->resolveAsNumber(conversionData), LengthType::Fixed);
    ASSERT_NOT_REACHED();
    return RenderStyle::zeroLength();
}


inline FilterOperations BuilderConverter::convertFilterOperations(BuilderState& builderState, const CSSValue& value)
{
    return builderState.createFilterOperations(value);
}

inline FilterOperations BuilderConverter::convertAppleColorFilterOperations(BuilderState& builderState, const CSSValue& value)
{
    return builderState.createAppleColorFilterOperations(value);
}

// The input value needs to parsed and valid, this function returns std::nullopt if the input was "normal".
inline std::optional<FontSelectionValue> BuilderConverter::convertFontStyleFromValue(BuilderState& builderState, const CSSValue& value)
{
    return fontStyleFromCSSValue(builderState, value);
}

inline FontSelectionValue BuilderConverter::convertFontWeight(BuilderState& builderState, const CSSValue& value)
{
    return fontWeightFromCSSValue(builderState, value);
}

inline FontSelectionValue BuilderConverter::convertFontWidth(BuilderState& builderState, const CSSValue& value)
{
    return fontStretchFromCSSValue(builderState, value);
}

inline FontFeatureSettings BuilderConverter::convertFontFeatureSettings(BuilderState& builderState, const CSSValue& value)
{
    return fontFeatureSettingsFromCSSValue(builderState, value);
}

inline FontVariationSettings BuilderConverter::convertFontVariationSettings(BuilderState& builderState, const CSSValue& value)
{
    return fontVariationSettingsFromCSSValue(builderState, value);
}

inline FontSizeAdjust BuilderConverter::convertFontSizeAdjust(BuilderState& builderState, const CSSValue& value)
{
    return fontSizeAdjustFromCSSValue(builderState, value);
}

#if PLATFORM(IOS_FAMILY)
inline bool BuilderConverter::convertTouchCallout(BuilderState& builderState, const CSSValue& value)
{
    auto* primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };
    return !equalLettersIgnoringASCIICase(primitiveValue->stringValue(), "none"_s);
}
#endif

#if ENABLE(TOUCH_EVENTS)
inline Color BuilderConverter::convertTapHighlightColor(BuilderState& builderState, const CSSValue& value)
{
    return convertStyleType<Color>(builderState, value, ForVisitedLink::No);
}
#endif

inline OptionSet<TouchAction> BuilderConverter::convertTouchAction(BuilderState&, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value))
        return fromCSSValue<TouchAction>(value);

    if (auto* list = dynamicDowncast<CSSValueList>(value)) {
        OptionSet<TouchAction> touchActions;
        for (auto& currentValue : *list) {
            auto valueID = currentValue.valueID();
            if (valueID != CSSValuePanX && valueID != CSSValuePanY && valueID != CSSValuePinchZoom)
                return RenderStyle::initialTouchActions();
            touchActions.add(fromCSSValueID<TouchAction>(valueID));
        }
        return touchActions;
    }

    return RenderStyle::initialTouchActions();
}

#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
inline bool BuilderConverter::convertOverflowScrolling(BuilderState&, const CSSValue& value)
{
    return value.valueID() == CSSValueTouch;
}
#endif

inline bool BuilderConverter::convertSmoothScrolling(BuilderState&, const CSSValue& value)
{
    return value.valueID() == CSSValueSmooth;
}

inline FixedVector<WebCore::Length> BuilderConverter::convertStrokeDashArray(BuilderState& builderState, const CSSValue& value)
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        if (primitiveValue->valueID() == CSSValueNone)
            return SVGRenderStyle::initialStrokeDashArray();

        // Values coming from CSS-Typed-OM may not have been converted to a CSSValueList yet.
        return FixedVector<WebCore::Length> { convertLengthAllowingNumber(builderState, *primitiveValue) };
    }

    auto list = requiredListDowncast<CSSValueList, CSSPrimitiveValue>(builderState, value);
    if (!list)
        return { };

    return FixedVector<WebCore::Length>::map(*list, [&](auto& item) {
        return convertLengthAllowingNumber(builderState, item);
    });
}

inline PaintOrder BuilderConverter::convertPaintOrder(BuilderState& builderState, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(value.valueID() == CSSValueNormal);
        return PaintOrder::Normal;
    }

    auto list = requiredListDowncast<CSSValueList, CSSPrimitiveValue>(builderState, value);
    if (!list)
        return { };

    switch (list->item(0).valueID()) {
    case CSSValueFill:
        return list->size() > 1 ? PaintOrder::FillMarkers : PaintOrder::Fill;
    case CSSValueStroke:
        return list->size() > 1 ? PaintOrder::StrokeMarkers : PaintOrder::Stroke;
    case CSSValueMarkers:
        return list->size() > 1 ? PaintOrder::MarkersStroke : PaintOrder::Markers;
    default:
        ASSERT_NOT_REACHED();
        return PaintOrder::Normal;
    }
}

inline float BuilderConverter::convertOpacity(BuilderState& builderState, const CSSValue& value)
{
    auto* primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };

    float opacity = primitiveValue->valueDividingBy100IfPercentage(builderState.cssToLengthConversionData());
    return std::max(0.0f, std::min(1.0f, opacity));
}

inline URL BuilderConverter::convertSVGURIReference(BuilderState& builderState, const CSSValue& value)
{
    if (auto url = dynamicDowncast<CSSURLValue>(value))
        return toStyle(url->url(), builderState);

    auto* primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return URL::none();

    ASSERT(primitiveValue->valueID() == CSSValueNone);
    return URL::none();
}

// Get the "opposite" ItemPosition to the provided ItemPosition.
// e.g: start -> end, end -> start, self-start -> self-end.
// Position that doesn't have an opposite value is returned as-is.
inline ItemPosition oppositeItemPosition(ItemPosition position)
{
    switch (position) {
    case ItemPosition::Legacy:
    case ItemPosition::Auto:
    case ItemPosition::Normal:
    case ItemPosition::Stretch:
    case ItemPosition::Baseline:
    case ItemPosition::LastBaseline:
    case ItemPosition::Center:
    case ItemPosition::AnchorCenter:
        return position;

    case ItemPosition::Start:
        return ItemPosition::End;
    case ItemPosition::End:
        return ItemPosition::Start;

    case ItemPosition::SelfStart:
        return ItemPosition::SelfEnd;
    case ItemPosition::SelfEnd:
        return ItemPosition::SelfStart;

    case ItemPosition::FlexStart:
        return ItemPosition::FlexEnd;
    case ItemPosition::FlexEnd:
        return ItemPosition::FlexStart;

    case ItemPosition::Left:
        return ItemPosition::Right;
    case ItemPosition::Right:
        return ItemPosition::Left;
    }

    ASSERT_NOT_REACHED();
    return position;
}

inline StyleSelfAlignmentData BuilderConverter::convertSelfOrDefaultAlignmentData(BuilderState& builderState, const CSSValue& value)
{
    auto alignmentData = RenderStyle::initialSelfAlignment();

    if (value.isPair()) {
        if (value.first().valueID() == CSSValueLegacy) {
            alignmentData.setPositionType(ItemPositionType::Legacy);
            alignmentData.setPosition(fromCSSValue<ItemPosition>(value.second()));
        } else if (value.first().valueID() == CSSValueFirst)
            alignmentData.setPosition(ItemPosition::Baseline);
        else if (value.first().valueID() == CSSValueLast)
            alignmentData.setPosition(ItemPosition::LastBaseline);
        else {
            alignmentData.setOverflow(fromCSSValue<OverflowAlignment>(value.first()));
            alignmentData.setPosition(fromCSSValue<ItemPosition>(value.second()));
        }
    } else
        alignmentData.setPosition(fromCSSValue<ItemPosition>(value));

    // Flip the position according to position-try fallback, if specified.
    if (auto positionTryFallback = builderState.positionTryFallback()) {
        for (auto tactic : positionTryFallback->tactics) {
            switch (tactic) {
            case PositionTryFallback::Tactic::FlipBlock:
                if (builderState.cssPropertyID() == CSSPropertyAlignSelf)
                    alignmentData.setPosition(oppositeItemPosition(alignmentData.position()));
                break;

            case PositionTryFallback::Tactic::FlipInline:
                if (builderState.cssPropertyID() == CSSPropertyJustifySelf)
                    alignmentData.setPosition(oppositeItemPosition(alignmentData.position()));
                break;

            case PositionTryFallback::Tactic::FlipStart:
                // justify-self additionally takes left/right, align-self doesn't. When
                // applying flip-start, justify-self gets swapped with align-self. So if
                // we're resolving justify-self (which later gets swapped with align-self),
                // and the position is 'left' or 'right', resolve it to self-start/self-end.
                if (builderState.cssPropertyID() == CSSPropertyJustifySelf) {
                    auto writingMode = builderState.style().writingMode();

                    switch (alignmentData.position()) {
                    case ItemPosition::Left:
                        alignmentData.setPosition(writingMode.bidiDirection() == TextDirection::LTR ? ItemPosition::SelfStart : ItemPosition::SelfEnd);
                        break;
                    case ItemPosition::Right:
                        alignmentData.setPosition(writingMode.bidiDirection() == TextDirection::LTR ? ItemPosition::SelfEnd : ItemPosition::SelfStart);
                        break;
                    default:
                        break;
                    }
                }

                break;
            }
        }
    }

    return alignmentData;
}

inline StyleContentAlignmentData BuilderConverter::convertContentAlignmentData(BuilderState&, const CSSValue& value)
{
    StyleContentAlignmentData alignmentData = RenderStyle::initialContentAlignment();
    auto* contentValue = dynamicDowncast<CSSContentDistributionValue>(value);
    if (!contentValue)
        return alignmentData;
    if (contentValue->distribution() != CSSValueInvalid)
        alignmentData.setDistribution(fromCSSValueID<ContentDistribution>(contentValue->distribution()));
    if (contentValue->position() != CSSValueInvalid)
        alignmentData.setPosition(fromCSSValueID<ContentPosition>(contentValue->position()));
    if (contentValue->overflow() != CSSValueInvalid)
        alignmentData.setOverflow(fromCSSValueID<OverflowAlignment>(contentValue->overflow()));
    return alignmentData;
}

inline GlyphOrientation BuilderConverter::convertGlyphOrientation(BuilderState& builderState, const CSSValue& value)
{
    auto* primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };

    float angle = std::abs(fmodf(primitiveValue->resolveAsAngle(builderState.cssToLengthConversionData()), 360.0f));
    if (angle <= 45.0f || angle > 315.0f)
        return GlyphOrientation::Degrees0;
    if (angle > 45.0f && angle <= 135.0f)
        return GlyphOrientation::Degrees90;
    if (angle > 135.0f && angle <= 225.0f)
        return GlyphOrientation::Degrees180;
    return GlyphOrientation::Degrees270;
}

inline GlyphOrientation BuilderConverter::convertGlyphOrientationOrAuto(BuilderState& builderState, const CSSValue& value)
{
    if (value.valueID() == CSSValueAuto)
        return GlyphOrientation::Auto;
    return convertGlyphOrientation(builderState, value);
}

inline WebCore::Length BuilderConverter::convertLineHeight(BuilderState& builderState, const CSSValue& value, float multiplier)
{
    auto* primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };

    auto valueID = primitiveValue->valueID();
    if (valueID == CSSValueNormal)
        return RenderStyle::initialLineHeight();

    if (CSSPropertyParserHelpers::isSystemFontShorthand(valueID))
        return RenderStyle::initialLineHeight();

    auto conversionData = builderState.cssToLengthConversionData().copyForLineHeight(zoomWithTextZoomFactor(builderState));

    if (primitiveValue->isLength() || primitiveValue->isCalculatedPercentageWithLength()) {
        WebCore::Length length;
        if (primitiveValue->isLength())
            length = primitiveValue->resolveAsLength<WebCore::Length>(conversionData);
        else {
            auto value = primitiveValue->cssCalcValue()->createCalculationValue(conversionData, CSSCalcSymbolTable { })->evaluate(builderState.style().computedFontSize());
            length = { clampTo<float>(value, minValueForCssLength, maxValueForCssLength), LengthType::Fixed };
        }
        if (multiplier != 1.f)
            length = WebCore::Length(length.value() * multiplier, LengthType::Fixed);
        return length;
    }

    // Line-height percentages need to inherit as if they were Fixed pixel values. In the example:
    // <div style="font-size: 10px; line-height: 150%;"><div style="font-size: 100px;"></div></div>
    // the inner element should have line-height of 15px. However, in this example:
    // <div style="font-size: 10px; line-height: 1.5;"><div style="font-size: 100px;"></div></div>
    // the inner element should have a line-height of 150px. Therefore, we map percentages to Fixed
    // values and raw numbers to percentages.
    if (primitiveValue->isPercentage()) {
        // FIXME: percentage should not be restricted to an integer here.
        return WebCore::Length((builderState.style().computedFontSize() * primitiveValue->resolveAsPercentage<int>(conversionData)) / 100, LengthType::Fixed);
    }

    ASSERT(primitiveValue->isNumber());
    return WebCore::Length(primitiveValue->resolveAsNumber(conversionData) * 100.0, LengthType::Percent);
}

inline FontPalette BuilderConverter::convertFontPalette(BuilderState& builderState, const CSSValue& value)
{
    auto* primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };

    switch (primitiveValue->valueID()) {
    case CSSValueLight:
        return { FontPalette::Type::Light, nullAtom() };
    case CSSValueDark:
        return { FontPalette::Type::Dark, nullAtom() };
    case CSSValueInvalid:
        ASSERT(primitiveValue->isCustomIdent());
        return { FontPalette::Type::Custom, AtomString { primitiveValue->stringValue() } };
    default:
        ASSERT(primitiveValue->valueID() == CSSValueNormal || CSSPropertyParserHelpers::isSystemFontShorthand(primitiveValue->valueID()));
        return { FontPalette::Type::Normal, nullAtom() };
    }
}
    
inline OptionSet<SpeakAs> BuilderConverter::convertSpeakAs(BuilderState&, const CSSValue& value)
{
    auto result = RenderStyle::initialSpeakAs();
    if (auto* list = dynamicDowncast<CSSValueList>(value)) {
        for (auto& currentValue : *list) {
            if (!isValueID(currentValue, CSSValueNormal))
                result.add(fromCSSValue<SpeakAs>(currentValue));
        }
    }
    return result;
}

inline OptionSet<HangingPunctuation> BuilderConverter::convertHangingPunctuation(BuilderState&, const CSSValue& value)
{
    auto result = RenderStyle::initialHangingPunctuation();
    if (auto* list = dynamicDowncast<CSSValueList>(value)) {
        for (auto& currentValue : *list)
            result.add(fromCSSValue<HangingPunctuation>(currentValue));
    }
    return result;
}

inline OptionSet<MarginTrimType> BuilderConverter::convertMarginTrim(BuilderState&, const CSSValue& value)
{
    // See if value is "block" or "inline" before trying to parse a list
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        if (primitiveValue->valueID() == CSSValueBlock)
            return { MarginTrimType::BlockStart, MarginTrimType::BlockEnd };
        if (primitiveValue->valueID() == CSSValueInline)
            return { MarginTrimType::InlineStart, MarginTrimType::InlineEnd };
    }
    auto list = dynamicDowncast<CSSValueList>(value);
    if (!list || !list->size())
        return RenderStyle::initialMarginTrim();
    OptionSet<MarginTrimType> marginTrim;
    for (auto& item : *list) {
        if (item.valueID() == CSSValueBlock)
            marginTrim.add({ MarginTrimType::BlockStart, MarginTrimType::BlockEnd });
        if (item.valueID() == CSSValueInline)
            marginTrim.add({ MarginTrimType::InlineStart, MarginTrimType::InlineEnd });
    }
    if (!marginTrim.isEmpty())
        return marginTrim;
    for (auto& item : *list) {
        if (item.valueID() == CSSValueBlockStart)
            marginTrim.add(MarginTrimType::BlockStart);
        if (item.valueID() == CSSValueBlockEnd)
            marginTrim.add(MarginTrimType::BlockEnd);
        if (item.valueID() == CSSValueInlineStart)
            marginTrim.add(MarginTrimType::InlineStart);
        if (item.valueID() == CSSValueInlineEnd)
            marginTrim.add(MarginTrimType::InlineEnd);
    }
    ASSERT(list->size() <= 4);
    return marginTrim;
}

inline TextSpacingTrim BuilderConverter::convertTextSpacingTrim(BuilderState&, const CSSValue& value)
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        switch (primitiveValue->valueID()) {
        case CSSValueSpaceAll:
            return TextSpacingTrim::TrimType::SpaceAll;
        case CSSValueTrimAll:
            return TextSpacingTrim::TrimType::TrimAll;
        case CSSValueAuto:
            return TextSpacingTrim::TrimType::Auto;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }
    return { };
}

inline TextAutospace BuilderConverter::convertTextAutospace(BuilderState& builderState, const CSSValue& value)
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        if (primitiveValue->valueID() == CSSValueNoAutospace)
            return { };
        if (primitiveValue->valueID() == CSSValueAuto)
            return { TextAutospace::Type::Auto };
        if (primitiveValue->valueID() == CSSValueNormal)
            return { TextAutospace::Type::Normal };
    }

    TextAutospace::Options options;

    auto list = requiredListDowncast<CSSValueList, CSSPrimitiveValue>(builderState, value);
    if (!list)
        return { };

    for (auto& value : *list) {
        switch (value.valueID()) {
        case CSSValueIdeographAlpha:
            options.add(TextAutospace::Type::IdeographAlpha);
            break;
        case CSSValueIdeographNumeric:
            options.add(TextAutospace::Type::IdeographNumeric);
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }
    return options;
}

inline std::optional<WebCore::Length> BuilderConverter::convertBlockStepSize(BuilderState& builderState, const CSSValue& value)
{
    if (value.valueID() == CSSValueNone)
        return { };
    return convertLength(builderState, value);
}

inline OptionSet<Containment> BuilderConverter::convertContain(BuilderState& builderState, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        if (value.valueID() == CSSValueNone)
            return RenderStyle::initialContainment();
        if (value.valueID() == CSSValueStrict)
            return RenderStyle::strictContainment();
        return RenderStyle::contentContainment();
    }

    OptionSet<Containment> containment;

    auto list = requiredListDowncast<CSSValueList, CSSPrimitiveValue>(builderState, value);
    if (!list)
        return { };

    for (auto& value : *list) {
        switch (value.valueID()) {
        case CSSValueSize:
            containment.add(Containment::Size);
            break;
        case CSSValueInlineSize:
            containment.add(Containment::InlineSize);
            break;
        case CSSValueLayout:
            containment.add(Containment::Layout);
            break;
        case CSSValuePaint:
            containment.add(Containment::Paint);
            break;
        case CSSValueStyle:
            containment.add(Containment::Style);
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        };
    }
    return containment;
}

inline ViewTransitionName BuilderConverter::convertViewTransitionName(BuilderState& state, const CSSValue& value)
{
    auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value);
    if (!primitiveValue)
        return ViewTransitionName::createWithNone();

    if (value.valueID() == CSSValueNone)
        return ViewTransitionName::createWithNone();

    if (value.valueID() == CSSValueAuto)
        return ViewTransitionName::createWithAuto(state.styleScopeOrdinal());

    if (value.valueID() == CSSValueMatchElement)
        return ViewTransitionName::createWithMatchElement(state.styleScopeOrdinal());

    return ViewTransitionName::createWithCustomIdent(state.styleScopeOrdinal(), AtomString { primitiveValue->stringValue() });
}

inline RefPtr<WillChangeData> BuilderConverter::convertWillChange(BuilderState& builderState, const CSSValue& value)
{
    if (value.valueID() == CSSValueAuto)
        return nullptr;

    auto willChange = WillChangeData::create();
    auto processSingleValue = [&](const CSSValue& item) {
        auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(item);
        if (!primitiveValue)
            return;
        switch (primitiveValue->valueID()) {
        case CSSValueScrollPosition:
            willChange->addFeature(WillChangeData::Feature::ScrollPosition);
            break;
        case CSSValueContents:
            willChange->addFeature(WillChangeData::Feature::Contents);
            break;
        default:
            if (primitiveValue->isPropertyID()) {
                if (!isExposed(primitiveValue->propertyID(), &builderState.document().settings()))
                    break;
                willChange->addFeature(WillChangeData::Feature::Property, primitiveValue->propertyID());
            }
            break;
        }
    };
    if (auto* list = dynamicDowncast<CSSValueList>(value)) {
        for (auto& item : *list)
            processSingleValue(item);
    } else
        processSingleValue(value);
    return willChange;
}

inline std::optional<ScopedName> BuilderConverter::convertPositionAnchor(BuilderState& builderState, const CSSValue& value)
{
    if (value.valueID() == CSSValueAuto)
        return { };
    return convertStyleType<ScopedName>(builderState, value);
}

static std::optional<PositionAreaAxis> positionAreaKeywordToAxis(CSSValueID keyword)
{
    switch (keyword) {
    case CSSValueLeft:
    case CSSValueSpanLeft:
    case CSSValueRight:
    case CSSValueSpanRight:
        return PositionAreaAxis::Horizontal;

    case CSSValueTop:
    case CSSValueSpanTop:
    case CSSValueBottom:
    case CSSValueSpanBottom:
        return PositionAreaAxis::Vertical;

    case CSSValueXStart:
    case CSSValueSpanXStart:
    case CSSValueXSelfStart:
    case CSSValueSpanXSelfStart:
    case CSSValueXEnd:
    case CSSValueSpanXEnd:
    case CSSValueXSelfEnd:
    case CSSValueSpanXSelfEnd:
        return PositionAreaAxis::X;

    case CSSValueYStart:
    case CSSValueSpanYStart:
    case CSSValueYSelfStart:
    case CSSValueSpanYSelfStart:
    case CSSValueYEnd:
    case CSSValueSpanYEnd:
    case CSSValueYSelfEnd:
    case CSSValueSpanYSelfEnd:
        return PositionAreaAxis::Y;

    case CSSValueBlockStart:
    case CSSValueSpanBlockStart:
    case CSSValueSelfBlockStart:
    case CSSValueSpanSelfBlockStart:
    case CSSValueBlockEnd:
    case CSSValueSpanBlockEnd:
    case CSSValueSelfBlockEnd:
    case CSSValueSpanSelfBlockEnd:
        return PositionAreaAxis::Block;

    case CSSValueInlineStart:
    case CSSValueSpanInlineStart:
    case CSSValueSelfInlineStart:
    case CSSValueSpanSelfInlineStart:
    case CSSValueInlineEnd:
    case CSSValueSpanInlineEnd:
    case CSSValueSelfInlineEnd:
    case CSSValueSpanSelfInlineEnd:
        return PositionAreaAxis::Inline;

    case CSSValueStart:
    case CSSValueSpanStart:
    case CSSValueSelfStart:
    case CSSValueSpanSelfStart:
    case CSSValueEnd:
    case CSSValueSpanEnd:
    case CSSValueSelfEnd:
    case CSSValueSpanSelfEnd:
    case CSSValueCenter:
    case CSSValueSpanAll:
        return { };

    default:
        ASSERT_NOT_REACHED();
        return { };
    }
}

static PositionAreaTrack positionAreaKeywordToTrack(CSSValueID keyword)
{
    switch (keyword) {
    case CSSValueLeft:
    case CSSValueTop:
    case CSSValueXStart:
    case CSSValueXSelfStart:
    case CSSValueYStart:
    case CSSValueYSelfStart:
    case CSSValueBlockStart:
    case CSSValueSelfBlockStart:
    case CSSValueInlineStart:
    case CSSValueSelfInlineStart:
    case CSSValueStart:
    case CSSValueSelfStart:
        return PositionAreaTrack::Start;

    case CSSValueSpanLeft:
    case CSSValueSpanTop:
    case CSSValueSpanXStart:
    case CSSValueSpanXSelfStart:
    case CSSValueSpanYStart:
    case CSSValueSpanYSelfStart:
    case CSSValueSpanBlockStart:
    case CSSValueSpanSelfBlockStart:
    case CSSValueSpanInlineStart:
    case CSSValueSpanSelfInlineStart:
    case CSSValueSpanStart:
    case CSSValueSpanSelfStart:
        return PositionAreaTrack::SpanStart;

    case CSSValueRight:
    case CSSValueBottom:
    case CSSValueXEnd:
    case CSSValueXSelfEnd:
    case CSSValueYEnd:
    case CSSValueYSelfEnd:
    case CSSValueBlockEnd:
    case CSSValueSelfBlockEnd:
    case CSSValueInlineEnd:
    case CSSValueSelfInlineEnd:
    case CSSValueEnd:
    case CSSValueSelfEnd:
        return PositionAreaTrack::End;

    case CSSValueSpanRight:
    case CSSValueSpanBottom:
    case CSSValueSpanXEnd:
    case CSSValueSpanXSelfEnd:
    case CSSValueSpanYEnd:
    case CSSValueSpanYSelfEnd:
    case CSSValueSpanBlockEnd:
    case CSSValueSpanSelfBlockEnd:
    case CSSValueSpanInlineEnd:
    case CSSValueSpanSelfInlineEnd:
    case CSSValueSpanEnd:
    case CSSValueSpanSelfEnd:
        return PositionAreaTrack::SpanEnd;

    case CSSValueCenter:
        return PositionAreaTrack::Center;
    case CSSValueSpanAll:
        return PositionAreaTrack::SpanAll;

    default:
        ASSERT_NOT_REACHED();
        return PositionAreaTrack::Start;
    }
}

static PositionAreaSelf positionAreaKeywordToSelf(CSSValueID keyword)
{
    switch (keyword) {
    case CSSValueLeft:
    case CSSValueSpanLeft:
    case CSSValueRight:
    case CSSValueSpanRight:
    case CSSValueTop:
    case CSSValueSpanTop:
    case CSSValueBottom:
    case CSSValueSpanBottom:
    case CSSValueXStart:
    case CSSValueSpanXStart:
    case CSSValueXEnd:
    case CSSValueSpanXEnd:
    case CSSValueYStart:
    case CSSValueSpanYStart:
    case CSSValueYEnd:
    case CSSValueSpanYEnd:
    case CSSValueBlockStart:
    case CSSValueSpanBlockStart:
    case CSSValueBlockEnd:
    case CSSValueSpanBlockEnd:
    case CSSValueInlineStart:
    case CSSValueSpanInlineStart:
    case CSSValueInlineEnd:
    case CSSValueSpanInlineEnd:
    case CSSValueStart:
    case CSSValueSpanStart:
    case CSSValueEnd:
    case CSSValueSpanEnd:
    case CSSValueCenter:
    case CSSValueSpanAll:
        return PositionAreaSelf::No;

    case CSSValueXSelfStart:
    case CSSValueSpanXSelfStart:
    case CSSValueXSelfEnd:
    case CSSValueSpanXSelfEnd:
    case CSSValueYSelfStart:
    case CSSValueSpanYSelfStart:
    case CSSValueYSelfEnd:
    case CSSValueSpanYSelfEnd:
    case CSSValueSelfBlockStart:
    case CSSValueSpanSelfBlockStart:
    case CSSValueSelfBlockEnd:
    case CSSValueSpanSelfBlockEnd:
    case CSSValueSelfInlineStart:
    case CSSValueSpanSelfInlineStart:
    case CSSValueSelfInlineEnd:
    case CSSValueSpanSelfInlineEnd:
    case CSSValueSelfStart:
    case CSSValueSpanSelfStart:
    case CSSValueSelfEnd:
    case CSSValueSpanSelfEnd:
        return PositionAreaSelf::Yes;

    default:
        ASSERT_NOT_REACHED();
        return PositionAreaSelf::No;
    }
}

// Expand a one keyword position-area to the equivalent keyword pair value.
static std::pair<CSSValueID, CSSValueID> positionAreaExpandKeyword(CSSValueID dim)
{
    auto maybeAxis = positionAreaKeywordToAxis(dim);
    if (maybeAxis) {
        // Keyword is axis unambiguous, second keyword is span-all.

        // Y/inline axis keyword goes after in the pair.
        auto axis = *maybeAxis;
        if (axis == PositionAreaAxis::Vertical || axis == PositionAreaAxis::Y || axis == PositionAreaAxis::Inline)
            return { CSSValueSpanAll, dim };

        return { dim, CSSValueSpanAll };
    }

    // Keyword is axis ambiguous, it's repeated.
    return { dim, dim };
}


// Flip a PositionArea across a logical axis (block or inline), given the current writing mode.
inline PositionArea flipPositionAreaByLogicalAxis(LogicalBoxAxis flipAxis, PositionArea area, WritingMode writingMode)
{
    auto blockOrXSpan = area.blockOrXAxis();
    auto inlineOrYSpan = area.inlineOrYAxis();

    // blockOrXSpan is on the flip axis, so flip its track and keep inlineOrYSpan intact.
    if (mapPositionAreaAxisToLogicalAxis(blockOrXSpan.axis(), writingMode) == flipAxis) {
        return {
            { blockOrXSpan.axis(), flipPositionAreaTrack(blockOrXSpan.track()), blockOrXSpan.self() },
            inlineOrYSpan
        };
    }

    // The two spans are orthogonal in axis, so if blockOrXSpan isn't on the flip axis,
    // inlineOrYSpan must be. In this case, flip the track of inlineOrYSpan, and
    // keep blockOrXSpan intact.
    return {
        blockOrXSpan,
        { inlineOrYSpan.axis(), flipPositionAreaTrack(inlineOrYSpan.track()), inlineOrYSpan.self() }
    };
}

// Flip a PositionArea as specified by flip-start tactic.
// Intuitively, this mirrors the PositionArea across a diagonal line drawn from the
// block-start/inline-start corner to the block-end/inline-end corner. This is done
// by flipping the axes of the spans in the PositionArea, while keeping their track
// and self properties intact. Because this turns a block/X span into an inline/Y
// span and vice versa, this function also swaps the order of the spans, so
// that the block/X span goes before the inline/Y span.
inline PositionArea mirrorPositionAreaAcrossDiagonal(PositionArea area)
{
    auto blockOrXSpan = area.blockOrXAxis();
    auto inlineOrYSpan = area.inlineOrYAxis();

    return {
        { oppositePositionAreaAxis(inlineOrYSpan.axis()), inlineOrYSpan.track(), inlineOrYSpan.self() },
        { oppositePositionAreaAxis(blockOrXSpan.axis()), blockOrXSpan.track(), blockOrXSpan.self() }
    };
}

inline std::optional<PositionArea> BuilderConverter::convertPositionArea(BuilderState& builderState, const CSSValue& value)
{
    std::pair<CSSValueID, CSSValueID> dimPair;

    if (value.isValueID()) {
        if (value.valueID() == CSSValueNone)
            return { };

        dimPair = positionAreaExpandKeyword(value.valueID());
    } else if (const auto* pair = dynamicDowncast<CSSValuePair>(value)) {
        const auto& first = pair->first();
        const auto& second = pair->second();
        ASSERT(first.isValueID() && second.isValueID());

        // The parsing logic guarantees the keyword pair is in the correct order
        // (horizontal/x/block axis before vertical/Y/inline axis)

        dimPair = { first.valueID(), second.valueID() };
    } else {
        // value MUST be a single ValueID or a pair of ValueIDs, as returned by the parsing logic.
        ASSERT_NOT_REACHED();
        return { };
    }

    auto dim1Axis = positionAreaKeywordToAxis(dimPair.first);
    auto dim2Axis = positionAreaKeywordToAxis(dimPair.second);

    // If both keyword axes are ambiguous, the first one is block axis and second one
    // is inline axis. If only one keyword axis is ambiguous, its axis is the opposite
    // of the other keyword's axis.
    if (!dim1Axis && !dim2Axis) {
        dim1Axis = PositionAreaAxis::Block;
        dim2Axis = PositionAreaAxis::Inline;
    } else if (!dim1Axis)
        dim1Axis = oppositePositionAreaAxis(*dim2Axis);
    else if (!dim2Axis)
        dim2Axis = oppositePositionAreaAxis(*dim1Axis);

    PositionArea area {
        { *dim1Axis, positionAreaKeywordToTrack(dimPair.first), positionAreaKeywordToSelf(dimPair.first) },
        { *dim2Axis, positionAreaKeywordToTrack(dimPair.second), positionAreaKeywordToSelf(dimPair.second) }
    };

    // Flip according to position-try-fallbacks, if specified.
    if (const auto& positionTryFallback = builderState.positionTryFallback()) {
        for (auto tactic : positionTryFallback->tactics) {
            switch (tactic) {
            case PositionTryFallback::Tactic::FlipBlock:
                area = flipPositionAreaByLogicalAxis(LogicalBoxAxis::Block, area, builderState.style().writingMode());
                break;
            case PositionTryFallback::Tactic::FlipInline:
                area = flipPositionAreaByLogicalAxis(LogicalBoxAxis::Inline, area, builderState.style().writingMode());
                break;
            case PositionTryFallback::Tactic::FlipStart:
                area = mirrorPositionAreaAcrossDiagonal(area);
                break;
            }
        }
    }

    return area;
}

inline OptionSet<PositionVisibility> BuilderConverter::convertPositionVisibility(BuilderState& builderState, const CSSValue& value)
{
    if (value.valueID() == CSSValueAlways)
        return { };

    auto maybeList = requiredListDowncast<CSSValueList, CSSPrimitiveValue>(builderState, value);
    if (!maybeList)
        return { };
    auto list = *maybeList;

    OptionSet<PositionVisibility> result;
    for (const auto& value : list)
        result.add(fromCSSValue<PositionVisibility>(value));

    return result;
}

inline size_t BuilderConverter::convertMaxLines(BuilderState& builderState, const CSSValue& value)
{
    auto* primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };

    if (primitiveValue->valueID() == CSSValueNone)
        return 0;
    return convertNumber<size_t>(builderState, value);
}

inline LineClampValue BuilderConverter::convertLineClamp(BuilderState& builderState, const CSSValue& value)
{
    auto* primitiveValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return { };

    if (primitiveValue->primitiveType() == CSSUnitType::CSS_INTEGER)
        return LineClampValue(std::max(primitiveValue->resolveAsInteger<int>(builderState.cssToLengthConversionData()), 1), LineClamp::LineCount);

    if (primitiveValue->primitiveType() == CSSUnitType::CSS_PERCENTAGE)
        return LineClampValue(std::max(primitiveValue->resolveAsPercentage<int>(builderState.cssToLengthConversionData()), 0), LineClamp::Percentage);

    ASSERT(primitiveValue->valueID() == CSSValueNone);
    return LineClampValue();
}

inline RefPtr<TimingFunction> BuilderConverter::convertTimingFunction(BuilderState& builderState, const CSSValue& value)
{
    return createTimingFunction(value, builderState.cssToLengthConversionData());
}

inline NameScope BuilderConverter::convertNameScope(BuilderState& builderState, const CSSValue& value)
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        switch (primitiveValue->valueID()) {
        case CSSValueNone:
            return { };
        case CSSValueAll:
            return { NameScope::Type::All, { }, builderState.styleScopeOrdinal() };
        default:
            return { NameScope::Type::Ident, { AtomString { primitiveValue->stringValue() } }, builderState.styleScopeOrdinal() };
        }
    }

    auto list = requiredListDowncast<CSSValueList, CSSPrimitiveValue>(builderState, value);
    if (!list)
        return { };

    ListHashSet<AtomString> nameHashSet;
    for (auto& name : *list)
        nameHashSet.add(AtomString { name.stringValue() });

    return { NameScope::Type::Ident, WTFMove(nameHashSet), builderState.styleScopeOrdinal() };
}

inline FixedVector<PositionTryFallback> BuilderConverter::convertPositionTryFallbacks(BuilderState& builderState, const CSSValue& value)
{
    auto convertFallback = [&](const CSSValue& fallbackValue) -> std::optional<PositionTryFallback> {
        auto* valueList = dynamicDowncast<CSSValueList>(fallbackValue);
        if (!valueList) {
            // Turn the inlined position-area fallback into properties object that can be applied similarly to @position-try declarations.
            auto property = CSSProperty { CSSPropertyPositionArea, Ref { const_cast<CSSValue&>(fallbackValue) } };
            return PositionTryFallback {
                .positionAreaProperties = ImmutableStyleProperties::create(std::span { &property, 1 }, HTMLStandardMode)
            };
        }

        if (valueList->separator() != CSSValueList::SpaceSeparator)
            return { };

        auto fallback = PositionTryFallback { };

        for (auto& item : *valueList) {
            if (item.isCustomIdent())
                fallback.positionTryRuleName = ScopedName { AtomString { item.customIdent() }, builderState.styleScopeOrdinal() };
            else {
                auto tacticValue = fromCSSValueID<PositionTryFallback::Tactic>(item.valueID());
                if (fallback.tactics.contains(tacticValue)) {
                    ASSERT_NOT_REACHED();
                    return { };
                }

                fallback.tactics.append(tacticValue);
            }
        }
        return fallback;
    };

    if (value.valueID() == CSSValueNone)
        return { };

    if (auto fallback = convertFallback(value))
        return { *fallback };

    auto* list = dynamicDowncast<CSSValueList>(value);
    if (!list)
        return { };

    return FixedVector<PositionTryFallback>::map(*list, [&](auto& item) {
        auto fallback = convertFallback(item);
        return fallback ? *fallback : PositionTryFallback { };
    });
}

} // namespace Style
} // namespace WebCore
