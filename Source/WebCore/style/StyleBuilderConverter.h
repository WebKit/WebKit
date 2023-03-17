/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2014-2022 Apple Inc. All rights reserved.
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
#include "CSSCalcValue.h"
#include "CSSContentDistributionValue.h"
#include "CSSCustomIdentValue.h"
#include "CSSFontFeatureValue.h"
#include "CSSFontStyleWithAngleValue.h"
#include "CSSFontVariationValue.h"
#include "CSSFunctionValue.h"
#include "CSSGridAutoRepeatValue.h"
#include "CSSGridIntegerRepeatValue.h"
#include "CSSGridLineNamesValue.h"
#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
#include "CSSOffsetRotateValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyParserHelpers.h"
#include "CSSRayValue.h"
#include "CSSReflectValue.h"
#include "CSSStringValue.h"
#include "CSSSubgridValue.h"
#include "CSSURLValue.h"
#include "CSSValuePair.h"
#include "CalcExpressionLength.h"
#include "CalcExpressionOperation.h"
#include "CalculationValue.h"
#include "FontPalette.h"
#include "FontSelectionValueInlines.h"
#include "FrameDestructionObserverInlines.h"
#include "GridPositionsResolver.h"
#include "Length.h"
#include "ListStyleType.h"
#include "LocalFrame.h"
#include "QuotesData.h"
#include "SVGElementTypeHelpers.h"
#include "SVGPathElement.h"
#include "SVGURIReference.h"
#include "Settings.h"
#include "StyleBuilderState.h"
#include "StyleGridData.h"
#include "StyleScrollSnapPoints.h"
#include "StyleTextEdge.h"
#include "TabSize.h"
#include "TextSpacing.h"
#include "TouchAction.h"
#include "TransformFunctions.h"

namespace WebCore {
namespace Style {

// Note that we assume the CSS parser only allows valid CSSValue types.
class BuilderConverter {
public:
    static Length convertLength(const BuilderState&, const CSSValue&);
    static Length convertLengthOrAuto(const BuilderState&, const CSSValue&);
    static Length convertLengthOrAutoOrContent(const BuilderState&, const CSSValue&);
    static Length convertLengthSizing(const BuilderState&, const CSSValue&);
    static Length convertLengthMaxSizing(const BuilderState&, const CSSValue&);
    static Length convertLengthAllowingNumber(const BuilderState&, const CSSValue&); // Assumes unit is 'px' if input is a number.
    static TabSize convertTabSize(const BuilderState&, const CSSValue&);
    template<typename T> static T convertComputedLength(BuilderState&, const CSSValue&);
    template<typename T> static T convertLineWidth(BuilderState&, const CSSValue&);
    static float convertSpacing(BuilderState&, const CSSValue&);
    static LengthSize convertRadius(BuilderState&, const CSSValue&);
    static LengthPoint convertPosition(BuilderState&, const CSSValue&);
    static LengthPoint convertPositionOrAuto(BuilderState&, const CSSValue&);
    static OptionSet<TextDecorationLine> convertTextDecorationLine(BuilderState&, const CSSValue&);
    static OptionSet<TextTransform> convertTextTransform(BuilderState&, const CSSValue&);
    template<typename T> static T convertNumber(BuilderState&, const CSSValue&);
    template<typename T> static T convertNumberOrAuto(BuilderState&, const CSSValue&);
    static short convertWebkitHyphenateLimitLines(BuilderState&, const CSSValue&);
    template<CSSPropertyID> static RefPtr<StyleImage> convertStyleImage(BuilderState&, CSSValue&);
    static ImageOrientation convertImageOrientation(BuilderState&, const CSSValue&);
    static TransformOperations convertTransform(BuilderState&, const CSSValue&);
    static RefPtr<RotateTransformOperation> convertRotate(BuilderState&, const CSSValue&);
    static RefPtr<ScaleTransformOperation> convertScale(BuilderState&, const CSSValue&);
    static RefPtr<TranslateTransformOperation> convertTranslate(BuilderState&, const CSSValue&);
#if ENABLE(DARK_MODE_CSS)
    static StyleColorScheme convertColorScheme(BuilderState&, const CSSValue&);
#endif
    static String convertString(BuilderState&, const CSSValue&);
    static String convertStringOrAuto(BuilderState&, const CSSValue&);
    static AtomString convertStringOrAutoAtom(BuilderState& state, const CSSValue& value) { return AtomString { convertStringOrAuto(state, value) }; }
    static String convertStringOrNone(BuilderState&, const CSSValue&);
    static AtomString convertStringOrNoneAtom(BuilderState& state, const CSSValue& value) { return AtomString { convertStringOrNone(state, value) }; }
    static AtomString convertCustomIdentAtom(BuilderState&, const CSSValue&);
    static AtomString convertCustomIdentOrNoneAtom(BuilderState&, const CSSValue&);
    static OptionSet<TextEmphasisPosition> convertTextEmphasisPosition(BuilderState&, const CSSValue&);
    static TextAlignMode convertTextAlign(BuilderState&, const CSSValue&);
    static TextAlignLast convertTextAlignLast(BuilderState&, const CSSValue&);
    static RefPtr<PathOperation> convertPathOperation(BuilderState&, const CSSValue&);
    static Resize convertResize(BuilderState&, const CSSValue&);
    static int convertMarqueeRepetition(BuilderState&, const CSSValue&);
    static int convertMarqueeSpeed(BuilderState&, const CSSValue&);
    static RefPtr<QuotesData> convertQuotes(BuilderState&, const CSSValue&);
    static TextUnderlinePosition convertTextUnderlinePosition(BuilderState&, const CSSValue&);
    static TextUnderlineOffset convertTextUnderlineOffset(BuilderState&, const CSSValue&);
    static TextDecorationThickness convertTextDecorationThickness(BuilderState&, const CSSValue&);
    static RefPtr<StyleReflection> convertReflection(BuilderState&, const CSSValue&);
    static TextEdge convertTextEdge(BuilderState&, const CSSValue&);
    static IntSize convertInitialLetter(BuilderState&, const CSSValue&);
    static float convertTextStrokeWidth(BuilderState&, const CSSValue&);
    static OptionSet<LineBoxContain> convertLineBoxContain(BuilderState&, const CSSValue&);
    static RefPtr<ShapeValue> convertShapeValue(BuilderState&, CSSValue&);
    static ScrollSnapType convertScrollSnapType(BuilderState&, const CSSValue&);
    static ScrollSnapAlign convertScrollSnapAlign(BuilderState&, const CSSValue&);
    static ScrollSnapStop convertScrollSnapStop(BuilderState&, const CSSValue&);
    static GridTrackSize convertGridTrackSize(BuilderState&, const CSSValue&);
    static Vector<GridTrackSize> convertGridTrackSizeList(BuilderState&, const CSSValue&);
    static std::optional<GridPosition> convertGridPosition(BuilderState&, const CSSValue&);
    static GridAutoFlow convertGridAutoFlow(BuilderState&, const CSSValue&);
    static Vector<StyleContentAlignmentData> convertContentAlignmentDataList(BuilderState&, const CSSValue&);
    static MasonryAutoFlow convertMasonryAutoFlow(BuilderState&, const CSSValue&);
    static std::optional<Length> convertWordSpacing(BuilderState&, const CSSValue&);
    static std::optional<float> convertPerspective(BuilderState&, const CSSValue&);
    static std::optional<Length> convertMarqueeIncrement(BuilderState&, const CSSValue&);
    static std::optional<FilterOperations> convertFilterOperations(BuilderState&, const CSSValue&);
    static ListStyleType convertListStyleType(const BuilderState&, const CSSValue&);
#if PLATFORM(IOS_FAMILY)
    static bool convertTouchCallout(BuilderState&, const CSSValue&);
#endif
#if ENABLE(TOUCH_EVENTS)
    static StyleColor convertTapHighlightColor(BuilderState&, const CSSValue&);
#endif
    static OptionSet<TouchAction> convertTouchAction(BuilderState&, const CSSValue&);
#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
    static bool convertOverflowScrolling(BuilderState&, const CSSValue&);
#endif
    static FontFeatureSettings convertFontFeatureSettings(BuilderState&, const CSSValue&);
    static bool convertSmoothScrolling(BuilderState&, const CSSValue&);
    static FontSelectionValue convertFontWeightFromValue(const CSSValue&);
    static FontSelectionValue convertFontStretchFromValue(const CSSValue&);
    static FontSelectionValue convertFontStyleAngle(const CSSValue&);
    static std::optional<FontSelectionValue> convertFontStyleFromValue(const CSSValue&);
    static FontSelectionValue convertFontWeight(BuilderState&, const CSSValue&);
    static FontSelectionValue convertFontStretch(BuilderState&, const CSSValue&);
    static FontSelectionValue convertFontStyle(BuilderState&, const CSSValue&);
    static FontVariationSettings convertFontVariationSettings(BuilderState&, const CSSValue&);
    static SVGLengthValue convertSVGLengthValue(BuilderState&, const CSSValue&, ShouldConvertNumberToPxLength = ShouldConvertNumberToPxLength::No);
    static Vector<SVGLengthValue> convertSVGLengthVector(BuilderState&, const CSSValue&, ShouldConvertNumberToPxLength = ShouldConvertNumberToPxLength::No);
    static Vector<SVGLengthValue> convertStrokeDashArray(BuilderState&, const CSSValue&);
    static PaintOrder convertPaintOrder(BuilderState&, const CSSValue&);
    static float convertOpacity(BuilderState&, const CSSValue&);
    static String convertSVGURIReference(BuilderState&, const CSSValue&);
    static StyleSelfAlignmentData convertSelfOrDefaultAlignmentData(BuilderState&, const CSSValue&);
    static StyleContentAlignmentData convertContentAlignmentData(BuilderState&, const CSSValue&);
    static GlyphOrientation convertGlyphOrientation(BuilderState&, const CSSValue&);
    static GlyphOrientation convertGlyphOrientationOrAuto(BuilderState&, const CSSValue&);
    static Length convertLineHeight(BuilderState&, const CSSValue&, float multiplier = 1.f);
    static FontPalette convertFontPalette(BuilderState&, const CSSValue&);
    
