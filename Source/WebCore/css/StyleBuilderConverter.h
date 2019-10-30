/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "BasicShapeFunctions.h"
#include "CSSCalculationValue.h"
#include "CSSContentDistributionValue.h"
#include "CSSFontFeatureValue.h"
#include "CSSFontStyleValue.h"
#include "CSSFontVariationValue.h"
#include "CSSFunctionValue.h"
#include "CSSGridAutoRepeatValue.h"
#include "CSSGridIntegerRepeatValue.h"
#include "CSSGridLineNamesValue.h"
#include "CSSImageGeneratorValue.h"
#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSReflectValue.h"
#include "FontSelectionValueInlines.h"
#include "Frame.h"
#include "GridPositionsResolver.h"
#include "Length.h"
#include "Pair.h"
#include "QuotesData.h"
#include "RuntimeEnabledFeatures.h"
#include "SVGURIReference.h"
#include "Settings.h"
#include "StyleBuilderState.h"
#include "StyleScrollSnapPoints.h"
#include "TabSize.h"
#include "TouchAction.h"
#include "TransformFunctions.h"
#include <wtf/Optional.h>

namespace WebCore {

// Note that we assume the CSS parser only allows valid CSSValue types.
class StyleBuilderConverter {
public:
    static Length convertLength(const Style::BuilderState&, const CSSValue&);
    static Length convertLengthOrAuto(const Style::BuilderState&, const CSSValue&);
    static Length convertLengthSizing(const Style::BuilderState&, const CSSValue&);
    static Length convertLengthMaxSizing(const Style::BuilderState&, const CSSValue&);
    static TabSize convertTabSize(const Style::BuilderState&, const CSSValue&);
    template<typename T> static T convertComputedLength(Style::BuilderState&, const CSSValue&);
    template<typename T> static T convertLineWidth(Style::BuilderState&, const CSSValue&);
    static float convertSpacing(Style::BuilderState&, const CSSValue&);
    static LengthSize convertRadius(Style::BuilderState&, const CSSValue&);
    static LengthPoint convertObjectPosition(Style::BuilderState&, const CSSValue&);
    static OptionSet<TextDecoration> convertTextDecoration(Style::BuilderState&, const CSSValue&);
    template<typename T> static T convertNumber(Style::BuilderState&, const CSSValue&);
    template<typename T> static T convertNumberOrAuto(Style::BuilderState&, const CSSValue&);
    static short convertWebkitHyphenateLimitLines(Style::BuilderState&, const CSSValue&);
    template<CSSPropertyID> static NinePieceImage convertBorderImage(Style::BuilderState&, CSSValue&);
    template<CSSPropertyID> static NinePieceImage convertBorderMask(Style::BuilderState&, CSSValue&);
    template<CSSPropertyID> static RefPtr<StyleImage> convertStyleImage(Style::BuilderState&, CSSValue&);
    static TransformOperations convertTransform(Style::BuilderState&, const CSSValue&);
#if ENABLE(DARK_MODE_CSS)
    static StyleColorScheme convertColorScheme(Style::BuilderState&, const CSSValue&);
#endif
    static String convertString(Style::BuilderState&, const CSSValue&);
    static String convertStringOrAuto(Style::BuilderState&, const CSSValue&);
    static String convertStringOrNone(Style::BuilderState&, const CSSValue&);
    static OptionSet<TextEmphasisPosition> convertTextEmphasisPosition(Style::BuilderState&, const CSSValue&);
    static TextAlignMode convertTextAlign(Style::BuilderState&, const CSSValue&);
    static RefPtr<ClipPathOperation> convertClipPath(Style::BuilderState&, const CSSValue&);
    static Resize convertResize(Style::BuilderState&, const CSSValue&);
    static int convertMarqueeRepetition(Style::BuilderState&, const CSSValue&);
    static int convertMarqueeSpeed(Style::BuilderState&, const CSSValue&);
    static Ref<QuotesData> convertQuotes(Style::BuilderState&, const CSSValue&);
    static TextUnderlinePosition convertTextUnderlinePosition(Style::BuilderState&, const CSSValue&);
    static TextUnderlineOffset convertTextUnderlineOffset(Style::BuilderState&, const CSSValue&);
    static TextDecorationThickness convertTextDecorationThickness(Style::BuilderState&, const CSSValue&);
    static RefPtr<StyleReflection> convertReflection(Style::BuilderState&, const CSSValue&);
    static IntSize convertInitialLetter(Style::BuilderState&, const CSSValue&);
    static float convertTextStrokeWidth(Style::BuilderState&, const CSSValue&);
    static OptionSet<LineBoxContain> convertLineBoxContain(Style::BuilderState&, const CSSValue&);
    static OptionSet<TextDecorationSkip> convertTextDecorationSkip(Style::BuilderState&, const CSSValue&);
    static RefPtr<ShapeValue> convertShapeValue(Style::BuilderState&, CSSValue&);
#if ENABLE(CSS_SCROLL_SNAP)
    static ScrollSnapType convertScrollSnapType(Style::BuilderState&, const CSSValue&);
    static ScrollSnapAlign convertScrollSnapAlign(Style::BuilderState&, const CSSValue&);
#endif
    static GridTrackSize convertGridTrackSize(Style::BuilderState&, const CSSValue&);
    static Vector<GridTrackSize> convertGridTrackSizeList(Style::BuilderState&, const CSSValue&);
    static Optional<GridPosition> convertGridPosition(Style::BuilderState&, const CSSValue&);
    static GridAutoFlow convertGridAutoFlow(Style::BuilderState&, const CSSValue&);
    static Optional<Length> convertWordSpacing(Style::BuilderState&, const CSSValue&);
    static Optional<float> convertPerspective(Style::BuilderState&, const CSSValue&);
    static Optional<Length> convertMarqueeIncrement(Style::BuilderState&, const CSSValue&);
    static Optional<FilterOperations> convertFilterOperations(Style::BuilderState&, const CSSValue&);
#if PLATFORM(IOS_FAMILY)
    static bool convertTouchCallout(Style::BuilderState&, const CSSValue&);
#endif
#if ENABLE(TOUCH_EVENTS)
    static Color convertTapHighlightColor(Style::BuilderState&, const CSSValue&);
#endif
#if ENABLE(POINTER_EVENTS)
    static OptionSet<TouchAction> convertTouchAction(Style::BuilderState&, const CSSValue&);
#endif
#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
    static bool convertOverflowScrolling(Style::BuilderState&, const CSSValue&);
#endif
    static FontFeatureSettings convertFontFeatureSettings(Style::BuilderState&, const CSSValue&);
    static FontSelectionValue convertFontWeightFromValue(const CSSValue&);
    static FontSelectionValue convertFontStretchFromValue(const CSSValue&);
    static Optional<FontSelectionValue> convertFontStyleFromValue(const CSSValue&);
    static FontSelectionValue convertFontWeight(Style::BuilderState&, const CSSValue&);
    static FontSelectionValue convertFontStretch(Style::BuilderState&, const CSSValue&);
    static FontSelectionValue convertFontStyle(Style::BuilderState&, const CSSValue&);
#if ENABLE(VARIATION_FONTS)
    static FontVariationSettings convertFontVariationSettings(Style::BuilderState&, const CSSValue&);
#endif
    static SVGLengthValue convertSVGLengthValue(Style::BuilderState&, const CSSValue&);
    static Vector<SVGLengthValue> convertSVGLengthVector(Style::BuilderState&, const CSSValue&);
    static Vector<SVGLengthValue> convertStrokeDashArray(Style::BuilderState&, const CSSValue&);
    static PaintOrder convertPaintOrder(Style::BuilderState&, const CSSValue&);
    static float convertOpacity(Style::BuilderState&, const CSSValue&);
    static String convertSVGURIReference(Style::BuilderState&, const CSSValue&);
    static Color convertSVGColor(Style::BuilderState&, const CSSValue&);
    static StyleSelfAlignmentData convertSelfOrDefaultAlignmentData(Style::BuilderState&, const CSSValue&);
    static StyleContentAlignmentData convertContentAlignmentData(Style::BuilderState&, const CSSValue&);
    static GlyphOrientation convertGlyphOrientation(Style::BuilderState&, const CSSValue&);
    static GlyphOrientation convertGlyphOrientationOrAuto(Style::BuilderState&, const CSSValue&);
    static Optional<Length> convertLineHeight(Style::BuilderState&, const CSSValue&, float multiplier = 1.f);
    static FontSynthesis convertFontSynthesis(Style::BuilderState&, const CSSValue&);
    