    static BreakBetween convertPageBreakBetween(BuilderState&, const CSSValue&);
    static BreakInside convertPageBreakInside(BuilderState&, const CSSValue&);
    static BreakBetween convertColumnBreakBetween(BuilderState&, const CSSValue&);
    static BreakInside convertColumnBreakInside(BuilderState&, const CSSValue&);

    static OptionSet<HangingPunctuation> convertHangingPunctuation(BuilderState&, const CSSValue&);

    static OptionSet<SpeakAs> convertSpeakAs(BuilderState&, const CSSValue&);

    static Length convertPositionComponentX(BuilderState&, const CSSValue&);
    static Length convertPositionComponentY(BuilderState&, const CSSValue&);

    static GapLength convertGapLength(BuilderState&, const CSSValue&);

    static OffsetRotation convertOffsetRotate(BuilderState&, const CSSValue&);
    static Vector<AtomString> convertContainerName(BuilderState&, const CSSValue&);

    static OptionSet<MarginTrimType> convertMarginTrim(BuilderState&, const CSSValue&);

    static TextSpacingTrim convertTextSpacingTrim(BuilderState&, const CSSValue&);
    static TextAutospace convertTextAutospace(BuilderState&, const CSSValue&);

    static std::optional<Length> convertBlockStepSize(BuilderState&, const CSSValue&);
    
private:
    friend class BuilderCustom;

    static Length convertToRadiusLength(const CSSToLengthConversionData&, const CSSPrimitiveValue&);
    static OptionSet<TextEmphasisPosition> valueToEmphasisPosition(const CSSValue&);
    static Length parseSnapCoordinate(BuilderState&, const CSSValue&);

#if ENABLE(DARK_MODE_CSS)
    static void updateColorScheme(const CSSValue&, StyleColorScheme&);
#endif

    static Length convertTo100PercentMinusLength(const Length&);
    template<CSSValueID, CSSValueID> static Length convertPositionComponent(BuilderState&, const CSSValue&);

    static GridLength createGridTrackBreadth(const CSSValue&, BuilderState&);
    static GridTrackSize createGridTrackSize(const CSSValue&, BuilderState&);
    static bool createGridTrackList(const CSSValue&, GridTrackList&, BuilderState&);
    static bool createGridPosition(const CSSValue&, GridPosition&);
    static void createImplicitNamedGridLinesFromGridArea(const NamedGridAreaMap&, NamedGridLinesMap&, GridTrackSizingDirection);
    static CSSToLengthConversionData csstoLengthConversionDataWithTextZoomFactor(BuilderState&);
};

inline Length BuilderConverter::convertLength(const BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    CSSToLengthConversionData conversionData = builderState.useSVGZoomRulesForLength() ?
        builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
        : builderState.cssToLengthConversionData();

    if (primitiveValue.isLength()) {
        Length length = primitiveValue.computeLength<Length>(conversionData);
        length.setHasQuirk(primitiveValue.primitiveType() == CSSUnitType::CSS_QUIRKY_EMS);
        return length;
    }

    if (primitiveValue.isPercentage())
        return Length(primitiveValue.doubleValue(), LengthType::Percent);

    if (primitiveValue.isCalculatedPercentageWithLength())
        return Length(primitiveValue.cssCalcValue()->createCalculationValue(conversionData));

    ASSERT_NOT_REACHED();
    return Length(0, LengthType::Fixed);
}

inline Length BuilderConverter::convertLengthAllowingNumber(const BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.isNumberOrInteger())
        return convertLength(builderState, CSSPrimitiveValue::create(primitiveValue.doubleValue(), CSSUnitType::CSS_PX));
    return convertLength(builderState, value);
}

inline Length BuilderConverter::convertLengthOrAuto(const BuilderState& builderState, const CSSValue& value)
{
    if (value == CSSValueAuto)
        return Length(LengthType::Auto);
    return convertLength(builderState, value);
}

inline Length BuilderConverter::convertLengthSizing(const BuilderState& builderState, const CSSValue& value)
{
    switch (value.valueID()) {
    case CSSValueInvalid:
        return convertLength(builderState, value);
    case CSSValueIntrinsic:
        return Length(LengthType::Intrinsic);
    case CSSValueMinIntrinsic:
        return Length(LengthType::MinIntrinsic);
    case CSSValueMinContent:
    case CSSValueWebkitMinContent:
        return Length(LengthType::MinContent);
    case CSSValueMaxContent:
    case CSSValueWebkitMaxContent:
        return Length(LengthType::MaxContent);
    case CSSValueWebkitFillAvailable:
        return Length(LengthType::FillAvailable);
    case CSSValueFitContent:
    case CSSValueWebkitFitContent:
        return Length(LengthType::FitContent);
    case CSSValueAuto:
        return Length(LengthType::Auto);
    case CSSValueContent:
        return Length(LengthType::Content);
    default:
        ASSERT_NOT_REACHED();
        return Length();
    }
}

inline ListStyleType BuilderConverter::convertListStyleType(const BuilderState&, const CSSValue& value)
{
    if (value.isValueID())
        return { fromCSSValue<ListStyleType::Type>(value), nullAtom() };
    if (value.isCustomIdent())
        return { ListStyleType::Type::CounterStyle, makeAtomString(downcast<CSSCustomIdentValue>(value).string()) };
    return { ListStyleType::Type::String, makeAtomString(downcast<CSSStringValue>(value).string()) };
}

inline Length BuilderConverter::convertLengthMaxSizing(const BuilderState& builderState, const CSSValue& value)
{
    if (value == CSSValueNone)
        return Length(LengthType::Undefined);
    return convertLengthSizing(builderState, value);
}

inline TabSize BuilderConverter::convertTabSize(const BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.isNumber())
        return TabSize(primitiveValue.floatValue(), SpaceValueType);
    return TabSize(primitiveValue.computeLength<float>(builderState.cssToLengthConversionData()), LengthValueType);
}

template<typename T>
inline T BuilderConverter::convertComputedLength(BuilderState& builderState, const CSSValue& value)
{
    return downcast<CSSPrimitiveValue>(value).computeLength<T>(builderState.cssToLengthConversionData());
}