    static BreakBetween convertPageBreakBetween(Style::BuilderState&, const CSSValue&);
    static BreakInside convertPageBreakInside(Style::BuilderState&, const CSSValue&);
    static BreakBetween convertColumnBreakBetween(Style::BuilderState&, const CSSValue&);
    static BreakInside convertColumnBreakInside(Style::BuilderState&, const CSSValue&);

    static OptionSet<HangingPunctuation> convertHangingPunctuation(Style::BuilderState&, const CSSValue&);

    static OptionSet<SpeakAs> convertSpeakAs(Style::BuilderState&, const CSSValue&);

    static Length convertPositionComponentX(Style::BuilderState&, const CSSValue&);
    static Length convertPositionComponentY(Style::BuilderState&, const CSSValue&);

    static GapLength convertGapLength(Style::BuilderState&, const CSSValue&);
    
private:
    friend class StyleBuilderCustom;

    static Length convertToRadiusLength(CSSToLengthConversionData&, const CSSPrimitiveValue&);
    static OptionSet<TextEmphasisPosition> valueToEmphasisPosition(const CSSPrimitiveValue&);
    static OptionSet<TextDecorationSkip> valueToDecorationSkip(const CSSPrimitiveValue&);
#if ENABLE(CSS_SCROLL_SNAP)
    static Length parseSnapCoordinate(Style::BuilderState&, const CSSValue&);
#endif

#if ENABLE(DARK_MODE_CSS)
    static void updateColorScheme(const CSSPrimitiveValue&, StyleColorScheme&);
#endif

    static Length convertTo100PercentMinusLength(const Length&);
    template<CSSValueID, CSSValueID> static Length convertPositionComponent(Style::BuilderState&, const CSSPrimitiveValue&);

    static GridLength createGridTrackBreadth(const CSSPrimitiveValue&, Style::BuilderState&);
    static GridTrackSize createGridTrackSize(const CSSValue&, Style::BuilderState&);
    struct TracksData;
    static bool createGridTrackList(const CSSValue&, TracksData&, Style::BuilderState&);
    static bool createGridPosition(const CSSValue&, GridPosition&);
    static void createImplicitNamedGridLinesFromGridArea(const NamedGridAreaMap&, NamedGridLinesMap&, GridTrackSizingDirection);
    static CSSToLengthConversionData csstoLengthConversionDataWithTextZoomFactor(Style::BuilderState&);
};

inline Length StyleBuilderConverter::convertLength(const Style::BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    CSSToLengthConversionData conversionData = builderState.useSVGZoomRulesForLength() ?
        builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
        : builderState.cssToLengthConversionData();

    if (primitiveValue.isLength()) {
        Length length = primitiveValue.computeLength<Length>(conversionData);
        length.setHasQuirk(primitiveValue.isQuirkValue());
        return length;
    }

    if (primitiveValue.isPercentage())
        return Length(primitiveValue.doubleValue(), Percent);

    if (primitiveValue.isCalculatedPercentageWithLength())
        return Length(primitiveValue.cssCalcValue()->createCalculationValue(conversionData));

    ASSERT_NOT_REACHED();
    return Length(0, Fixed);
}

inline Length StyleBuilderConverter::convertLengthOrAuto(const Style::BuilderState& builderState, const CSSValue& value)
{
    if (downcast<CSSPrimitiveValue>(value).valueID() == CSSValueAuto)
        return Length(Auto);
    return convertLength(builderState, value);
}

inline Length StyleBuilderConverter::convertLengthSizing(const Style::BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    switch (primitiveValue.valueID()) {
    case CSSValueInvalid:
        return convertLength(builderState, value);
    case CSSValueIntrinsic:
        return Length(Intrinsic);
    case CSSValueMinIntrinsic:
        return Length(MinIntrinsic);
    case CSSValueMinContent:
    case CSSValueWebkitMinContent:
        return Length(MinContent);
    case CSSValueMaxContent:
    case CSSValueWebkitMaxContent:
        return Length(MaxContent);
    case CSSValueWebkitFillAvailable:
        return Length(FillAvailable);
    case CSSValueFitContent:
    case CSSValueWebkitFitContent:
        return Length(FitContent);
    case CSSValueAuto:
        return Length(Auto);
    default:
        ASSERT_NOT_REACHED();
        return Length();
    }
}

inline Length StyleBuilderConverter::convertLengthMaxSizing(const Style::BuilderState& builderState, const CSSValue& value)
{
    if (downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone)
        return Length(Undefined);
    return convertLengthSizing(builderState, value);
}

inline TabSize StyleBuilderConverter::convertTabSize(const Style::BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.isNumber())
        return TabSize(primitiveValue.floatValue(), SpaceValueType);
    return TabSize(primitiveValue.computeLength<float>(builderState.cssToLengthConversionData()), LengthValueType);
}

template<typename T>
inline T StyleBuilderConverter::convertComputedLength(Style::BuilderState& builderState, const CSSValue& value)
{
    return downcast<CSSPrimitiveValue>(value).computeLength<T>(builderState.cssToLengthConversionData());
}

template<typename T>
inline T StyleBuilderConverter::convertLineWidth(Style::BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    switch (primitiveValue.valueID()) {
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
        if (builderState.style().effectiveZoom() < 1.0f && result < 1.0) {
            T originalLength = primitiveValue.computeLength<T>(builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0));
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

inline float StyleBuilderConverter::convertSpacing(Style::BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueNormal)
        return 0.f;

    CSSToLengthConversionData conversionData = builderState.useSVGZoomRulesForLength() ?
        builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
        : builderState.cssToLengthConversionData();
    return primitiveValue.computeLength<float>(conversionData);
}

inline Length StyleBuilderConverter::convertToRadiusLength(CSSToLengthConversionData& conversionData, const CSSPrimitiveValue& value)
{
    if (value.isPercentage())
        return Length(value.doubleValue(), Percent);
    if (value.isCalculatedPercentageWithLength())
        return Length(value.cssCalcValue()->createCalculationValue(conversionData));
    return value.computeLength<Length>(conversionData);
}

inline LengthSize StyleBuilderConverter::convertRadius(Style::BuilderState& builderState, const CSSValue& value)
{
    auto* pair = downcast<CSSPrimitiveValue>(value).pairValue();
    if (!pair || !pair->first() || !pair->second())
        return { { 0, Fixed }, { 0, Fixed } };

    CSSToLengthConversionData conversionData = builderState.cssToLengthConversionData();
    LengthSize radius { convertToRadiusLength(conversionData, *pair->first()), convertToRadiusLength(conversionData, *pair->second()) };

    ASSERT(!radius.width.isNegative());
    ASSERT(!radius.height.isNegative());
    if (radius.width.isZero() || radius.height.isZero())
        return { { 0, Fixed }, { 0, Fixed } };

    return radius;
}

inline Length StyleBuilderConverter::convertTo100PercentMinusLength(const Length& length)
{
    if (length.isPercent())
        return Length(100 - length.value(), Percent);
    
    // Turn this into a calc expression: calc(100% - length)
    Vector<std::unique_ptr<CalcExpressionNode>> lengths;
    lengths.reserveInitialCapacity(2);
    lengths.uncheckedAppend(makeUnique<CalcExpressionLength>(Length(100, Percent)));
    lengths.uncheckedAppend(makeUnique<CalcExpressionLength>(length));
    auto op = makeUnique<CalcExpressionOperation>(WTFMove(lengths), CalcOperator::Subtract);
    return Length(CalculationValue::create(WTFMove(op), ValueRangeAll));
}

inline Length StyleBuilderConverter::convertPositionComponentX(Style::BuilderState& builderState, const CSSValue& value)
{
    return convertPositionComponent<CSSValueLeft, CSSValueRight>(builderState, downcast<CSSPrimitiveValue>(value));
}

inline Length StyleBuilderConverter::convertPositionComponentY(Style::BuilderState& builderState, const CSSValue& value)
{
    return convertPositionComponent<CSSValueTop, CSSValueBottom>(builderState, downcast<CSSPrimitiveValue>(value));
}

template<CSSValueID cssValueFor0, CSSValueID cssValueFor100>
inline Length StyleBuilderConverter::convertPositionComponent(Style::BuilderState& builderState, const CSSPrimitiveValue& value)
{
    Length length;

    auto* lengthValue = &value;
    bool relativeToTrailingEdge = false;
    
    if (value.isPair()) {
        auto& first = *value.pairValue()->first();
        if (first.valueID() == CSSValueRight || first.valueID() == CSSValueBottom)
            relativeToTrailingEdge = true;
        lengthValue = value.pairValue()->second();
    }
    
    if (value.isValueID()) {
        switch (value.valueID()) {
        case cssValueFor0:
            return Length(0, Percent);
        case cssValueFor100:
            return Length(100, Percent);
        case CSSValueCenter:
            return Length(50, Percent);
        default:
            ASSERT_NOT_REACHED();
        }
    }
        
    length = convertLength(builderState, *lengthValue);

    if (relativeToTrailingEdge)
        length = convertTo100PercentMinusLength(length);

    return length;
}

inline LengthPoint StyleBuilderConverter::convertObjectPosition(Style::BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    Pair* pair = primitiveValue.pairValue();
    if (!pair || !pair->first() || !pair->second())
        return RenderStyle::initialObjectPosition();

    Length lengthX = convertPositionComponent<CSSValueLeft, CSSValueRight>(builderState, *pair->first());
    Length lengthY = convertPositionComponent<CSSValueTop, CSSValueBottom>(builderState, *pair->second());

    return LengthPoint(lengthX, lengthY);
}

inline OptionSet<TextDecoration> StyleBuilderConverter::convertTextDecoration(Style::BuilderState&, const CSSValue& value)
{
    auto result = RenderStyle::initialTextDecoration();
    if (is<CSSValueList>(value)) {
        for (auto& currentValue : downcast<CSSValueList>(value))
            result.add(downcast<CSSPrimitiveValue>(currentValue.get()));
    }
    return result;
}

template<typename T>
inline T StyleBuilderConverter::convertNumber(Style::BuilderState&, const CSSValue& value)
{
    return downcast<CSSPrimitiveValue>(value).value<T>(CSSPrimitiveValue::CSS_NUMBER);
}

template<typename T>
inline T StyleBuilderConverter::convertNumberOrAuto(Style::BuilderState& builderState, const CSSValue& value)
{
    if (downcast<CSSPrimitiveValue>(value).valueID() == CSSValueAuto)
        return -1;
    return convertNumber<T>(builderState, value);
}

inline short StyleBuilderConverter::convertWebkitHyphenateLimitLines(Style::BuilderState&, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueNoLimit)
        return -1;
    return primitiveValue.value<short>(CSSPrimitiveValue::CSS_NUMBER);
}

template<CSSPropertyID property>
inline NinePieceImage StyleBuilderConverter::convertBorderImage(Style::BuilderState& builderState, CSSValue& value)
{
    NinePieceImage image;
    builderState.styleMap().mapNinePieceImage(property, &value, image);
    return image;
}

template<CSSPropertyID property>
inline NinePieceImage StyleBuilderConverter::convertBorderMask(Style::BuilderState& builderState, CSSValue& value)
{
    NinePieceImage image(NinePieceImage::Type::Mask);
    builderState.styleMap().mapNinePieceImage(property, &value, image);
    return image;
}

template<CSSPropertyID>
inline RefPtr<StyleImage> StyleBuilderConverter::convertStyleImage(Style::BuilderState& builderState, CSSValue& value)
{
    return builderState.createStyleImage(value);
}

inline TransformOperations StyleBuilderConverter::convertTransform(Style::BuilderState& builderState, const CSSValue& value)
{
    TransformOperations operations;
    transformsForValue(value, builderState.cssToLengthConversionData(), operations);
    return operations;
}

#if ENABLE(DARK_MODE_CSS)
inline void StyleBuilderConverter::updateColorScheme(const CSSPrimitiveValue& primitiveValue, StyleColorScheme& colorScheme)
{
    ASSERT(primitiveValue.isValueID());

    switch (primitiveValue.valueID()) {
    case CSSValueAuto:
        colorScheme = StyleColorScheme();
        break;
    case CSSValueOnly:
        colorScheme.setAllowsTransformations(false);
        break;
    case CSSValueLight:
        colorScheme.add(ColorScheme::Light);
        break;
    case CSSValueDark:
        colorScheme.add(ColorScheme::Dark);
        break;
    default:
        // Unknown identifiers are allowed and ignored.
        break;
    }
}

inline StyleColorScheme StyleBuilderConverter::convertColorScheme(Style::BuilderState&, const CSSValue& value)
{
    StyleColorScheme colorScheme;

    if (is<CSSValueList>(value)) {
        for (auto& currentValue : downcast<CSSValueList>(value))
            updateColorScheme(downcast<CSSPrimitiveValue>(currentValue.get()), colorScheme);
    } else if (is<CSSPrimitiveValue>(value))
        updateColorScheme(downcast<CSSPrimitiveValue>(value), colorScheme);

    // If the value was just "only", that is synonymous for "only light".
    if (colorScheme.isOnly())
        colorScheme.add(ColorScheme::Light);

    return colorScheme;
}
#endif

inline String StyleBuilderConverter::convertString(Style::BuilderState&, const CSSValue& value)
{
    return downcast<CSSPrimitiveValue>(value).stringValue();
}

inline String StyleBuilderConverter::convertStringOrAuto(Style::BuilderState& builderState, const CSSValue& value)
{
    if (downcast<CSSPrimitiveValue>(value).valueID() == CSSValueAuto)
        return nullAtom();
    return convertString(builderState, value);
}

inline String StyleBuilderConverter::convertStringOrNone(Style::BuilderState& builderState, const CSSValue& value)
{
    if (downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone)
        return nullAtom();
    return convertString(builderState, value);
}

inline OptionSet<TextEmphasisPosition> StyleBuilderConverter::valueToEmphasisPosition(const CSSPrimitiveValue& primitiveValue)
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

inline OptionSet<TextEmphasisPosition> StyleBuilderConverter::convertTextEmphasisPosition(Style::BuilderState&, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value))
        return valueToEmphasisPosition(downcast<CSSPrimitiveValue>(value));

    OptionSet<TextEmphasisPosition> position;
    for (auto& currentValue : downcast<CSSValueList>(value))
        position.add(valueToEmphasisPosition(downcast<CSSPrimitiveValue>(currentValue.get())));
    return position;
}

inline TextAlignMode StyleBuilderConverter::convertTextAlign(Style::BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    ASSERT(primitiveValue.isValueID());

    if (primitiveValue.valueID() != CSSValueWebkitMatchParent)
        return primitiveValue;

    auto& parentStyle = builderState.parentStyle();
    if (parentStyle.textAlign() == TextAlignMode::Start)
        return parentStyle.isLeftToRightDirection() ? TextAlignMode::Left : TextAlignMode::Right;
    if (parentStyle.textAlign() == TextAlignMode::End)
        return parentStyle.isLeftToRightDirection() ? TextAlignMode::Right : TextAlignMode::Left;
    return parentStyle.textAlign();
}