template<typename T>
inline T BuilderConverter::convertLineWidth(BuilderState& builderState, const CSSValue& value)
{
    switch (value.valueID()) {
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
            T originalLength = downcast<CSSPrimitiveValue>(value).computeLength<T>(builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0));
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

inline float BuilderConverter::convertSpacing(BuilderState& builderState, const CSSValue& value)
{
    if (value == CSSValueNormal)
        return 0.f;

    auto conversionData = builderState.useSVGZoomRulesForLength()
        ? builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
        : builderState.cssToLengthConversionData();
    return downcast<CSSPrimitiveValue>(value).computeLength<float>(conversionData);
}

inline Length BuilderConverter::convertToRadiusLength(const CSSToLengthConversionData& conversionData, const CSSPrimitiveValue& value)
{
    if (value.isPercentage())
        return Length(value.doubleValue(), LengthType::Percent);
    if (value.isCalculatedPercentageWithLength())
        return Length(value.cssCalcValue()->createCalculationValue(conversionData));
    auto length = value.computeLength<Length>(conversionData);
    if (length.isNegative())
        return { 0, LengthType::Fixed };
    return length;
}

inline LengthSize BuilderConverter::convertRadius(BuilderState& builderState, const CSSValue& value)
{
    if (!value.isPair())
        return { { 0, LengthType::Fixed }, { 0, LengthType::Fixed } };
    auto& conversionData = builderState.cssToLengthConversionData();
    auto length = [&](const CSSValue& value) {
        auto length = convertToRadiusLength(conversionData, downcast<CSSPrimitiveValue>(value));
        ASSERT(!length.isNegative());
        return length;
    };
    auto& pair = downcast<CSSValuePair>(value);
    return { length(pair.first()), length(pair.second()) };
}

inline Length BuilderConverter::convertTo100PercentMinusLength(const Length& length)
{
    if (length.isPercent())
        return Length(100 - length.value(), LengthType::Percent);
    
    // Turn this into a calc expression: calc(100% - length)
    auto lengths = Vector<std::unique_ptr<CalcExpressionNode>>::from(
        makeUnique<CalcExpressionLength>(Length(100, LengthType::Percent)),
        makeUnique<CalcExpressionLength>(length)
    );
    auto operation = makeUnique<CalcExpressionOperation>(WTFMove(lengths), CalcOperator::Subtract);
    return Length(CalculationValue::create(WTFMove(operation), ValueRange::All));
}

inline Length BuilderConverter::convertPositionComponentX(BuilderState& builderState, const CSSValue& value)
{
    return convertPositionComponent<CSSValueLeft, CSSValueRight>(builderState, value);
}

inline Length BuilderConverter::convertPositionComponentY(BuilderState& builderState, const CSSValue& value)
{
    return convertPositionComponent<CSSValueTop, CSSValueBottom>(builderState, value);
}

template<CSSValueID cssValueFor0, CSSValueID cssValueFor100>
inline Length BuilderConverter::convertPositionComponent(BuilderState& builderState, const CSSValue& value)
{
    Length length;

    auto* lengthValue = &value;
    bool relativeToTrailingEdge = false;
    
    if (value.isPair()) {
        auto& first = downcast<CSSValuePair>(value).first();
        if (first == CSSValueRight || first == CSSValueBottom)
            relativeToTrailingEdge = true;
        lengthValue = &downcast<CSSValuePair>(value).second();
    }
    
    if (value.isValueID()) {
        switch (value.valueID()) {
        case cssValueFor0:
            return Length(0, LengthType::Percent);
        case cssValueFor100:
            return Length(100, LengthType::Percent);
        case CSSValueCenter:
            return Length(50, LengthType::Percent);
        default:
            ASSERT_NOT_REACHED();
        }
    }
        
    length = convertLength(builderState, *lengthValue);

    if (relativeToTrailingEdge)
        length = convertTo100PercentMinusLength(length);

    return length;
}

inline LengthPoint BuilderConverter::convertPosition(BuilderState& builderState, const CSSValue& value)
{
    if (!value.isPair())
        return RenderStyle::initialObjectPosition();

    Length lengthX = convertPositionComponent<CSSValueLeft, CSSValueRight>(builderState, downcast<CSSValuePair>(value).first());
    Length lengthY = convertPositionComponent<CSSValueTop, CSSValueBottom>(builderState, downcast<CSSValuePair>(value).second());

    return LengthPoint(lengthX, lengthY);
}

inline LengthPoint BuilderConverter::convertPositionOrAuto(BuilderState& builderState, const CSSValue& value)
{
    if (value.isPair())
        return convertPosition(builderState, value);
    return { };
}

inline OptionSet<TextDecorationLine> BuilderConverter::convertTextDecorationLine(BuilderState&, const CSSValue& value)
{
    auto result = RenderStyle::initialTextDecorationLine();
    if (is<CSSValueList>(value)) {
        for (auto& currentValue : downcast<CSSValueList>(value))
            result.add(fromCSSValue<TextDecorationLine>(currentValue));
    }
    return result;
}

inline OptionSet<TextTransform> BuilderConverter::convertTextTransform(BuilderState&, const CSSValue& value)
{
    auto result = RenderStyle::initialTextTransform();
    if (is<CSSValueList>(value)) {
        for (auto& currentValue : downcast<CSSValueList>(value))
            result.add(fromCSSValue<TextTransform>(currentValue));
    }
    return result;
}

template<typename T>
inline T BuilderConverter::convertNumber(BuilderState&, const CSSValue& value)
{
    return downcast<CSSPrimitiveValue>(value).value<T>(CSSUnitType::CSS_NUMBER);
}

template<typename T>
inline T BuilderConverter::convertNumberOrAuto(BuilderState& builderState, const CSSValue& value)
{
    if (value == CSSValueAuto)
        return -1;
    return convertNumber<T>(builderState, value);
}

inline short BuilderConverter::convertWebkitHyphenateLimitLines(BuilderState&, const CSSValue& value)
{
    if (value == CSSValueNoLimit)
        return -1;
    return downcast<CSSPrimitiveValue>(value).value<short>(CSSUnitType::CSS_NUMBER);
}

template<CSSPropertyID>
inline RefPtr<StyleImage> BuilderConverter::convertStyleImage(BuilderState& builderState, CSSValue& value)
{
    return builderState.createStyleImage(value);
}

inline ImageOrientation BuilderConverter::convertImageOrientation(BuilderState&, const CSSValue& value)
{
    if (value == CSSValueFromImage)
        return ImageOrientation::Orientation::FromImage;
    return ImageOrientation::Orientation::None;
}

inline TransformOperations BuilderConverter::convertTransform(BuilderState& builderState, const CSSValue& value)
{
    auto operations = transformsForValue(value, builderState.cssToLengthConversionData());
    if (!operations)
        return TransformOperations { };
    return *operations;
}

inline RefPtr<TranslateTransformOperation> BuilderConverter::convertTranslate(BuilderState& builderState, const CSSValue& value)
{
    return translateForValue(value, builderState.cssToLengthConversionData());
}

inline RefPtr<RotateTransformOperation> BuilderConverter::convertRotate(BuilderState&, const CSSValue& value)
{
    return rotateForValue(value);
}

inline RefPtr<ScaleTransformOperation> BuilderConverter::convertScale(BuilderState&, const CSSValue& value)
{
    return scaleForValue(value);
}

#if ENABLE(DARK_MODE_CSS)
inline void BuilderConverter::updateColorScheme(const CSSValue& value, StyleColorScheme& colorScheme)
{
    switch (value.valueID()) {
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

inline StyleColorScheme BuilderConverter::convertColorScheme(BuilderState&, const CSSValue& value)
{
    StyleColorScheme colorScheme;

    if (is<CSSValueList>(value)) {
        for (auto& currentValue : downcast<CSSValueList>(value))
            updateColorScheme(currentValue, colorScheme);
    } else
        updateColorScheme(value, colorScheme);

    // If the value was just "only", that is synonymous for "only light".
    if (colorScheme.isOnly())
        colorScheme.add(ColorScheme::Light);

    return colorScheme;
}
#endif

inline String BuilderConverter::convertString(BuilderState&, const CSSValue& value)
{
    return downcast<CSSStringValue>(value).string();
}

inline String BuilderConverter::convertStringOrAuto(BuilderState& builderState, const CSSValue& value)
{
    if (value == CSSValueAuto)
        return nullAtom();
    return convertString(builderState, value);
}

inline String BuilderConverter::convertStringOrNone(BuilderState& builderState, const CSSValue& value)
{
    if (value == CSSValueNone)
        return nullAtom();
    return convertString(builderState, value);
}

inline AtomString BuilderConverter::convertCustomIdentAtom(BuilderState&, const CSSValue& value)
{
    return AtomString { downcast<CSSCustomIdentValue>(value).string() };
}

inline AtomString BuilderConverter::convertCustomIdentOrNoneAtom(BuilderState& builderState, const CSSValue& value)
{
    if (value == CSSValueNone)
        return nullAtom();
    return convertCustomIdentAtom(builderState, value);
}

inline OptionSet<TextEmphasisPosition> BuilderConverter::valueToEmphasisPosition(const CSSValue& value)
{
    switch (value.valueID()) {
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

inline OptionSet<TextEmphasisPosition> BuilderConverter::convertTextEmphasisPosition(BuilderState&, const CSSValue& value)
{
    if (!is<CSSValueList>(value))
        return valueToEmphasisPosition(value);

    OptionSet<TextEmphasisPosition> position;
    for (auto& currentValue : downcast<CSSValueList>(value))
        position.add(valueToEmphasisPosition(currentValue));
    return position;
}

inline TextAlignMode BuilderConverter::convertTextAlign(BuilderState& builderState, const CSSValue& value)
{
    const auto& parentStyle = builderState.parentStyle();

    // User agents are expected to have a rule in their user agent stylesheet that matches th elements that have a parent
    // node whose computed value for the 'text-align' property is its initial value, whose declaration block consists of
    // just a single declaration that sets the 'text-align' property to the value 'center'.
    // https://html.spec.whatwg.org/multipage/rendering.html#rendering
    if (value == CSSValueInternalThCenter) {
        if (parentStyle.textAlign() == RenderStyle::initialTextAlign())
            return TextAlignMode::Center;
        return parentStyle.textAlign();
    }

    if (value == CSSValueWebkitMatchParent || value == CSSValueMatchParent) {
        const auto* element = builderState.element();

        if (element && element == builderState.document().documentElement())
            return TextAlignMode::Start;
        if (parentStyle.textAlign() == TextAlignMode::Start)
            return parentStyle.isLeftToRightDirection() ? TextAlignMode::Left : TextAlignMode::Right;
        if (parentStyle.textAlign() == TextAlignMode::End)
            return parentStyle.isLeftToRightDirection() ? TextAlignMode::Right : TextAlignMode::Left;

        return parentStyle.textAlign();
    }

    return fromCSSValue<TextAlignMode>(value);
}

inline TextAlignLast BuilderConverter::convertTextAlignLast(BuilderState& builderState, const CSSValue& value)
{
    if (value != CSSValueMatchParent)
        return fromCSSValue<TextAlignLast>(value);

    auto& parentStyle = builderState.parentStyle();
    if (parentStyle.textAlignLast() == TextAlignLast::Start)
        return parentStyle.isLeftToRightDirection() ? TextAlignLast::Left : TextAlignLast::Right;
    if (parentStyle.textAlignLast() == TextAlignLast::End)
        return parentStyle.isLeftToRightDirection() ? TextAlignLast::Right : TextAlignLast::Left;
    return parentStyle.textAlignLast();
}

inline RefPtr<PathOperation> BuilderConverter::convertPathOperation(BuilderState& builderState, const CSSValue& value)
{
    if (value == CSSValueNone)
        return nullptr;

    if (value.isURL()) {
        auto& string = downcast<CSSURLValue>(value).string();
        auto fragment = SVGURIReference::fragmentIdentifierFromIRIString(string, builderState.document());
        // FIXME: It doesn't work with external SVG references (see https://bugs.webkit.org/show_bug.cgi?id=126133)
        auto target = SVGURIReference::targetElementFromIRIString(string, builderState.document());
        return ReferencePathOperation::create(string, fragment, dynamicDowncast<SVGElement>(target.element.get()));
    }

    if (is<CSSRayValue>(value)) {
        auto& rayValue = downcast<CSSRayValue>(value);

        RayPathOperation::Size size = RayPathOperation::Size::ClosestCorner;
        switch (rayValue.size()) {
        case CSSValueClosestCorner:
            size = RayPathOperation::Size::ClosestCorner;
            break;
        case CSSValueClosestSide:
            size = RayPathOperation::Size::ClosestSide;
            break;
        case CSSValueFarthestCorner:
            size = RayPathOperation::Size::FarthestCorner;
            break;
        case CSSValueFarthestSide:
            size = RayPathOperation::Size::FarthestSide;
            break;
        case CSSValueSides:
            size = RayPathOperation::Size::Sides;
            break;
        default:
            ASSERT_NOT_REACHED();
            return nullptr;
        }

        return RayPathOperation::create(rayValue.angle().computeDegrees(), size, rayValue.isContaining());
    }

    auto referenceBox = CSSBoxType::BoxMissing;
    RefPtr<PathOperation> operation;
    for (auto& currentValue : downcast<CSSValueList>(value)) {
        if (!currentValue.isValueID()) {
            operation = ShapePathOperation::create(basicShapeForValue(builderState.cssToLengthConversionData(),
                currentValue, builderState.style().effectiveZoom()));
        } else
            referenceBox = fromCSSValue<CSSBoxType>(currentValue);
    }
    if (operation)
        downcast<ShapePathOperation>(*operation).setReferenceBox(referenceBox);
    else {
        ASSERT(referenceBox != CSSBoxType::BoxMissing);
        operation = BoxPathOperation::create(referenceBox);
    }

    return operation;
}

inline Resize BuilderConverter::convertResize(BuilderState& builderState, const CSSValue& value)
{
    Resize resize = Resize::None;
    if (value == CSSValueAuto)
        resize = builderState.document().settings().textAreasAreResizable() ? Resize::Both : Resize::None;
    else
        resize = fromCSSValue<Resize>(value);

    return resize;
}

inline int BuilderConverter::convertMarqueeRepetition(BuilderState&, const CSSValue& value)
{
    if (value == CSSValueInfinite)
        return -1; // -1 means repeat forever.

    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    ASSERT(primitiveValue.isNumber());
    return primitiveValue.intValue();
}

inline int BuilderConverter::convertMarqueeSpeed(BuilderState&, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.isTime())
        return primitiveValue.computeTime<int, CSSPrimitiveValue::Milliseconds>();
    // For scrollamount support.
    ASSERT(primitiveValue.isNumber());
    return primitiveValue.intValue();
}

inline RefPtr<QuotesData> BuilderConverter::convertQuotes(BuilderState&, const CSSValue& value)
{
    if (value == CSSValueAuto)
        return nullptr;
    if (value == CSSValueNone)
        return QuotesData::create({ });

    auto& list = downcast<CSSValueList>(value);
    Vector<std::pair<String, String>> quotes;
    quotes.reserveInitialCapacity(list.length() / 2);
    for (unsigned i = 0; i < list.length(); i += 2) {
        auto* first = list.itemWithoutBoundsCheck(i);
        // item() returns null if out of bounds so this is safe.
        auto* second = list.item(i + 1);
        if (!second)
            break;
        String startQuote = downcast<CSSStringValue>(*first).string();
        String endQuote = downcast<CSSStringValue>(*second).string();
        quotes.append(std::make_pair(WTFMove(startQuote), WTFMove(endQuote)));
    }
    return QuotesData::create(quotes);
}

inline TextUnderlinePosition BuilderConverter::convertTextUnderlinePosition(BuilderState&, const CSSValue& value)
{
    return fromCSSValue<TextUnderlinePosition>(value);
}

inline TextUnderlineOffset BuilderConverter::convertTextUnderlineOffset(BuilderState& builderState, const CSSValue& value)
{
    if (value == CSSValueAuto)
        return TextUnderlineOffset::createWithAuto();

    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    ASSERT(primitiveValue.isLength());
    auto computedLength = convertComputedLength<float>(builderState, primitiveValue);
    return TextUnderlineOffset::createWithLength(computedLength);
}

inline TextDecorationThickness BuilderConverter::convertTextDecorationThickness(BuilderState& builderState, const CSSValue& value)
{
    switch (value.valueID()) {
    case CSSValueAuto:
        return TextDecorationThickness::createWithAuto();
    case CSSValueFromFont:
        return TextDecorationThickness::createFromFont();
    default:
        auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
        ASSERT(primitiveValue.isLength());
        auto computedLength = convertComputedLength<float>(builderState, primitiveValue);
        return TextDecorationThickness::createWithLength(computedLength);
    }
}

inline RefPtr<StyleReflection> BuilderConverter::convertReflection(BuilderState& builderState, const CSSValue& value)
{
    if (value == CSSValueNone)
        return nullptr;

    auto& reflectValue = downcast<CSSReflectValue>(value);

    NinePieceImage mask(NinePieceImage::Type::Mask);
    builderState.styleMap().mapNinePieceImage(reflectValue.mask(), mask);

    auto reflection = StyleReflection::create();
    reflection->setDirection(fromCSSValueID<ReflectionDirection>(reflectValue.direction()));
    reflection->setOffset(reflectValue.offset().convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion>(builderState.cssToLengthConversionData()));
    reflection->setMask(mask);
    return reflection;
}

inline TextEdge BuilderConverter::convertTextEdge(BuilderState&, const CSSValue& value)
{
    auto& values = downcast<CSSValueList>(value);

    auto overValue = [&] {
        switch (values[0].valueID()) {
        case CSSValueLeading:
            return TextEdgeType::Leading;
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
            return TextEdgeType::Leading;
        }
    };

    auto underValue = [&] {
        if (values[0] == CSSValueLeading)
            return TextEdgeType::Leading;
        if (values.length() == 1) {
            // https://www.w3.org/TR/css-inline-3/#text-edges
            // "If only one value is specified, both edges are assigned that same keyword if possible; else text is assumed as the missing value."
            // FIXME: Figure out what "if possible" means here.
            return TextEdgeType::Text;
        }

        switch (values[1].valueID()) {
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
            return TextEdgeType::Leading;
        }
    };

    return { overValue(), underValue() };
}

inline IntSize BuilderConverter::convertInitialLetter(BuilderState&, const CSSValue& value)
{
    if (value == CSSValueNormal)
        return IntSize();
    auto integer = [](const CSSValue& value) {
        return downcast<CSSPrimitiveValue>(value).intValue();
    };
    auto& pair = downcast<CSSValuePair>(value);
    return { integer(pair.first()), integer(pair.second()) };
}

inline float BuilderConverter::convertTextStrokeWidth(BuilderState& builderState, const CSSValue& value)
{
    switch (value.valueID()) {
    case CSSValueThin:
    case CSSValueMedium:
    case CSSValueThick: {
        double result = 1.0 / 48;
        if (value == CSSValueMedium)
            result *= 3;
        else if (value == CSSValueThick)
            result *= 5;
        auto emsValue = CSSPrimitiveValue::create(result, CSSUnitType::CSS_EMS);
        return convertComputedLength<float>(builderState, emsValue);
        break;
    }
    default:
        break;
    }
    return convertComputedLength<float>(builderState, downcast<CSSPrimitiveValue>(value));
}

inline OptionSet<LineBoxContain> BuilderConverter::convertLineBoxContain(BuilderState&, const CSSValue& value)
{
    if (value == CSSValueNone)
        return { };
    return downcast<CSSLineBoxContainValue>(value).value();
}

inline RefPtr<ShapeValue> BuilderConverter::convertShapeValue(BuilderState& builderState, CSSValue& value)
{
    if (value == CSSValueNone)
        return nullptr;

    if (value.isInImageFamily())
        return ShapeValue::create(builderState.createStyleImage(value).releaseNonNull());

    RefPtr<BasicShape> shape;
    CSSBoxType referenceBox = CSSBoxType::BoxMissing;
    for (auto& currentValue : downcast<CSSValueList>(value)) {
        if (!currentValue.isValueID())
            shape = basicShapeForValue(builderState.cssToLengthConversionData(), currentValue);
        else
            referenceBox = fromCSSValue<CSSBoxType>(currentValue);
    }

    if (shape)
        return ShapeValue::create(shape.releaseNonNull(), referenceBox);

    if (referenceBox != CSSBoxType::BoxMissing)
        return ShapeValue::create(referenceBox);

    ASSERT_NOT_REACHED();
    return nullptr;
}

inline ScrollSnapType BuilderConverter::convertScrollSnapType(BuilderState&, const CSSValue& value)
{
    ScrollSnapType type;
    auto& values = downcast<CSSValueList>(value);
    if (values[0] == CSSValueNone)
        return type;
    type.axis = fromCSSValue<ScrollSnapAxis>(values[0]);
    if (values.length() == 2)
        type.strictness = fromCSSValue<ScrollSnapStrictness>(values[1]);
    else
        type.strictness = ScrollSnapStrictness::Proximity;
    return type;
}

inline ScrollSnapAlign BuilderConverter::convertScrollSnapAlign(BuilderState&, const CSSValue& value)
{
    auto& values = downcast<CSSValueList>(value);
    ScrollSnapAlign alignment;
    alignment.blockAlign = fromCSSValue<ScrollSnapAxisAlignType>(values[0]);
    if (values.length() == 1)
        alignment.inlineAlign = alignment.blockAlign;
    else
        alignment.inlineAlign = fromCSSValue<ScrollSnapAxisAlignType>(values[1]);
    return alignment;
}

inline ScrollSnapStop BuilderConverter::convertScrollSnapStop(BuilderState&, const CSSValue& value)
{
    return fromCSSValue<ScrollSnapStop>(value);
}

inline GridLength BuilderConverter::createGridTrackBreadth(const CSSValue& value, BuilderState& builderState)
{
    if (value == CSSValueMinContent || value == CSSValueWebkitMinContent)
        return Length(LengthType::MinContent);

    if (value == CSSValueMaxContent || value == CSSValueWebkitMaxContent)
        return Length(LengthType::MaxContent);

    if (value == CSSValueAuto)
        return Length(LengthType::Auto);

    auto& numericValue = downcast<CSSPrimitiveValue>(value);
    if (numericValue.isFlex())
        return GridLength(numericValue.doubleValue());
    return numericValue.convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion>(builderState.cssToLengthConversionData());
}

inline GridTrackSize BuilderConverter::createGridTrackSize(const CSSValue& value, BuilderState& builderState)
{
    if (!is<CSSFunctionValue>(value))
        return GridTrackSize(createGridTrackBreadth(value, builderState));

    const auto& function = downcast<CSSFunctionValue>(value);

    if (function.length() == 1)
        return GridTrackSize(createGridTrackBreadth(function[0], builderState), FitContentTrackSizing);

    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(function.length() == 2);
    GridLength minTrackBreadth(createGridTrackBreadth(function[0], builderState));
    GridLength maxTrackBreadth(createGridTrackBreadth(function[1], builderState));
    return GridTrackSize(minTrackBreadth, maxTrackBreadth);
}

inline bool BuilderConverter::createGridTrackList(const CSSValue& value, GridTrackList& trackList, BuilderState& builderState)
{
    if (value == CSSValueMasonry) {
        trackList.append(GridTrackEntryMasonry());
        return true;
    }
    if (value == CSSValueNone)
        return true;

    RefPtr<const CSSValueContainingVector> valueList;
    bool isSubgrid = is<CSSSubgridValue>(value);
    if (isSubgrid) {
        valueList = &downcast<CSSSubgridValue>(value);
        trackList.append(GridTrackEntrySubgrid());
    } else if (is<CSSValueList>(value))
        valueList = &downcast<CSSValueList>(value);

    // https://drafts.csswg.org/css-grid-2/#computed-tracks
    // The computed track list of a non-subgrid axis is a list alternating between line name sets
    // and track sections, with the first and last items being line name sets.
    auto ensureLineNames = [&](auto& list) {
        if (isSubgrid)
            return;
        if (list.isEmpty() || !std::holds_alternative<Vector<String>>(list.last()))
            list.append(Vector<String>());
    };

    auto buildRepeatList = [&](const CSSValue& repeatValue, RepeatTrackList& repeatList) {
        for (auto& currentValue : downcast<CSSValueContainingVector>(repeatValue)) {
            if (is<CSSGridLineNamesValue>(currentValue)) {
                Vector<String> names;
                for (auto& namedGridLineValue : downcast<CSSGridLineNamesValue>(currentValue).names())
                    names.append(namedGridLineValue);
                repeatList.append(WTFMove(names));
            } else {
                ensureLineNames(repeatList);
                repeatList.append(createGridTrackSize(currentValue, builderState));
            }
        }

        if (!repeatList.isEmpty())
            ensureLineNames(repeatList);
    };

    auto addOne = [&](const CSSValue& currentValue) {
        if (is<CSSGridLineNamesValue>(currentValue)) {
            Vector<String> names;
            for (auto& namedGridLineValue : downcast<CSSGridLineNamesValue>(currentValue).names())
                names.append(namedGridLineValue);
            trackList.append(WTFMove(names));
            return;
        }

        ensureLineNames(trackList);

        if (is<CSSGridAutoRepeatValue>(currentValue)) {
            CSSValueID autoRepeatID = downcast<CSSGridAutoRepeatValue>(currentValue).autoRepeatID();
            ASSERT(autoRepeatID == CSSValueAutoFill || autoRepeatID == CSSValueAutoFit);

            GridTrackEntryAutoRepeat repeat;
            repeat.type = autoRepeatID == CSSValueAutoFill ? AutoRepeatType::Fill : AutoRepeatType::Fit;

            buildRepeatList(currentValue, repeat.list);
            trackList.append(WTFMove(repeat));
        } else if (is<CSSGridIntegerRepeatValue>(currentValue)) {
            GridTrackEntryRepeat repeat;
            repeat.repeats = downcast<CSSGridIntegerRepeatValue>(currentValue).repetitions();

            buildRepeatList(currentValue, repeat.list);
            trackList.append(WTFMove(repeat));
        } else {
            trackList.append(createGridTrackSize(currentValue, builderState));
        }
    };

    if (!valueList)
        addOne(value);
    else {
        for (auto& value : *valueList)
            addOne(value);
    }

    if (!trackList.isEmpty())
        ensureLineNames(trackList);

    return true;
}

inline bool BuilderConverter::createGridPosition(const CSSValue& value, GridPosition& position)
{
    // We accept the specification's grammar:
    // auto | <custom-ident> | [ <integer> && <custom-ident>? ] | [ span && [ <integer> || <custom-ident> ] ]

    if (value == CSSValueAuto)
        return true;

    if (value.isCustomIdent()) {
        position.setNamedGridArea(downcast<CSSCustomIdentValue>(value).string());
        return true;
    }

    auto& values = downcast<CSSValueList>(value);
    ASSERT(values.length());

    auto it = values.begin();
    auto* currentValue = &*it;
    bool isSpanPosition = false;
    if (*currentValue == CSSValueSpan) {
        isSpanPosition = true;
        ++it;
        currentValue = it != values.end() ? &*it : nullptr;
    }

    int gridLineNumber = 0;
    if (is<CSSPrimitiveValue>(currentValue)) {
        gridLineNumber = downcast<CSSPrimitiveValue>(*currentValue).intValue();
        ++it;
        currentValue = it != values.end() ? &*it : nullptr;
    }

    AtomString gridLineName;
    if (auto* identValue = dynamicDowncast<CSSCustomIdentValue>(currentValue)) {
        gridLineName = identValue->string();
        ++it;
    }

    ASSERT(it == values.end());
    if (isSpanPosition)
        position.setSpanPosition(gridLineNumber ? gridLineNumber : 1, gridLineName);
    else
        position.setExplicitPosition(gridLineNumber, gridLineName);

    return true;
}

inline void BuilderConverter::createImplicitNamedGridLinesFromGridArea(const NamedGridAreaMap& namedGridAreas, NamedGridLinesMap& namedGridLines, GridTrackSizingDirection direction)
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

inline Vector<GridTrackSize> BuilderConverter::convertGridTrackSizeList(BuilderState& builderState, const CSSValue& value)
{
    if (value == CSSValueAuto)
        return RenderStyle::initialGridAutoRows();

    auto validateValueAndAppend = [&builderState](Vector<GridTrackSize>& trackSizes, const CSSValue& value) {
        ASSERT(!value.isGridLineNamesValue());
        ASSERT(!value.isGridAutoRepeatValue());
        ASSERT(!value.isGridIntegerRepeatValue());
        trackSizes.uncheckedAppend(convertGridTrackSize(builderState, value));
    };

    if (!is<CSSValueList>(value)) {
        // Values coming from CSS Typed OM may not have been converted to a CSSValueList yet.
        Vector<GridTrackSize> trackSizes;
        trackSizes.reserveInitialCapacity(1);
        validateValueAndAppend(trackSizes, value);
        return trackSizes;
    }

    auto& valueList = downcast<CSSValueList>(value);
    Vector<GridTrackSize> trackSizes;
    trackSizes.reserveInitialCapacity(valueList.length());
    for (auto& currentValue : valueList)
        validateValueAndAppend(trackSizes, currentValue);
    return trackSizes;
}

inline GridTrackSize BuilderConverter::convertGridTrackSize(BuilderState& builderState, const CSSValue& value)
{
    return createGridTrackSize(value, builderState);
}

inline std::optional<GridPosition> BuilderConverter::convertGridPosition(BuilderState&, const CSSValue& value)
{
    GridPosition gridPosition;
    if (createGridPosition(value, gridPosition))
        return gridPosition;
    return std::nullopt;
}

inline GridAutoFlow BuilderConverter::convertGridAutoFlow(BuilderState&, const CSSValue& value)
{
    bool isList = is<CSSValueList>(value);
    if (isList) {
        auto& list = downcast<CSSValueList>(value);
        if (!list.length())
            return RenderStyle::initialGridAutoFlow();
    }

    auto& first = isList ? downcast<CSSValueList>(value)[0] : value;
    auto* second = isList && downcast<CSSValueList>(value).length() == 2 ? &downcast<CSSValueList>(value)[1] : nullptr;

    GridAutoFlow autoFlow;
    switch (first.valueID()) {
    case CSSValueRow:
        if (second == CSSValueDense)
            autoFlow = AutoFlowRowDense;
        else
            autoFlow = AutoFlowRow;
        break;
    case CSSValueColumn:
        if (second == CSSValueDense)
            autoFlow = AutoFlowColumnDense;
        else
            autoFlow = AutoFlowColumn;
        break;
    case CSSValueDense:
        if (second == CSSValueColumn)
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

inline Vector<StyleContentAlignmentData> BuilderConverter::convertContentAlignmentDataList(BuilderState& builder, const CSSValue& value)
{
    auto& list = downcast<CSSValueList>(value);
    Vector<StyleContentAlignmentData> tracks;
    tracks.reserveInitialCapacity(list.length());
    for (auto& value : list)
        tracks.append(convertContentAlignmentData(builder, downcast<CSSContentDistributionValue>(value)));
    return tracks;
}

inline MasonryAutoFlow BuilderConverter::convertMasonryAutoFlow(BuilderState&, const CSSValue& value)
{
    auto& valueList = downcast<CSSValueList>(value);
    MasonryAutoFlow masonryAutoFlow;
    if (valueList.size() == 2) {
        ASSERT(valueList[0] == CSSValuePack || valueList[0] == CSSValueNext);
        ASSERT(valueList[1] == CSSValueOrdered);
        if (valueList[0] == CSSValuePack)
            masonryAutoFlow = { MasonryAutoFlowPlacementAlgorithm::Pack, MasonryAutoFlowPlacementOrder::Ordered };
        else
            masonryAutoFlow = { MasonryAutoFlowPlacementAlgorithm::Next, MasonryAutoFlowPlacementOrder::Ordered };
    } else {
        ASSERT(valueList[0] == CSSValuePack || valueList[0] == CSSValueNext || valueList[0] == CSSValueOrdered);
        if (valueList[0] == CSSValuePack)
            masonryAutoFlow = { MasonryAutoFlowPlacementAlgorithm::Pack, MasonryAutoFlowPlacementOrder::DefiniteFirst };
        else if (valueList[0] == CSSValueNext)
            masonryAutoFlow = { MasonryAutoFlowPlacementAlgorithm::Next, MasonryAutoFlowPlacementOrder::DefiniteFirst };
        else
            masonryAutoFlow = { MasonryAutoFlowPlacementAlgorithm::Pack, MasonryAutoFlowPlacementOrder::Ordered };
    }
    return masonryAutoFlow;
}

inline float zoomWithTextZoomFactor(BuilderState& builderState)
{
    if (auto* frame = builderState.document().frame()) {
        float textZoomFactor = builderState.style().textZoom() != TextZoom::Reset ? frame->textZoomFactor() : 1.0f;
        return builderState.style().effectiveZoom() * textZoomFactor;
    }
    return builderState.cssToLengthConversionData().zoom();
}

inline CSSToLengthConversionData BuilderConverter::csstoLengthConversionDataWithTextZoomFactor(BuilderState& builderState)
{
    float zoom = zoomWithTextZoomFactor(builderState);
    if (zoom == builderState.cssToLengthConversionData().zoom())
        return builderState.cssToLengthConversionData();

    return builderState.cssToLengthConversionData().copyWithAdjustedZoom(zoom);
}

inline std::optional<Length> BuilderConverter::convertWordSpacing(BuilderState& builderState, const CSSValue& value)
{
    if (value == CSSValueNormal)
        return RenderStyle::initialWordSpacing();
    auto& numericValue = downcast<CSSPrimitiveValue>(value);
    if (numericValue.isLength())
        return numericValue.computeLength<Length>(csstoLengthConversionDataWithTextZoomFactor(builderState));
    if (numericValue.isPercentage())
        return Length(clampTo<double>(numericValue.doubleValue(), minValueForCssLength, maxValueForCssLength), LengthType::Percent);
    if (numericValue.isNumber())
        return Length(numericValue.doubleValue(), LengthType::Fixed);
    return std::nullopt;
}

inline std::optional<float> BuilderConverter::convertPerspective(BuilderState& builderState, const CSSValue& value)
{
    if (value == CSSValueNone)
        return RenderStyle::initialPerspective();

    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    float perspective = -1;
    if (primitiveValue.isLength())
        perspective = primitiveValue.computeLength<float>(builderState.cssToLengthConversionData());
    else if (primitiveValue.isNumber())
        perspective = primitiveValue.doubleValue() * builderState.cssToLengthConversionData().zoom();
    else
        ASSERT_NOT_REACHED();

    return perspective < 0 ? std::optional<float>(std::nullopt) : std::optional<float>(perspective);
}

inline std::optional<Length> BuilderConverter::convertMarqueeIncrement(BuilderState& builderState, const CSSValue& value)
{
    Length length = downcast<CSSPrimitiveValue>(value).convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion>(builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f));
    if (length.isUndefined())
        return std::nullopt;
    return length;
}

inline std::optional<FilterOperations> BuilderConverter::convertFilterOperations(BuilderState& builderState, const CSSValue& value)
{
    return builderState.createFilterOperations(value);
}

inline FontFeatureSettings BuilderConverter::convertFontFeatureSettings(BuilderState&, const CSSValue& value)
{
    if (value.isValueID()) {
        ASSERT(value == CSSValueNormal || CSSPropertyParserHelpers::isSystemFontShorthand(value.valueID()));
        return { };
    }

    FontFeatureSettings settings;
    for (auto& item : downcast<CSSValueList>(value)) {
        auto& feature = downcast<CSSFontFeatureValue>(item);
        settings.insert(FontFeature(feature.tag(), feature.value()));
    }
    return settings;
}

inline FontSelectionValue BuilderConverter::convertFontWeightFromValue(const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).isNumber());
        return FontSelectionValue::clampFloat(downcast<CSSPrimitiveValue>(value).floatValue());
    }

    switch (value.valueID()) {
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

inline FontSelectionValue BuilderConverter::convertFontStretchFromValue(const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).isPercentage());
        return FontSelectionValue::clampFloat(downcast<CSSPrimitiveValue>(value).floatValue());
    }
    if (auto stretchValue = fontStretchValue(value.valueID()))
        return stretchValue.value();
    ASSERT(CSSPropertyParserHelpers::isSystemFontShorthand(value.valueID()));
    return normalStretchValue();
}

inline FontSelectionValue BuilderConverter::convertFontStyleAngle(const CSSValue& value)
{
    return FontSelectionValue { std::clamp(downcast<CSSPrimitiveValue>(value).value<float>(CSSUnitType::CSS_DEG), -90.0f, 90.0f) };
}

// The input value needs to parsed and valid, this function returns std::nullopt if the input was "normal".
inline std::optional<FontSelectionValue> BuilderConverter::convertFontStyleFromValue(const CSSValue& value)
{
    if (auto* fontStyleValue = dynamicDowncast<CSSFontStyleWithAngleValue>(value))
        return convertFontStyleAngle(fontStyleValue->obliqueAngle());

    auto valueID = value.valueID();
    if (valueID == CSSValueNormal)
        return std::nullopt;
    ASSERT(valueID == CSSValueItalic || valueID == CSSValueOblique);
    return italicValue();
}

inline FontSelectionValue BuilderConverter::convertFontWeight(BuilderState& builderState, const CSSValue& value)
{
    if (value == CSSValueBolder)
        return FontCascadeDescription::bolderWeight(builderState.parentStyle().fontDescription().weight());
    if (value == CSSValueLighter)
        return FontCascadeDescription::lighterWeight(builderState.parentStyle().fontDescription().weight());
    if (CSSPropertyParserHelpers::isSystemFontShorthand(value.valueID()))
        return SystemFontDatabase::singleton().systemFontShorthandWeight(CSSPropertyParserHelpers::lowerFontShorthand(value.valueID()));
    return convertFontWeightFromValue(value);
}

inline FontSelectionValue BuilderConverter::convertFontStretch(BuilderState&, const CSSValue& value)
{
    return convertFontStretchFromValue(value);
}

inline FontVariationSettings BuilderConverter::convertFontVariationSettings(BuilderState&, const CSSValue& value)
{
    if (value.isValueID()) {
        ASSERT(value == CSSValueNormal || CSSPropertyParserHelpers::isSystemFontShorthand(value.valueID()));
        return { };
    }

    FontVariationSettings settings;
    for (auto& item : downcast<CSSValueList>(value)) {
        auto& feature = downcast<CSSFontVariationValue>(item);
        settings.insert({ feature.tag(), feature.value() });
    }
    return settings;
}

#if PLATFORM(IOS_FAMILY)
inline bool BuilderConverter::convertTouchCallout(BuilderState&, const CSSValue& value)
{
    return value != CSSValueNone;
}
#endif

#if ENABLE(TOUCH_EVENTS)
inline StyleColor BuilderConverter::convertTapHighlightColor(BuilderState& builderState, const CSSValue& value)
{
    return builderState.colorFromValue(value);
}
#endif

inline OptionSet<TouchAction> BuilderConverter::convertTouchAction(BuilderState&, const CSSValue& value)
{
    if (value.isValueID())
        return fromCSSValue<TouchAction>(value);

    if (is<CSSValueList>(value)) {
        OptionSet<TouchAction> touchActions;
        for (auto& currentValue : downcast<CSSValueList>(value)) {
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
    return value == CSSValueTouch;
}
#endif

inline bool BuilderConverter::convertSmoothScrolling(BuilderState&, const CSSValue& value)
{
    return value == CSSValueSmooth;
}

inline SVGLengthValue BuilderConverter::convertSVGLengthValue(BuilderState& builderState, const CSSValue& value, ShouldConvertNumberToPxLength shouldConvertNumberToPxLength)
{
    return SVGLengthValue::fromCSSPrimitiveValue(downcast<CSSPrimitiveValue>(value), builderState.cssToLengthConversionData(), shouldConvertNumberToPxLength);
}

inline Vector<SVGLengthValue> BuilderConverter::convertSVGLengthVector(BuilderState& builderState, const CSSValue& value, ShouldConvertNumberToPxLength shouldConvertNumberToPxLength)
{
    auto& valueList = downcast<CSSValueList>(value);

    Vector<SVGLengthValue> svgLengths;
    svgLengths.reserveInitialCapacity(valueList.length());
    for (auto& item : valueList)
        svgLengths.uncheckedAppend(convertSVGLengthValue(builderState, item, shouldConvertNumberToPxLength));

    return svgLengths;
}

inline Vector<SVGLengthValue> BuilderConverter::convertStrokeDashArray(BuilderState& builderState, const CSSValue& value)
{
    if (value == CSSValueNone)
        return SVGRenderStyle::initialStrokeDashArray();

    // Values coming from CSS-Typed-OM may not have been converted to a CSSValueList yet.
    if (!is<CSSValueList>(value))
        return Vector { convertSVGLengthValue(builderState, value, ShouldConvertNumberToPxLength::Yes) };

    return convertSVGLengthVector(builderState, value, ShouldConvertNumberToPxLength::Yes);
}

inline PaintOrder BuilderConverter::convertPaintOrder(BuilderState&, const CSSValue& value)
{
    if (value == CSSValueNormal)
        return PaintOrder::Normal;

    auto& list = downcast<CSSValueList>(value);
    switch (list[0].valueID()) {
    case CSSValueFill:
        return list.length() > 1 ? PaintOrder::FillMarkers : PaintOrder::Fill;
    case CSSValueStroke:
        return list.length() > 1 ? PaintOrder::StrokeMarkers : PaintOrder::Stroke;
    case CSSValueMarkers:
        return list.length() > 1 ? PaintOrder::MarkersStroke : PaintOrder::Markers;
    default:
        ASSERT_NOT_REACHED();
        return PaintOrder::Normal;
    }
}

inline float BuilderConverter::convertOpacity(BuilderState&, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    float opacity = primitiveValue.floatValue();
    if (primitiveValue.isPercentage())
        opacity /= 100.0f;
    return std::max(0.0f, std::min(1.0f, opacity));
}

inline String BuilderConverter::convertSVGURIReference(BuilderState&, const CSSValue& value)
{
    return value.isURL() ? downcast<CSSURLValue>(value).string() : emptyString();
}

inline StyleSelfAlignmentData BuilderConverter::convertSelfOrDefaultAlignmentData(BuilderState&, const CSSValue& value)
{
    auto alignmentData = RenderStyle::initialSelfAlignment();
    if (value.isPair()) {
        auto& first = downcast<CSSValuePair>(value).first();
        if (first == CSSValueLegacy) {
            alignmentData.setPositionType(ItemPositionType::Legacy);
            alignmentData.setPosition(fromCSSValue<ItemPosition>(downcast<CSSValuePair>(value).second()));
        } else if (first == CSSValueFirst)
            alignmentData.setPosition(ItemPosition::Baseline);
        else if (first == CSSValueLast)
            alignmentData.setPosition(ItemPosition::LastBaseline);
        else {
            alignmentData.setOverflow(fromCSSValue<OverflowAlignment>(first));
            alignmentData.setPosition(fromCSSValue<ItemPosition>(downcast<CSSValuePair>(value).second()));
        }
    } else
        alignmentData.setPosition(fromCSSValue<ItemPosition>(value));
    return alignmentData;
}

inline StyleContentAlignmentData BuilderConverter::convertContentAlignmentData(BuilderState&, const CSSValue& value)
{
    StyleContentAlignmentData alignmentData = RenderStyle::initialContentAlignment();
    if (!is<CSSContentDistributionValue>(value))
        return alignmentData;
    auto& contentValue = downcast<CSSContentDistributionValue>(value);
    if (contentValue.distribution() != CSSValueInvalid)
        alignmentData.setDistribution(fromCSSValueID<ContentDistribution>(contentValue.distribution()));
    if (contentValue.position() != CSSValueInvalid)
        alignmentData.setPosition(fromCSSValueID<ContentPosition>(contentValue.position()));
    if (contentValue.overflow() != CSSValueInvalid)
        alignmentData.setOverflow(fromCSSValueID<OverflowAlignment>(contentValue.overflow()));
    return alignmentData;
}

inline GlyphOrientation BuilderConverter::convertGlyphOrientation(BuilderState&, const CSSValue& value)
{
    float angle = std::abs(fmodf(downcast<CSSPrimitiveValue>(value).floatValue(), 360.0f));
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
    if (value == CSSValueAuto)
        return GlyphOrientation::Auto;
    return convertGlyphOrientation(builderState, value);
}

inline Length BuilderConverter::convertLineHeight(BuilderState& builderState, const CSSValue& value, float multiplier)
{
    if (value == CSSValueNormal)
        return RenderStyle::initialLineHeight();

    if (CSSPropertyParserHelpers::isSystemFontShorthand(value.valueID()))
        return RenderStyle::initialLineHeight();

    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);

    if (primitiveValue.isLength() || primitiveValue.isCalculatedPercentageWithLength()) {
        auto conversionData = builderState.cssToLengthConversionData().copyForLineHeight(zoomWithTextZoomFactor(builderState));
        Length length;
        if (primitiveValue.isLength())
            length = primitiveValue.computeLength<Length>(conversionData);
        else {
            auto value = primitiveValue.cssCalcValue()->createCalculationValue(conversionData)->evaluate(builderState.style().computedFontSize());
            length = { clampTo<float>(value, minValueForCssLength, static_cast<float>(maxValueForCssLength)), LengthType::Fixed };
        }
        if (multiplier != 1.f)
            length = Length(length.value() * multiplier, LengthType::Fixed);
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
        return Length((builderState.style().computedFontSize() * primitiveValue.intValue()) / 100, LengthType::Fixed);
    }

    ASSERT(primitiveValue.isNumber());
    return Length(primitiveValue.doubleValue() * 100.0, LengthType::Percent);
}

inline FontPalette BuilderConverter::convertFontPalette(BuilderState&, const CSSValue& value)
{
    switch (value.valueID()) {
    case CSSValueLight:
        return { FontPalette::Type::Light, nullAtom() };
    case CSSValueDark:
        return { FontPalette::Type::Dark, nullAtom() };
    case CSSValueInvalid:
        return { FontPalette::Type::Custom, AtomString { downcast<CSSCustomIdentValue>(value).string() } };
    default:
        ASSERT(value == CSSValueNormal || CSSPropertyParserHelpers::isSystemFontShorthand(value.valueID()));
        return { FontPalette::Type::Normal, nullAtom() };
    }
}
    
inline OptionSet<SpeakAs> BuilderConverter::convertSpeakAs(BuilderState&, const CSSValue& value)
{
    auto result = RenderStyle::initialSpeakAs();
    if (is<CSSValueList>(value)) {
        for (auto& currentValue : downcast<CSSValueList>(value)) {
            if (currentValue != CSSValueNormal)
                result.add(fromCSSValue<SpeakAs>(currentValue));
        }
    }
    return result;
}

inline OptionSet<HangingPunctuation> BuilderConverter::convertHangingPunctuation(BuilderState&, const CSSValue& value)
{
    auto result = RenderStyle::initialHangingPunctuation();
    if (is<CSSValueList>(value)) {
        for (auto& currentValue : downcast<CSSValueList>(value))
            result.add(fromCSSValue<HangingPunctuation>(currentValue));
    }
    return result;
}

inline GapLength BuilderConverter::convertGapLength(BuilderState& builderState, const CSSValue& value)
{
    return value == CSSValueNormal ? GapLength() : GapLength(convertLength(builderState, value));
}

inline OffsetRotation BuilderConverter::convertOffsetRotate(BuilderState&, const CSSValue& value)
{
    CSSValueID modifier { };
    RefPtr<const CSSPrimitiveValue> angleValue;

    if (auto* offsetRotateValue = dynamicDowncast<CSSOffsetRotateValue>(value)) {
        modifier = offsetRotateValue->modifier();
        angleValue = offsetRotateValue->angle();
    } else if (value.isValueID())
        modifier = value.valueID();
    else
        angleValue = dynamicDowncast<CSSPrimitiveValue>(value);

    float angleInDegrees = angleValue ? static_cast<float>(angleValue->computeDegrees()) : 0;
    if (modifier == CSSValueReverse)
        angleInDegrees += 180;
    return OffsetRotation(modifier == CSSValueAuto || modifier == CSSValueReverse, angleInDegrees);
}

inline Vector<AtomString> BuilderConverter::convertContainerName(BuilderState&, const CSSValue& value)
{
    if (value == CSSValueNone)
        return { };
    return WTF::map(downcast<CSSValueList>(value), [](auto& item) {
        return AtomString { downcast<CSSCustomIdentValue>(item).string() };
    });
}

inline OptionSet<MarginTrimType> BuilderConverter::convertMarginTrim(BuilderState&, const CSSValue& value)
{
    // See if value is "block" or "inline" before trying to parse a list
    if (value == CSSValueBlock)
        return { MarginTrimType::BlockStart, MarginTrimType::BlockEnd };
    if (value == CSSValueInline)
        return { MarginTrimType::InlineStart, MarginTrimType::InlineEnd };
    auto list = dynamicDowncast<CSSValueList>(value);
    if (!list || !list->size())
        return RenderStyle::initialMarginTrim();
    OptionSet<MarginTrimType> marginTrim;
    ASSERT(list->size() <= 4);
    for (auto& item : *list) {
        if (item == CSSValueBlockStart)
            marginTrim.add(MarginTrimType::BlockStart);
        if (item == CSSValueBlockEnd)
            marginTrim.add(MarginTrimType::BlockEnd);
        if (item == CSSValueInlineStart)
            marginTrim.add(MarginTrimType::InlineStart);
        if (item == CSSValueInlineEnd)
            marginTrim.add(MarginTrimType::InlineEnd);
    }
    return marginTrim;
}

inline TextSpacingTrim BuilderConverter::convertTextSpacingTrim(BuilderState&, const CSSValue& value)
{
    if (value == CSSValueAuto)
        return { .m_trim = TextSpacingTrim::TrimType::Auto };
    return { };
}

inline TextAutospace BuilderConverter::convertTextAutospace(BuilderState&, const CSSValue& value)
{
    if (value == CSSValueAuto)
        return { .m_autoSpace = TextAutospace::TextAutospaceType::Auto };
    return { };
}

inline std::optional<Length> BuilderConverter::convertBlockStepSize(BuilderState& builderState, const CSSValue& value)
{
    if (value == CSSValueNone)
        return { };
    return convertLength(builderState, value);
}

} // namespace Style
} // namespace WebCore