inline RefPtr<ClipPathOperation> StyleBuilderConverter::convertClipPath(Style::BuilderState& builderState, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
        if (primitiveValue.primitiveType() == CSSPrimitiveValue::CSS_URI) {
            String cssURLValue = primitiveValue.stringValue();
            String fragment = SVGURIReference::fragmentIdentifierFromIRIString(cssURLValue, builderState.document());
            // FIXME: It doesn't work with external SVG references (see https://bugs.webkit.org/show_bug.cgi?id=126133)
            return ReferenceClipPathOperation::create(cssURLValue, fragment);
        }
        ASSERT(primitiveValue.valueID() == CSSValueNone);
        return nullptr;
    }

    CSSBoxType referenceBox = CSSBoxType::BoxMissing;
    RefPtr<ClipPathOperation> operation;

    for (auto& currentValue : downcast<CSSValueList>(value)) {
        auto& primitiveValue = downcast<CSSPrimitiveValue>(currentValue.get());
        if (primitiveValue.isShape()) {
            ASSERT(!operation);
            operation = ShapeClipPathOperation::create(basicShapeForValue(builderState.cssToLengthConversionData(), *primitiveValue.shapeValue()));
        } else {
            ASSERT(primitiveValue.valueID() == CSSValueContentBox
                || primitiveValue.valueID() == CSSValueBorderBox
                || primitiveValue.valueID() == CSSValuePaddingBox
                || primitiveValue.valueID() == CSSValueMarginBox
                || primitiveValue.valueID() == CSSValueFillBox
                || primitiveValue.valueID() == CSSValueStrokeBox
                || primitiveValue.valueID() == CSSValueViewBox);
            ASSERT(referenceBox == CSSBoxType::BoxMissing);
            referenceBox = primitiveValue;
        }
    }
    if (operation)
        downcast<ShapeClipPathOperation>(*operation).setReferenceBox(referenceBox);
    else {
        ASSERT(referenceBox != CSSBoxType::BoxMissing);
        operation = BoxClipPathOperation::create(referenceBox);
    }

    return operation;
}

inline Resize StyleBuilderConverter::convertResize(Style::BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);

    Resize resize = Resize::None;
    if (primitiveValue.valueID() == CSSValueAuto)
        resize = builderState.document().settings().textAreasAreResizable() ? Resize::Both : Resize::None;
    else
        resize = primitiveValue;

    return resize;
}

inline int StyleBuilderConverter::convertMarqueeRepetition(Style::BuilderState&, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueInfinite)
        return -1; // -1 means repeat forever.

    ASSERT(primitiveValue.isNumber());
    return primitiveValue.intValue();
}

inline int StyleBuilderConverter::convertMarqueeSpeed(Style::BuilderState&, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    int speed = 85;
    if (CSSValueID ident = primitiveValue.valueID()) {
        switch (ident) {
        case CSSValueSlow:
            speed = 500; // 500 msec.
            break;
        case CSSValueNormal:
            speed = 85; // 85msec. The WinIE default.
            break;
        case CSSValueFast:
            speed = 10; // 10msec. Super fast.
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    } else if (primitiveValue.isTime())
        speed = primitiveValue.computeTime<int, CSSPrimitiveValue::Milliseconds>();
    else {
        // For scrollamount support.
        ASSERT(primitiveValue.isNumber());
        speed = primitiveValue.intValue();
    }
    return speed;
}

inline Ref<QuotesData> StyleBuilderConverter::convertQuotes(Style::BuilderState&, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone);
        return QuotesData::create(Vector<std::pair<String, String>>());
    }

    auto& list = downcast<CSSValueList>(value);
    Vector<std::pair<String, String>> quotes;
    quotes.reserveInitialCapacity(list.length() / 2);
    for (unsigned i = 0; i < list.length(); i += 2) {
        const CSSValue* first = list.itemWithoutBoundsCheck(i);
        // item() returns null if out of bounds so this is safe.
        const CSSValue* second = list.item(i + 1);
        if (!second)
            break;
        String startQuote = downcast<CSSPrimitiveValue>(*first).stringValue();
        String endQuote = downcast<CSSPrimitiveValue>(*second).stringValue();
        quotes.append(std::make_pair(startQuote, endQuote));
    }
    return QuotesData::create(quotes);
}

inline TextUnderlinePosition StyleBuilderConverter::convertTextUnderlinePosition(Style::BuilderState&, const CSSValue& value)
{
    ASSERT(is<CSSPrimitiveValue>(value));
    return downcast<CSSPrimitiveValue>(value);
}

inline TextUnderlineOffset StyleBuilderConverter::convertTextUnderlineOffset(Style::BuilderState& builderState, const CSSValue& value)
{
    ASSERT(is<CSSPrimitiveValue>(value));
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    switch (primitiveValue.valueID()) {
    case CSSValueAuto:
        return TextUnderlineOffset::createWithAuto();
    default:
        ASSERT(primitiveValue.isLength());
        auto computedLength = convertComputedLength<float>(builderState, primitiveValue);
        return TextUnderlineOffset::createWithLength(computedLength);
    }
}

inline TextDecorationThickness StyleBuilderConverter::convertTextDecorationThickness(Style::BuilderState& builderState, const CSSValue& value)
{
    ASSERT(is<CSSPrimitiveValue>(value));
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    switch (primitiveValue.valueID()) {
    case CSSValueAuto:
        return TextDecorationThickness::createWithAuto();
    case CSSValueFromFont:
        return TextDecorationThickness::createFromFont();
    default:
        ASSERT(primitiveValue.isLength());
        auto computedLength = convertComputedLength<float>(builderState, primitiveValue);
        return TextDecorationThickness::createWithLength(computedLength);
    }
}

inline RefPtr<StyleReflection> StyleBuilderConverter::convertReflection(Style::BuilderState& builderState, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone);
        return nullptr;
    }

    auto& reflectValue = downcast<CSSReflectValue>(value);

    auto reflection = StyleReflection::create();
    reflection->setDirection(reflectValue.direction());
    reflection->setOffset(reflectValue.offset().convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion>(builderState.cssToLengthConversionData()));

    NinePieceImage mask(NinePieceImage::Type::Mask);
    builderState.styleMap().mapNinePieceImage(CSSPropertyWebkitBoxReflect, reflectValue.mask(), mask);
    reflection->setMask(mask);

    return reflection;
}

inline IntSize StyleBuilderConverter::convertInitialLetter(Style::BuilderState&, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);

    if (primitiveValue.valueID() == CSSValueNormal)
        return IntSize();

    Pair* pair = primitiveValue.pairValue();
    ASSERT(pair);
    ASSERT(pair->first());
    ASSERT(pair->second());

    return IntSize(pair->first()->intValue(), pair->second()->intValue());
}

inline float StyleBuilderConverter::convertTextStrokeWidth(Style::BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);

    float width = 0;
    switch (primitiveValue.valueID()) {
    case CSSValueThin:
    case CSSValueMedium:
    case CSSValueThick: {
        double result = 1.0 / 48;
        if (primitiveValue.valueID() == CSSValueMedium)
            result *= 3;
        else if (primitiveValue.valueID() == CSSValueThick)
            result *= 5;
        Ref<CSSPrimitiveValue> emsValue(CSSPrimitiveValue::create(result, CSSPrimitiveValue::CSS_EMS));
        width = convertComputedLength<float>(builderState, emsValue);
        break;
    }
    case CSSValueInvalid: {
        width = convertComputedLength<float>(builderState, primitiveValue);
        break;
    }
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }

    return width;
}

inline OptionSet<LineBoxContain> StyleBuilderConverter::convertLineBoxContain(Style::BuilderState&, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone);
        return { };
    }

    return downcast<CSSLineBoxContainValue>(value).value();
}

inline OptionSet<TextDecorationSkip> StyleBuilderConverter::valueToDecorationSkip(const CSSPrimitiveValue& primitiveValue)
{
    ASSERT(primitiveValue.isValueID());

    switch (primitiveValue.valueID()) {
    case CSSValueAuto:
        return TextDecorationSkip::Auto;
    case CSSValueNone:
        return OptionSet<TextDecorationSkip> { };
    case CSSValueInk:
        return TextDecorationSkip::Ink;
    case CSSValueObjects:
        return TextDecorationSkip::Objects;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return OptionSet<TextDecorationSkip> { };
}

inline OptionSet<TextDecorationSkip> StyleBuilderConverter::convertTextDecorationSkip(Style::BuilderState&, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value))
        return valueToDecorationSkip(downcast<CSSPrimitiveValue>(value));

    OptionSet<TextDecorationSkip> skip;
    for (auto& currentValue : downcast<CSSValueList>(value))
        skip.add(valueToDecorationSkip(downcast<CSSPrimitiveValue>(currentValue.get())));
    return skip;
}

static inline bool isImageShape(const CSSValue& value)
{
    return is<CSSImageValue>(value) || is<CSSImageSetValue>(value) || is<CSSImageGeneratorValue>(value);
}

inline RefPtr<ShapeValue> StyleBuilderConverter::convertShapeValue(Style::BuilderState& builderState, CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone);
        return nullptr;
    }

    if (isImageShape(value))
        return ShapeValue::create(builderState.createStyleImage(value).releaseNonNull());

    RefPtr<BasicShape> shape;
    CSSBoxType referenceBox = CSSBoxType::BoxMissing;
    for (auto& currentValue : downcast<CSSValueList>(value)) {
        auto& primitiveValue = downcast<CSSPrimitiveValue>(currentValue.get());
        if (primitiveValue.isShape())
            shape = basicShapeForValue(builderState.cssToLengthConversionData(), *primitiveValue.shapeValue());
        else if (primitiveValue.valueID() == CSSValueContentBox
            || primitiveValue.valueID() == CSSValueBorderBox
            || primitiveValue.valueID() == CSSValuePaddingBox
            || primitiveValue.valueID() == CSSValueMarginBox)
            referenceBox = primitiveValue;
        else {
            ASSERT_NOT_REACHED();
            return nullptr;
        }
    }

    if (shape)
        return ShapeValue::create(shape.releaseNonNull(), referenceBox);

    if (referenceBox != CSSBoxType::BoxMissing)
        return ShapeValue::create(referenceBox);

    ASSERT_NOT_REACHED();
    return nullptr;
}

#if ENABLE(CSS_SCROLL_SNAP)

inline ScrollSnapType StyleBuilderConverter::convertScrollSnapType(Style::BuilderState&, const CSSValue& value)
{
    ScrollSnapType type;
    auto& values = downcast<CSSValueList>(value);
    auto& firstValue = downcast<CSSPrimitiveValue>(*values.item(0));
    if (values.length() == 2) {
        type.axis = firstValue;
        type.strictness = downcast<CSSPrimitiveValue>(*values.item(1));
        return type;
    }

    switch (firstValue.valueID()) {
    case CSSValueNone:
    case CSSValueMandatory:
    case CSSValueProximity:
        type.strictness = firstValue;
        break;
    default:
        type.axis = firstValue;
        type.strictness = ScrollSnapStrictness::Proximity;
        break;
    }
    return type;
}

inline ScrollSnapAlign StyleBuilderConverter::convertScrollSnapAlign(Style::BuilderState&, const CSSValue& value)
{
    auto& values = downcast<CSSValueList>(value);
    ScrollSnapAlign alignment;
    alignment.x = downcast<CSSPrimitiveValue>(*values.item(0));
    if (values.length() == 1)
        alignment.y = alignment.x;
    else
        alignment.y = downcast<CSSPrimitiveValue>(*values.item(1));
    return alignment;
}

#endif

inline GridLength StyleBuilderConverter::createGridTrackBreadth(const CSSPrimitiveValue& primitiveValue, Style::BuilderState& builderState)
{
    if (primitiveValue.valueID() == CSSValueMinContent || primitiveValue.valueID() == CSSValueWebkitMinContent)
        return Length(MinContent);

    if (primitiveValue.valueID() == CSSValueMaxContent || primitiveValue.valueID() == CSSValueWebkitMaxContent)
        return Length(MaxContent);

    // Fractional unit.
    if (primitiveValue.isFlex())
        return GridLength(primitiveValue.doubleValue());

    return primitiveValue.convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion | AutoConversion>(builderState.cssToLengthConversionData());
}

inline GridTrackSize StyleBuilderConverter::createGridTrackSize(const CSSValue& value, Style::BuilderState& builderState)
{
    if (is<CSSPrimitiveValue>(value))
        return GridTrackSize(createGridTrackBreadth(downcast<CSSPrimitiveValue>(value), builderState));

    ASSERT(is<CSSFunctionValue>(value));
    const auto& function = downcast<CSSFunctionValue>(value);

    if (function.length() == 1)
        return GridTrackSize(createGridTrackBreadth(downcast<CSSPrimitiveValue>(*function.itemWithoutBoundsCheck(0)), builderState), FitContentTrackSizing);

    ASSERT_WITH_SECURITY_IMPLICATION(function.length() == 2);
    GridLength minTrackBreadth(createGridTrackBreadth(downcast<CSSPrimitiveValue>(*function.itemWithoutBoundsCheck(0)), builderState));
    GridLength maxTrackBreadth(createGridTrackBreadth(downcast<CSSPrimitiveValue>(*function.itemWithoutBoundsCheck(1)), builderState));
    return GridTrackSize(minTrackBreadth, maxTrackBreadth);
}

static void createGridLineNamesList(const CSSValue& value, unsigned currentNamedGridLine, NamedGridLinesMap& namedGridLines, OrderedNamedGridLinesMap& orderedNamedGridLines)
{
    ASSERT(value.isGridLineNamesValue());

    for (auto& namedGridLineValue : downcast<CSSGridLineNamesValue>(value)) {
        String namedGridLine = downcast<CSSPrimitiveValue>(namedGridLineValue.get()).stringValue();
        auto result = namedGridLines.add(namedGridLine, Vector<unsigned>());
        result.iterator->value.append(currentNamedGridLine);
        auto orderedResult = orderedNamedGridLines.add(currentNamedGridLine, Vector<String>());
        orderedResult.iterator->value.append(namedGridLine);
    }
}

struct StyleBuilderConverter::TracksData {
    WTF_MAKE_NONCOPYABLE(TracksData); WTF_MAKE_FAST_ALLOCATED;
public:
    TracksData() = default;

    Vector<GridTrackSize> m_trackSizes;
    NamedGridLinesMap m_namedGridLines;
    OrderedNamedGridLinesMap m_orderedNamedGridLines;
    Vector<GridTrackSize> m_autoRepeatTrackSizes;
    NamedGridLinesMap m_autoRepeatNamedGridLines;
    OrderedNamedGridLinesMap m_autoRepeatOrderedNamedGridLines;
    unsigned m_autoRepeatInsertionPoint { RenderStyle::initialGridAutoRepeatInsertionPoint() };
    AutoRepeatType m_autoRepeatType { RenderStyle::initialGridAutoRepeatType() };
};

inline bool StyleBuilderConverter::createGridTrackList(const CSSValue& value, TracksData& tracksData, Style::BuilderState& builderState)
{
    // Handle 'none'.
    if (is<CSSPrimitiveValue>(value))
        return downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone;

    if (!is<CSSValueList>(value))
        return false;

    unsigned currentNamedGridLine = 0;
    auto handleLineNameOrTrackSize = [&](const CSSValue& currentValue) {
        if (is<CSSGridLineNamesValue>(currentValue))
            createGridLineNamesList(currentValue, currentNamedGridLine, tracksData.m_namedGridLines, tracksData.m_orderedNamedGridLines);
        else {
            ++currentNamedGridLine;
            tracksData.m_trackSizes.append(createGridTrackSize(currentValue, builderState));
        }
    };

    for (auto& currentValue : downcast<CSSValueList>(value)) {
        if (is<CSSGridAutoRepeatValue>(currentValue)) {
            ASSERT(tracksData.m_autoRepeatTrackSizes.isEmpty());
            unsigned autoRepeatIndex = 0;
            CSSValueID autoRepeatID = downcast<CSSGridAutoRepeatValue>(currentValue.get()).autoRepeatID();
            ASSERT(autoRepeatID == CSSValueAutoFill || autoRepeatID == CSSValueAutoFit);
            tracksData.m_autoRepeatType = autoRepeatID == CSSValueAutoFill ? AutoRepeatType::Fill : AutoRepeatType::Fit;
            for (auto& autoRepeatValue : downcast<CSSValueList>(currentValue.get())) {
                if (is<CSSGridLineNamesValue>(autoRepeatValue)) {
                    createGridLineNamesList(autoRepeatValue.get(), autoRepeatIndex, tracksData.m_autoRepeatNamedGridLines, tracksData.m_autoRepeatOrderedNamedGridLines);
                    continue;
                }
                ++autoRepeatIndex;
                tracksData.m_autoRepeatTrackSizes.append(createGridTrackSize(autoRepeatValue.get(), builderState));
            }
            tracksData.m_autoRepeatInsertionPoint = currentNamedGridLine++;
            continue;
        }

        if (is<CSSGridIntegerRepeatValue>(currentValue)) {
            size_t repetitions = downcast<CSSGridIntegerRepeatValue>(currentValue.get()).repetitions();
            for (size_t i = 0; i < repetitions; ++i) {
                for (auto& integerRepeatValue : downcast<CSSValueList>(currentValue.get()))
                    handleLineNameOrTrackSize(integerRepeatValue);
            }
            continue;
        }

        handleLineNameOrTrackSize(currentValue);
    }

    // The parser should have rejected any <track-list> without any <track-size> as
    // this is not conformant to the syntax.
    ASSERT(!tracksData.m_trackSizes.isEmpty() || !tracksData.m_autoRepeatTrackSizes.isEmpty());
    return true;
}

inline bool StyleBuilderConverter::createGridPosition(const CSSValue& value, GridPosition& position)
{
    // We accept the specification's grammar:
    // auto | <custom-ident> | [ <integer> && <custom-ident>? ] | [ span && [ <integer> || <custom-ident> ] ]
    if (is<CSSPrimitiveValue>(value)) {
        auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
        // We translate <ident> to <string> during parsing as it makes handling it simpler.
        if (primitiveValue.isString()) {
            position.setNamedGridArea(primitiveValue.stringValue());
            return true;
        }

        ASSERT(primitiveValue.valueID() == CSSValueAuto);
        return true;
    }

    auto& values = downcast<CSSValueList>(value);
    ASSERT(values.length());

    auto it = values.begin();
    const CSSPrimitiveValue* currentValue = &downcast<CSSPrimitiveValue>(it->get());
    bool isSpanPosition = false;
    if (currentValue->valueID() == CSSValueSpan) {
        isSpanPosition = true;
        ++it;
        currentValue = it != values.end() ? &downcast<CSSPrimitiveValue>(it->get()) : nullptr;
    }

    int gridLineNumber = 0;
    if (currentValue && currentValue->isNumber()) {
        gridLineNumber = currentValue->intValue();
        ++it;
        currentValue = it != values.end() ? &downcast<CSSPrimitiveValue>(it->get()) : nullptr;
    }

    String gridLineName;
    if (currentValue && currentValue->isString()) {
        gridLineName = currentValue->stringValue();
        ++it;
    }

    ASSERT(it == values.end());
    if (isSpanPosition)
        position.setSpanPosition(gridLineNumber ? gridLineNumber : 1, gridLineName);
    else
        position.setExplicitPosition(gridLineNumber, gridLineName);

    return true;
}

inline void StyleBuilderConverter::createImplicitNamedGridLinesFromGridArea(const NamedGridAreaMap& namedGridAreas, NamedGridLinesMap& namedGridLines, GridTrackSizingDirection direction)
{
    for (auto& area : namedGridAreas) {
        GridSpan areaSpan = direction == ForRows ? area.value.rows : area.value.columns;
        {
            auto& startVector = namedGridLines.add(area.key + "-start", Vector<unsigned>()).iterator->value;
            startVector.append(areaSpan.startLine());
            std::sort(startVector.begin(), startVector.end());
        }
        {
            auto& endVector = namedGridLines.add(area.key + "-end", Vector<unsigned>()).iterator->value;
            endVector.append(areaSpan.endLine());
            std::sort(endVector.begin(), endVector.end());
        }
    }
}

inline Vector<GridTrackSize> StyleBuilderConverter::convertGridTrackSizeList(Style::BuilderState& builderState, const CSSValue& value)
{
    ASSERT(value.isValueList());
    auto& valueList = downcast<CSSValueList>(value);
    Vector<GridTrackSize> trackSizes;
    trackSizes.reserveInitialCapacity(valueList.length());
    for (auto& currValue : valueList) {
        ASSERT(!currValue->isGridLineNamesValue());
        ASSERT(!currValue->isGridAutoRepeatValue());
        ASSERT(!currValue->isGridIntegerRepeatValue());
        trackSizes.uncheckedAppend(convertGridTrackSize(builderState, currValue));
    }
    return trackSizes;
}

inline GridTrackSize StyleBuilderConverter::convertGridTrackSize(Style::BuilderState& builderState, const CSSValue& value)
{
    return createGridTrackSize(value, builderState);
}

inline Optional<GridPosition> StyleBuilderConverter::convertGridPosition(Style::BuilderState&, const CSSValue& value)
{
    GridPosition gridPosition;
    if (createGridPosition(value, gridPosition))
        return gridPosition;
    return WTF::nullopt;
}

inline GridAutoFlow StyleBuilderConverter::convertGridAutoFlow(Style::BuilderState&, const CSSValue& value)
{
    auto& list = downcast<CSSValueList>(value);
    if (!list.length())
        return RenderStyle::initialGridAutoFlow();

    auto& first = downcast<CSSPrimitiveValue>(*list.item(0));
    auto* second = downcast<CSSPrimitiveValue>(list.item(1));

    GridAutoFlow autoFlow;
    switch (first.valueID()) {
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

inline CSSToLengthConversionData StyleBuilderConverter::csstoLengthConversionDataWithTextZoomFactor(Style::BuilderState& builderState)
{
    if (auto* frame = builderState.document().frame()) {
        float textZoomFactor = builderState.style().textZoom() != TextZoom::Reset ? frame->textZoomFactor() : 1.0f;
        return builderState.cssToLengthConversionData().copyWithAdjustedZoom(builderState.style().effectiveZoom() * textZoomFactor);
    }
    return builderState.cssToLengthConversionData();
}

inline Optional<Length> StyleBuilderConverter::convertWordSpacing(Style::BuilderState& builderState, const CSSValue& value)
{
    Optional<Length> wordSpacing;
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueNormal)
        wordSpacing = RenderStyle::initialWordSpacing();
    else if (primitiveValue.isLength())
        wordSpacing = primitiveValue.computeLength<Length>(csstoLengthConversionDataWithTextZoomFactor(builderState));
    else if (primitiveValue.isPercentage())
        wordSpacing = Length(clampTo<float>(primitiveValue.doubleValue(), minValueForCssLength, maxValueForCssLength), Percent);
    else if (primitiveValue.isNumber())
        wordSpacing = Length(primitiveValue.doubleValue(), Fixed);

    return wordSpacing;
}

inline Optional<float> StyleBuilderConverter::convertPerspective(Style::BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueNone)
        return 0.f;

    float perspective = -1;
    if (primitiveValue.isLength())
        perspective = primitiveValue.computeLength<float>(builderState.cssToLengthConversionData());
    else if (primitiveValue.isNumber())
        perspective = primitiveValue.doubleValue() * builderState.cssToLengthConversionData().zoom();
    else
        ASSERT_NOT_REACHED();

    return perspective < 0 ? Optional<float>(WTF::nullopt) : Optional<float>(perspective);
}

inline Optional<Length> StyleBuilderConverter::convertMarqueeIncrement(Style::BuilderState& builderState, const CSSValue& value)
{
    Optional<Length> marqueeLength;
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    switch (primitiveValue.valueID()) {
    case CSSValueSmall:
        marqueeLength = Length(1, Fixed); // 1px.
        break;
    case CSSValueNormal:
        marqueeLength = Length(6, Fixed); // 6px. The WinIE default.
        break;
    case CSSValueLarge:
        marqueeLength = Length(36, Fixed); // 36px.
        break;
    case CSSValueInvalid: {
        Length length = primitiveValue.convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion>(builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f));
        if (!length.isUndefined())
            marqueeLength = length;
        break;
    }
    default:
        break;
    }
    return marqueeLength;
}

inline Optional<FilterOperations> StyleBuilderConverter::convertFilterOperations(Style::BuilderState& builderState, const CSSValue& value)
{
    FilterOperations operations;
    if (builderState.createFilterOperations(value, operations))
        return operations;
    return WTF::nullopt;
}

inline FontFeatureSettings StyleBuilderConverter::convertFontFeatureSettings(Style::BuilderState&, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNormal);
        return { };
    }

    FontFeatureSettings settings;
    for (auto& item : downcast<CSSValueList>(value)) {
        auto& feature = downcast<CSSFontFeatureValue>(item.get());
        settings.insert(FontFeature(feature.tag(), feature.value()));
    }
    return settings;
}

inline FontSelectionValue StyleBuilderConverter::convertFontWeightFromValue(const CSSValue& value)
{
    ASSERT(is<CSSPrimitiveValue>(value));
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);

    if (primitiveValue.isNumber())
        return FontSelectionValue::clampFloat(primitiveValue.floatValue());

    ASSERT(primitiveValue.isValueID());
    switch (primitiveValue.valueID()) {
    case CSSValueNormal:
        return normalWeightValue();
    case CSSValueBold:
    case CSSValueBolder:
        return boldWeightValue();
    case CSSValueLighter:
        return lightWeightValue();
    default:
        ASSERT_NOT_REACHED();
        return normalWeightValue();
    }
}

inline FontSelectionValue StyleBuilderConverter::convertFontStretchFromValue(const CSSValue& value)
{
    ASSERT(is<CSSPrimitiveValue>(value));
    const auto& primitiveValue = downcast<CSSPrimitiveValue>(value);

    if (primitiveValue.isPercentage())
        return FontSelectionValue::clampFloat(primitiveValue.floatValue());

    ASSERT(primitiveValue.isValueID());
    if (auto value = fontStretchValue(primitiveValue.valueID()))
        return value.value();
    ASSERT_NOT_REACHED();
    return normalStretchValue();
}

// The input value needs to parsed and valid, this function returns WTF::nullopt if the input was "normal".
inline Optional<FontSelectionValue> StyleBuilderConverter::convertFontStyleFromValue(const CSSValue& value)
{
    ASSERT(is<CSSFontStyleValue>(value));
    const auto& fontStyleValue = downcast<CSSFontStyleValue>(value);

    auto valueID = fontStyleValue.fontStyleValue->valueID();
    if (valueID == CSSValueNormal)
        return WTF::nullopt;
    if (valueID == CSSValueItalic)
        return italicValue();
    ASSERT(valueID == CSSValueOblique);
    if (auto* obliqueValue = fontStyleValue.obliqueValue.get())
        return FontSelectionValue(obliqueValue->value<float>(CSSPrimitiveValue::CSS_DEG));
    return italicValue();
}

inline FontSelectionValue StyleBuilderConverter::convertFontWeight(Style::BuilderState& builderState, const CSSValue& value)
{
    ASSERT(is<CSSPrimitiveValue>(value));
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.isValueID()) {
        auto valueID = primitiveValue.valueID();
        if (valueID == CSSValueBolder)
            return FontCascadeDescription::bolderWeight(builderState.parentStyle().fontDescription().weight());
        if (valueID == CSSValueLighter)
            return FontCascadeDescription::lighterWeight(builderState.parentStyle().fontDescription().weight());
    }
    return convertFontWeightFromValue(value);
}

inline FontSelectionValue StyleBuilderConverter::convertFontStretch(Style::BuilderState&, const CSSValue& value)
{
    return convertFontStretchFromValue(value);
}

#if ENABLE(VARIATION_FONTS)
inline FontVariationSettings StyleBuilderConverter::convertFontVariationSettings(Style::BuilderState&, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNormal);
        return { };
    }

    FontVariationSettings settings;
    for (auto& item : downcast<CSSValueList>(value)) {
        auto& feature = downcast<CSSFontVariationValue>(item.get());
        settings.insert({ feature.tag(), feature.value() });
    }
    return settings;
}
#endif

#if PLATFORM(IOS_FAMILY)
inline bool StyleBuilderConverter::convertTouchCallout(Style::BuilderState&, const CSSValue& value)
{
    return !equalLettersIgnoringASCIICase(downcast<CSSPrimitiveValue>(value).stringValue(), "none");
}
#endif

#if ENABLE(TOUCH_EVENTS)
inline Color StyleBuilderConverter::convertTapHighlightColor(Style::BuilderState& builderState, const CSSValue& value)
{
    return builderState.colorFromPrimitiveValue(downcast<CSSPrimitiveValue>(value));
}
#endif

#if ENABLE(POINTER_EVENTS)
inline OptionSet<TouchAction> StyleBuilderConverter::convertTouchAction(Style::BuilderState&, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value))
        return downcast<CSSPrimitiveValue>(value);

    if (is<CSSValueList>(value)) {
        OptionSet<TouchAction> touchActions;
        for (auto& currentValue : downcast<CSSValueList>(value)) {
            auto& primitiveValue = downcast<CSSPrimitiveValue>(currentValue.get());
            auto primitiveValueID = primitiveValue.valueID();
            if (primitiveValueID != CSSValuePanX && primitiveValueID != CSSValuePanY && primitiveValueID != CSSValuePinchZoom)
                return RenderStyle::initialTouchActions();
            touchActions.add(primitiveValue);
        }
        return touchActions;
    }

    return RenderStyle::initialTouchActions();
}
#endif

#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
inline bool StyleBuilderConverter::convertOverflowScrolling(Style::BuilderState&, const CSSValue& value)
{
    return downcast<CSSPrimitiveValue>(value).valueID() == CSSValueTouch;
}
#endif

inline SVGLengthValue StyleBuilderConverter::convertSVGLengthValue(Style::BuilderState&, const CSSValue& value)
{
    return SVGLengthValue::fromCSSPrimitiveValue(downcast<CSSPrimitiveValue>(value));
}

inline Vector<SVGLengthValue> StyleBuilderConverter::convertSVGLengthVector(Style::BuilderState& builderState, const CSSValue& value)
{
    auto& valueList = downcast<CSSValueList>(value);

    Vector<SVGLengthValue> svgLengths;
    svgLengths.reserveInitialCapacity(valueList.length());
    for (auto& item : valueList)
        svgLengths.uncheckedAppend(convertSVGLengthValue(builderState, item));

    return svgLengths;
}

inline Vector<SVGLengthValue> StyleBuilderConverter::convertStrokeDashArray(Style::BuilderState& builderState, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone);
        return SVGRenderStyle::initialStrokeDashArray();
    }

    return convertSVGLengthVector(builderState, value);
}

inline PaintOrder StyleBuilderConverter::convertPaintOrder(Style::BuilderState&, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNormal);
        return PaintOrder::Normal;
    }

    auto& orderTypeList = downcast<CSSValueList>(value);
    switch (downcast<CSSPrimitiveValue>(*orderTypeList.itemWithoutBoundsCheck(0)).valueID()) {
    case CSSValueFill:
        return orderTypeList.length() > 1 ? PaintOrder::FillMarkers : PaintOrder::Fill;
    case CSSValueStroke:
        return orderTypeList.length() > 1 ? PaintOrder::StrokeMarkers : PaintOrder::Stroke;
    case CSSValueMarkers:
        return orderTypeList.length() > 1 ? PaintOrder::MarkersStroke : PaintOrder::Markers;
    default:
        ASSERT_NOT_REACHED();
        return PaintOrder::Normal;
    }
}

inline float StyleBuilderConverter::convertOpacity(Style::BuilderState&, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    float opacity = primitiveValue.floatValue();
    if (primitiveValue.isPercentage())
        opacity /= 100.0f;
    return std::max(0.0f, std::min(1.0f, opacity));
}

inline String StyleBuilderConverter::convertSVGURIReference(Style::BuilderState& builderState, const CSSValue& value)
{
    String s;
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.isURI())
        s = primitiveValue.stringValue();

    return SVGURIReference::fragmentIdentifierFromIRIString(s, builderState.document());
}

inline Color StyleBuilderConverter::convertSVGColor(Style::BuilderState& builderState, const CSSValue& value)
{
    return builderState.colorFromPrimitiveValue(downcast<CSSPrimitiveValue>(value));
}

inline StyleSelfAlignmentData StyleBuilderConverter::convertSelfOrDefaultAlignmentData(Style::BuilderState&, const CSSValue& value)
{
    StyleSelfAlignmentData alignmentData = RenderStyle::initialSelfAlignment();
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (Pair* pairValue = primitiveValue.pairValue()) {
        if (pairValue->first()->valueID() == CSSValueLegacy) {
            alignmentData.setPositionType(ItemPositionType::Legacy);
            alignmentData.setPosition(*pairValue->second());
        } else if (pairValue->first()->valueID() == CSSValueFirst) {
            alignmentData.setPosition(ItemPosition::Baseline);
        } else if (pairValue->first()->valueID() == CSSValueLast) {
            alignmentData.setPosition(ItemPosition::LastBaseline);
        } else {
            alignmentData.setOverflow(*pairValue->first());
            alignmentData.setPosition(*pairValue->second());
        }
    } else
        alignmentData.setPosition(primitiveValue);
    return alignmentData;
}

inline StyleContentAlignmentData StyleBuilderConverter::convertContentAlignmentData(Style::BuilderState&, const CSSValue& value)
{
    StyleContentAlignmentData alignmentData = RenderStyle::initialContentAlignment();
    if (!is<CSSContentDistributionValue>(value))
        return alignmentData;
    auto& contentValue = downcast<CSSContentDistributionValue>(value);
    if (contentValue.distribution()->valueID() != CSSValueInvalid)
        alignmentData.setDistribution(contentValue.distribution().get());
    if (contentValue.position()->valueID() != CSSValueInvalid)
        alignmentData.setPosition(contentValue.position().get());
    if (contentValue.overflow()->valueID() != CSSValueInvalid)
        alignmentData.setOverflow(contentValue.overflow().get());
    return alignmentData;
}

inline GlyphOrientation StyleBuilderConverter::convertGlyphOrientation(Style::BuilderState&, const CSSValue& value)
{
    float angle = fabsf(fmodf(downcast<CSSPrimitiveValue>(value).floatValue(), 360.0f));
    if (angle <= 45.0f || angle > 315.0f)
        return GlyphOrientation::Degrees0;
    if (angle > 45.0f && angle <= 135.0f)
        return GlyphOrientation::Degrees90;
    if (angle > 135.0f && angle <= 225.0f)
        return GlyphOrientation::Degrees180;
    return GlyphOrientation::Degrees270;
}

inline GlyphOrientation StyleBuilderConverter::convertGlyphOrientationOrAuto(Style::BuilderState& builderState, const CSSValue& value)
{
    if (downcast<CSSPrimitiveValue>(value).valueID() == CSSValueAuto)
        return GlyphOrientation::Auto;
    return convertGlyphOrientation(builderState, value);
}

inline Optional<Length> StyleBuilderConverter::convertLineHeight(Style::BuilderState& builderState, const CSSValue& value, float multiplier)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueNormal)
        return RenderStyle::initialLineHeight();

    if (primitiveValue.isLength()) {
        Length length = primitiveValue.computeLength<Length>(StyleBuilderConverter::csstoLengthConversionDataWithTextZoomFactor(builderState));
        if (multiplier != 1.f)
            length = Length(length.value() * multiplier, Fixed);
        return length;
    }

    // Line-height percentages need to inherit as if they were Fixed pixel values. In the example:
    // <div style="font-size: 10px; line-height: 150%;"><div style="font-size: 100px;"></div></div>
    // the inner element should have line-height of 15px. However, in this example:
    // <div style="font-size: 10px; line-height: 1.5;"><div style="font-size: 100px;"></div></div>
    // the inner element should have a line-height of 150px. Therefore, we map percentages to Fixed
    // values and raw numbers to percentages.
    if (primitiveValue.isPercentage()) {
        // FIXME: percentage should not be restricted to an integer here.
        return Length((builderState.style().computedFontSize() * primitiveValue.intValue()) / 100, Fixed);
    }
    if (primitiveValue.isNumber())
        return Length(primitiveValue.doubleValue() * 100.0, Percent);

    // FIXME: The parser should only emit the above types, so this should never be reached. We should change the
    // type of this function to return just a Length (and not an Optional).
    return WTF::nullopt;
}

inline FontSynthesis StyleBuilderConverter::convertFontSynthesis(Style::BuilderState&, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone);
        return FontSynthesisNone;
    }

    FontSynthesis result = FontSynthesisNone;
    ASSERT(is<CSSValueList>(value));
    for (const CSSValue& v : downcast<CSSValueList>(value)) {
        switch (downcast<CSSPrimitiveValue>(v).valueID()) {
        case CSSValueWeight:
            result |= FontSynthesisWeight;
            break;
        case CSSValueStyle:
            result |= FontSynthesisStyle;
            break;
        case CSSValueSmallCaps:
            result |= FontSynthesisSmallCaps;
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    return result;
}
    
inline OptionSet<SpeakAs> StyleBuilderConverter::convertSpeakAs(Style::BuilderState&, const CSSValue& value)
{
    auto result = RenderStyle::initialSpeakAs();
    if (is<CSSValueList>(value)) {
        for (auto& currentValue : downcast<CSSValueList>(value))
            result.add(downcast<CSSPrimitiveValue>(currentValue.get()));
    }
    return result;
}

inline OptionSet<HangingPunctuation> StyleBuilderConverter::convertHangingPunctuation(Style::BuilderState&, const CSSValue& value)
{
    auto result = RenderStyle::initialHangingPunctuation();
    if (is<CSSValueList>(value)) {
        for (auto& currentValue : downcast<CSSValueList>(value))
            result.add(downcast<CSSPrimitiveValue>(currentValue.get()));
    }
    return result;
}

inline GapLength StyleBuilderConverter::convertGapLength(Style::BuilderState& builderState, const CSSValue& value)
{
    return (downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNormal) ? GapLength() : GapLength(convertLength(builderState, value));
}

} // namespace WebCore
