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
#include "CSSSubgridValue.h"
#include "CalcExpressionLength.h"
#include "CalcExpressionOperation.h"
#include "FontPalette.h"
#include "FontSelectionValueInlines.h"
#include "Frame.h"
#include "FrameDestructionObserverInlines.h"
#include "GridPositionsResolver.h"
#include "Length.h"
#include "Pair.h"
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
    static TabSize convertTabSize(const BuilderState&, const CSSValue&);
    template<typename T> static T convertComputedLength(BuilderState&, const CSSValue&);
    template<typename T> static T convertLineWidth(BuilderState&, const CSSValue&);
    static float convertSpacing(BuilderState&, const CSSValue&);
    static LengthSize convertRadius(BuilderState&, const CSSValue&);
    static LengthPoint convertPosition(BuilderState&, const CSSValue&);
    static LengthPoint convertPositionOrAuto(BuilderState&, const CSSValue&);
    static OptionSet<TextDecorationLine> convertTextDecorationLine(BuilderState&, const CSSValue&);
    template<typename T> static T convertNumber(BuilderState&, const CSSValue&);
    template<typename T> static T convertNumberOrAuto(BuilderState&, const CSSValue&);
    static short convertWebkitHyphenateLimitLines(BuilderState&, const CSSValue&);
    template<CSSPropertyID> static NinePieceImage convertBorderMask(BuilderState&, CSSValue&);
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
    static MasonryAutoFlow convertMasonryAutoFlow(BuilderState&, const CSSValue&);
    static std::optional<Length> convertWordSpacing(BuilderState&, const CSSValue&);
    static std::optional<float> convertPerspective(BuilderState&, const CSSValue&);
    static std::optional<Length> convertMarqueeIncrement(BuilderState&, const CSSValue&);
    static std::optional<FilterOperations> convertFilterOperations(BuilderState&, const CSSValue&);
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
    static SVGLengthValue convertSVGLengthValue(BuilderState&, const CSSValue&);
    static Vector<SVGLengthValue> convertSVGLengthVector(BuilderState&, const CSSValue&);
    static Vector<SVGLengthValue> convertStrokeDashArray(BuilderState&, const CSSValue&);
    static PaintOrder convertPaintOrder(BuilderState&, const CSSValue&);
    static float convertOpacity(BuilderState&, const CSSValue&);
    static String convertSVGURIReference(BuilderState&, const CSSValue&);
    static StyleSelfAlignmentData convertSelfOrDefaultAlignmentData(BuilderState&, const CSSValue&);
    static StyleContentAlignmentData convertContentAlignmentData(BuilderState&, const CSSValue&);
    static GlyphOrientation convertGlyphOrientation(BuilderState&, const CSSValue&);
    static GlyphOrientation convertGlyphOrientationOrAuto(BuilderState&, const CSSValue&);
    static std::optional<Length> convertLineHeight(BuilderState&, const CSSValue&, float multiplier = 1.f);
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
    
private:
    friend class BuilderCustom;

    static Length convertToRadiusLength(CSSToLengthConversionData&, const CSSPrimitiveValue&);
    static OptionSet<TextEmphasisPosition> valueToEmphasisPosition(const CSSPrimitiveValue&);
    static Length parseSnapCoordinate(BuilderState&, const CSSValue&);

#if ENABLE(DARK_MODE_CSS)
    static void updateColorScheme(const CSSPrimitiveValue&, StyleColorScheme&);
#endif

    static Length convertTo100PercentMinusLength(const Length&);
    template<CSSValueID, CSSValueID> static Length convertPositionComponent(BuilderState&, const CSSPrimitiveValue&);

    static GridLength createGridTrackBreadth(const CSSPrimitiveValue&, BuilderState&);
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

inline Length BuilderConverter::convertLengthOrAuto(const BuilderState& builderState, const CSSValue& value)
{
    if (downcast<CSSPrimitiveValue>(value).valueID() == CSSValueAuto)
        return Length(LengthType::Auto);
    return convertLength(builderState, value);
}

inline Length BuilderConverter::convertLengthSizing(const BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    switch (primitiveValue.valueID()) {
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

inline Length BuilderConverter::convertLengthMaxSizing(const BuilderState& builderState, const CSSValue& value)
{
    if (downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone)
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

inline float BuilderConverter::convertSpacing(BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueNormal)
        return 0.f;

    CSSToLengthConversionData conversionData = builderState.useSVGZoomRulesForLength() ?
        builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
        : builderState.cssToLengthConversionData();
    return primitiveValue.computeLength<float>(conversionData);
}

inline Length BuilderConverter::convertToRadiusLength(CSSToLengthConversionData& conversionData, const CSSPrimitiveValue& value)
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
    auto* pair = downcast<CSSPrimitiveValue>(value).pairValue();
    if (!pair || !pair->first() || !pair->second())
        return { { 0, LengthType::Fixed }, { 0, LengthType::Fixed } };

    CSSToLengthConversionData conversionData = builderState.cssToLengthConversionData();
    LengthSize radius { convertToRadiusLength(conversionData, *pair->first()), convertToRadiusLength(conversionData, *pair->second()) };

    ASSERT(!radius.width.isNegative());
    ASSERT(!radius.height.isNegative());
    return radius;
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
    return convertPositionComponent<CSSValueLeft, CSSValueRight>(builderState, downcast<CSSPrimitiveValue>(value));
}

inline Length BuilderConverter::convertPositionComponentY(BuilderState& builderState, const CSSValue& value)
{
    return convertPositionComponent<CSSValueTop, CSSValueBottom>(builderState, downcast<CSSPrimitiveValue>(value));
}

template<CSSValueID cssValueFor0, CSSValueID cssValueFor100>
inline Length BuilderConverter::convertPositionComponent(BuilderState& builderState, const CSSPrimitiveValue& value)
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
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    Pair* pair = primitiveValue.pairValue();
    if (!pair || !pair->first() || !pair->second())
        return RenderStyle::initialObjectPosition();

    Length lengthX = convertPositionComponent<CSSValueLeft, CSSValueRight>(builderState, *pair->first());
    Length lengthY = convertPositionComponent<CSSValueTop, CSSValueBottom>(builderState, *pair->second());

    return LengthPoint(lengthX, lengthY);
}

inline LengthPoint BuilderConverter::convertPositionOrAuto(BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);

    if (primitiveValue.isPair())
        return convertPosition(builderState, value);

    return LengthPoint(Length(LengthType::Auto), Length(LengthType::Auto));
}

inline OptionSet<TextDecorationLine> BuilderConverter::convertTextDecorationLine(BuilderState&, const CSSValue& value)
{
    auto result = RenderStyle::initialTextDecorationLine();
    if (is<CSSValueList>(value)) {
        for (auto& currentValue : downcast<CSSValueList>(value))
            result.add(downcast<CSSPrimitiveValue>(currentValue.get()));
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
    if (downcast<CSSPrimitiveValue>(value).valueID() == CSSValueAuto)
        return -1;
    return convertNumber<T>(builderState, value);
}

inline short BuilderConverter::convertWebkitHyphenateLimitLines(BuilderState&, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueNoLimit)
        return -1;
    return primitiveValue.value<short>(CSSUnitType::CSS_NUMBER);
}

template<CSSPropertyID property>
inline NinePieceImage BuilderConverter::convertBorderMask(BuilderState& builderState, CSSValue& value)
{
    NinePieceImage image(NinePieceImage::Type::Mask);
    builderState.styleMap().mapNinePieceImage(&value, image);
    return image;
}

template<CSSPropertyID>
inline RefPtr<StyleImage> BuilderConverter::convertStyleImage(BuilderState& builderState, CSSValue& value)
{
    return builderState.createStyleImage(value);
}

inline ImageOrientation BuilderConverter::convertImageOrientation(BuilderState&, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueFromImage)
        return ImageOrientation::FromImage;
    return ImageOrientation::None;
}

inline TransformOperations BuilderConverter::convertTransform(BuilderState& builderState, const CSSValue& value)
{
    TransformOperations operations;
    transformsForValue(value, builderState.cssToLengthConversionData(), operations);
    return operations;
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
inline void BuilderConverter::updateColorScheme(const CSSPrimitiveValue& primitiveValue, StyleColorScheme& colorScheme)
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

inline StyleColorScheme BuilderConverter::convertColorScheme(BuilderState&, const CSSValue& value)
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

inline String BuilderConverter::convertString(BuilderState&, const CSSValue& value)
{
    return downcast<CSSPrimitiveValue>(value).stringValue();
}

inline String BuilderConverter::convertStringOrAuto(BuilderState& builderState, const CSSValue& value)
{
    if (downcast<CSSPrimitiveValue>(value).valueID() == CSSValueAuto)
        return nullAtom();
    return convertString(builderState, value);
}

inline String BuilderConverter::convertStringOrNone(BuilderState& builderState, const CSSValue& value)
{
    if (downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone)
        return nullAtom();
    return convertString(builderState, value);
}

inline OptionSet<TextEmphasisPosition> BuilderConverter::valueToEmphasisPosition(const CSSPrimitiveValue& primitiveValue)
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

inline OptionSet<TextEmphasisPosition> BuilderConverter::convertTextEmphasisPosition(BuilderState&, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value))
        return valueToEmphasisPosition(downcast<CSSPrimitiveValue>(value));

    OptionSet<TextEmphasisPosition> position;
    for (auto& currentValue : downcast<CSSValueList>(value))
        position.add(valueToEmphasisPosition(downcast<CSSPrimitiveValue>(currentValue.get())));
    return position;
}

inline TextAlignMode BuilderConverter::convertTextAlign(BuilderState& builderState, const CSSValue& value)
{
    const auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    ASSERT(primitiveValue.isValueID());

    const auto& parentStyle = builderState.parentStyle();

    // User agents are expected to have a rule in their user agent stylesheet that matches th elements that have a parent
    // node whose computed value for the 'text-align' property is its initial value, whose declaration block consists of
    // just a single declaration that sets the 'text-align' property to the value 'center'.
    // https://html.spec.whatwg.org/multipage/rendering.html#rendering
    if (primitiveValue.valueID() == CSSValueInternalThCenter) {
        if (parentStyle.textAlign() == RenderStyle::initialTextAlign())
            return TextAlignMode::Center;
        return parentStyle.textAlign();
    }

    if (primitiveValue.valueID() == CSSValueWebkitMatchParent || primitiveValue.valueID() == CSSValueMatchParent) {
        const auto* element = builderState.element();

        if (element && element == builderState.document().documentElement())
            return TextAlignMode::Start;
        if (parentStyle.textAlign() == TextAlignMode::Start)
            return parentStyle.isLeftToRightDirection() ? TextAlignMode::Left : TextAlignMode::Right;
        if (parentStyle.textAlign() == TextAlignMode::End)
            return parentStyle.isLeftToRightDirection() ? TextAlignMode::Right : TextAlignMode::Left;

        return parentStyle.textAlign();
    }

    return primitiveValue;
}

inline TextAlignLast BuilderConverter::convertTextAlignLast(BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    ASSERT(primitiveValue.isValueID());

    if (primitiveValue.valueID() != CSSValueMatchParent)
        return primitiveValue;

    auto& parentStyle = builderState.parentStyle();
    if (parentStyle.textAlignLast() == TextAlignLast::Start)
        return parentStyle.isLeftToRightDirection() ? TextAlignLast::Left : TextAlignLast::Right;
    if (parentStyle.textAlignLast() == TextAlignLast::End)
        return parentStyle.isLeftToRightDirection() ? TextAlignLast::Right : TextAlignLast::Left;
    return parentStyle.textAlignLast();
}

inline RefPtr<PathOperation> BuilderConverter::convertPathOperation(BuilderState& builderState, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
        if (primitiveValue.primitiveType() == CSSUnitType::CSS_URI) {
            auto cssURLValue = primitiveValue.stringValue();
            auto fragment = SVGURIReference::fragmentIdentifierFromIRIString(cssURLValue, builderState.document());
            // FIXME: It doesn't work with external SVG references (see https://bugs.webkit.org/show_bug.cgi?id=126133)
            auto target = SVGURIReference::targetElementFromIRIString(cssURLValue, builderState.document());
            return ReferencePathOperation::create(cssURLValue, fragment, dynamicDowncast<SVGElement>(target.element.get()));
        }
        ASSERT(primitiveValue.valueID() == CSSValueNone);
        return nullptr;
    }

    if (is<CSSRayValue>(value)) {
        auto& rayValue = downcast<CSSRayValue>(value);

        RayPathOperation::Size size = RayPathOperation::Size::ClosestCorner;
        switch (rayValue.size()->valueID()) {
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

        return RayPathOperation::create(rayValue.angle()->computeDegrees(), size, rayValue.isContaining());
    }

    CSSBoxType referenceBox = CSSBoxType::BoxMissing;
    RefPtr<PathOperation> operation;

    for (auto& currentValue : downcast<CSSValueList>(value)) {
        auto& primitiveValue = downcast<CSSPrimitiveValue>(currentValue.get());
        if (primitiveValue.isShape()) {
            ASSERT(!operation);
            operation = ShapePathOperation::create(
                basicShapeForValue(builderState.cssToLengthConversionData(), 
                *primitiveValue.shapeValue(),
                builderState.style().effectiveZoom()));
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
        downcast<ShapePathOperation>(*operation).setReferenceBox(referenceBox);
    else {
        ASSERT(referenceBox != CSSBoxType::BoxMissing);
        operation = BoxPathOperation::create(referenceBox);
    }

    return operation;
}

inline Resize BuilderConverter::convertResize(BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);

    Resize resize = Resize::None;
    if (primitiveValue.valueID() == CSSValueAuto)
        resize = builderState.document().settings().textAreasAreResizable() ? Resize::Both : Resize::None;
    else
        resize = primitiveValue;

    return resize;
}

inline int BuilderConverter::convertMarqueeRepetition(BuilderState&, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueInfinite)
        return -1; // -1 means repeat forever.

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
    if (is<CSSPrimitiveValue>(value)) {
        auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
        if (primitiveValue.valueID() == CSSValueNone)
            return QuotesData::create({ });
        ASSERT(primitiveValue.valueID() == CSSValueAuto);
        return nullptr;
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

inline TextUnderlinePosition BuilderConverter::convertTextUnderlinePosition(BuilderState&, const CSSValue& value)
{
    return downcast<CSSPrimitiveValue>(value);
}

inline TextUnderlineOffset BuilderConverter::convertTextUnderlineOffset(BuilderState& builderState, const CSSValue& value)
{
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

inline TextDecorationThickness BuilderConverter::convertTextDecorationThickness(BuilderState& builderState, const CSSValue& value)
{
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

inline RefPtr<StyleReflection> BuilderConverter::convertReflection(BuilderState& builderState, const CSSValue& value)
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
    builderState.styleMap().mapNinePieceImage(reflectValue.mask(), mask);
    reflection->setMask(mask);

    return reflection;
}

inline TextEdge BuilderConverter::convertTextEdge(BuilderState&, const CSSValue& value)
{
    auto& values = downcast<CSSValueList>(value);
    auto& firstValue = downcast<CSSPrimitiveValue>(*values.item(0));

    auto overValue = [&] {
        switch (firstValue.valueID()) {
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
        if (firstValue.valueID() == CSSValueLeading)
            return TextEdgeType::Leading;
        if (values.length() == 1) {
            // https://www.w3.org/TR/css-inline-3/#text-edges
            // "If only one value is specified, both edges are assigned that same keyword if possible; else text is assumed as the missing value."
            // FIXME: Figure out what "if possible" means here.
            return TextEdgeType::Text;
        }

        auto& secondValue = downcast<CSSPrimitiveValue>(*values.item(1));
        switch (secondValue.valueID()) {
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
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);

    if (primitiveValue.valueID() == CSSValueNormal)
        return IntSize();

    Pair* pair = primitiveValue.pairValue();
    ASSERT(pair);
    ASSERT(pair->first());
    ASSERT(pair->second());

    return IntSize(pair->first()->intValue(), pair->second()->intValue());
}

inline float BuilderConverter::convertTextStrokeWidth(BuilderState& builderState, const CSSValue& value)
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
        Ref<CSSPrimitiveValue> emsValue(CSSPrimitiveValue::create(result, CSSUnitType::CSS_EMS));
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

inline OptionSet<LineBoxContain> BuilderConverter::convertLineBoxContain(BuilderState&, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone);
        return { };
    }

    return downcast<CSSLineBoxContainValue>(value).value();
}

inline RefPtr<ShapeValue> BuilderConverter::convertShapeValue(BuilderState& builderState, CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone);
        return nullptr;
    }

    if (value.isImage())
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

inline ScrollSnapType BuilderConverter::convertScrollSnapType(BuilderState&, const CSSValue& value)
{
    ScrollSnapType type;
    auto& values = downcast<CSSValueList>(value);
    auto& firstValue = downcast<CSSPrimitiveValue>(*values.item(0));
    if (firstValue.valueID() == CSSValueNone)
        return type;

    type.axis = firstValue;
    if (values.length() == 2)
        type.strictness = downcast<CSSPrimitiveValue>(*values.item(1));
    else
        type.strictness = ScrollSnapStrictness::Proximity;

    return type;
}

inline ScrollSnapAlign BuilderConverter::convertScrollSnapAlign(BuilderState&, const CSSValue& value)
{
    auto& values = downcast<CSSValueList>(value);
    ScrollSnapAlign alignment;
    alignment.blockAlign = downcast<CSSPrimitiveValue>(*values.item(0));
    if (values.length() == 1)
        alignment.inlineAlign = alignment.blockAlign;
    else
        alignment.inlineAlign = downcast<CSSPrimitiveValue>(*values.item(1));
    return alignment;
}

inline ScrollSnapStop BuilderConverter::convertScrollSnapStop(BuilderState&, const CSSValue& value)
{
    return downcast<CSSPrimitiveValue>(value);
}

inline GridLength BuilderConverter::createGridTrackBreadth(const CSSPrimitiveValue& primitiveValue, BuilderState& builderState)
{
    if (primitiveValue.valueID() == CSSValueMinContent || primitiveValue.valueID() == CSSValueWebkitMinContent)
        return Length(LengthType::MinContent);

    if (primitiveValue.valueID() == CSSValueMaxContent || primitiveValue.valueID() == CSSValueWebkitMaxContent)
        return Length(LengthType::MaxContent);

    // Fractional unit.
    if (primitiveValue.isFlex())
        return GridLength(primitiveValue.doubleValue());

    return primitiveValue.convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion | AutoConversion>(builderState.cssToLengthConversionData());
}

inline GridTrackSize BuilderConverter::createGridTrackSize(const CSSValue& value, BuilderState& builderState)
{
    if (is<CSSPrimitiveValue>(value))
        return GridTrackSize(createGridTrackBreadth(downcast<CSSPrimitiveValue>(value), builderState));

    const auto& function = downcast<CSSFunctionValue>(value);

    if (function.length() == 1)
        return GridTrackSize(createGridTrackBreadth(downcast<CSSPrimitiveValue>(*function.itemWithoutBoundsCheck(0)), builderState), FitContentTrackSizing);

    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(function.length() == 2);
    GridLength minTrackBreadth(createGridTrackBreadth(downcast<CSSPrimitiveValue>(*function.itemWithoutBoundsCheck(0)), builderState));
    GridLength maxTrackBreadth(createGridTrackBreadth(downcast<CSSPrimitiveValue>(*function.itemWithoutBoundsCheck(1)), builderState));
    return GridTrackSize(minTrackBreadth, maxTrackBreadth);
}

inline bool BuilderConverter::createGridTrackList(const CSSValue& value, GridTrackList& trackList, BuilderState& builderState)
{
    // Handle 'none' or 'masonry'.
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        if (primitiveValue->valueID() == CSSValueMasonry) {

            trackList.append(GridTrackEntryMasonry());
            return true;
        }
        return primitiveValue->valueID() == CSSValueNone;
    }

    if (!is<CSSValueList>(value))
        return false;

    bool isSubgrid = false;
    if (is<CSSSubgridValue>(value)) {
        isSubgrid = true;
        trackList.append(GridTrackEntrySubgrid());
    }

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
        for (auto& currentValue : downcast<CSSValueList>(repeatValue)) {
            if (is<CSSGridLineNamesValue>(currentValue)) {
                Vector<String> names;
                for (auto& namedGridLineValue : downcast<CSSGridLineNamesValue>(currentValue.get()))
                    names.append(downcast<CSSPrimitiveValue>(namedGridLineValue.get()).stringValue());
                repeatList.append(WTFMove(names));
            } else {
                ensureLineNames(repeatList);
                repeatList.append(createGridTrackSize(currentValue, builderState));
            }
        }

        if (!repeatList.isEmpty())
            ensureLineNames(repeatList);
    };

    for (auto& currentValue : downcast<CSSValueList>(value)) {
        if (is<CSSGridLineNamesValue>(currentValue)) {
            Vector<String> names;
            for (auto& namedGridLineValue : downcast<CSSGridLineNamesValue>(currentValue.get()))
                names.append(downcast<CSSPrimitiveValue>(namedGridLineValue.get()).stringValue());
            trackList.append(WTFMove(names));
            continue;
        }

        ensureLineNames(trackList);

        if (is<CSSGridAutoRepeatValue>(currentValue)) {
            CSSValueID autoRepeatID = downcast<CSSGridAutoRepeatValue>(currentValue.get()).autoRepeatID();
            ASSERT(autoRepeatID == CSSValueAutoFill || autoRepeatID == CSSValueAutoFit);

            GridTrackEntryAutoRepeat repeat;
            repeat.type = autoRepeatID == CSSValueAutoFill ? AutoRepeatType::Fill : AutoRepeatType::Fit;

            buildRepeatList(currentValue, repeat.list);
            trackList.append(WTFMove(repeat));
        } else if (is<CSSGridIntegerRepeatValue>(currentValue)) {
            GridTrackEntryRepeat repeat;
            repeat.repeats = downcast<CSSGridIntegerRepeatValue>(currentValue.get()).repetitions();

            buildRepeatList(currentValue, repeat.list);
            trackList.append(WTFMove(repeat));
        } else {
            trackList.append(createGridTrackSize(currentValue, builderState));
        }
    }

    if (!trackList.isEmpty())
        ensureLineNames(trackList);

    return true;
}

inline bool BuilderConverter::createGridPosition(const CSSValue& value, GridPosition& position)
{
    // We accept the specification's grammar:
    // auto | <custom-ident> | [ <integer> && <custom-ident>? ] | [ span && [ <integer> || <custom-ident> ] ]
    if (is<CSSPrimitiveValue>(value)) {
        auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
        if (primitiveValue.isCustomIdent()) {
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
    if (currentValue && currentValue->isInteger()) {
        gridLineNumber = currentValue->intValue();
        ++it;
        currentValue = it != values.end() ? &downcast<CSSPrimitiveValue>(it->get()) : nullptr;
    }

    String gridLineName;
    if (currentValue && currentValue->isCustomIdent()) {
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
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).isValueID() && downcast<CSSPrimitiveValue>(value).valueID() == CSSValueAuto);
        return RenderStyle::initialGridAutoRows();
    }

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
    ASSERT(!is<CSSPrimitiveValue>(value) || downcast<CSSPrimitiveValue>(value).isValueID());

    bool isValuelist = is<CSSValueList>(value);
    if (isValuelist) {
        auto& list = downcast<CSSValueList>(value);
        if (!list.length())
            return RenderStyle::initialGridAutoFlow();
    }

    auto& first = downcast<CSSPrimitiveValue>(isValuelist ? *(downcast<CSSValueList>(value).item(0)) : value);
    auto* second = downcast<CSSPrimitiveValue>(isValuelist && downcast<CSSValueList>(value).length() == 2 ? downcast<CSSValueList>(value).item(1) : nullptr);

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

inline MasonryAutoFlow BuilderConverter::convertMasonryAutoFlow(BuilderState&, const CSSValue& value)
{
    auto& valueList = downcast<CSSValueList>(value);
    ASSERT(valueList.size() == 1 || valueList.size() == 2);
    if (!(valueList.size() == 1 || valueList.size() == 2))
        return RenderStyle::initialMasonryAutoFlow();

    auto* firstValue = downcast<CSSPrimitiveValue>(valueList.item(0));
    auto* secondValue = downcast<CSSPrimitiveValue>(valueList.length() == 2 ? valueList.item(1) : nullptr);
    MasonryAutoFlow masonryAutoFlow;
    if (valueList.size() == 2 && firstValue && secondValue) {
        ASSERT(firstValue->valueID() == CSSValueID::CSSValuePack || firstValue->valueID() == CSSValueID::CSSValueNext);
        ASSERT(secondValue->valueID() == CSSValueID::CSSValueOrdered);
        if (firstValue->valueID() == CSSValueID::CSSValuePack)
            masonryAutoFlow = { MasonryAutoFlowPlacementAlgorithm::Pack, MasonryAutoFlowPlacementOrder::Ordered };
        else
            masonryAutoFlow = { MasonryAutoFlowPlacementAlgorithm::Next, MasonryAutoFlowPlacementOrder::Ordered };

    } else if (valueList.size() == 1 && firstValue)  {
        if (firstValue->valueID() == CSSValueID::CSSValuePack)
            masonryAutoFlow = { MasonryAutoFlowPlacementAlgorithm::Pack, MasonryAutoFlowPlacementOrder::DefiniteFirst };
        else if (firstValue->valueID() == CSSValueID::CSSValueNext)
            masonryAutoFlow = { MasonryAutoFlowPlacementAlgorithm::Next, MasonryAutoFlowPlacementOrder::DefiniteFirst };
        else if (firstValue->valueID() == CSSValueID::CSSValueOrdered)
            masonryAutoFlow = { MasonryAutoFlowPlacementAlgorithm::Pack, MasonryAutoFlowPlacementOrder::Ordered };
        else {
            ASSERT_NOT_REACHED();
            return RenderStyle::initialMasonryAutoFlow();
        }
    } else {
        ASSERT_NOT_REACHED();
        return RenderStyle::initialMasonryAutoFlow();
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
    std::optional<Length> wordSpacing;
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueNormal)
        wordSpacing = RenderStyle::initialWordSpacing();
    else if (primitiveValue.isLength())
        wordSpacing = primitiveValue.computeLength<Length>(csstoLengthConversionDataWithTextZoomFactor(builderState));
    else if (primitiveValue.isPercentage())
        wordSpacing = Length(clampTo<double>(primitiveValue.doubleValue(), minValueForCssLength, maxValueForCssLength), LengthType::Percent);
    else if (primitiveValue.isNumber())
        wordSpacing = Length(primitiveValue.doubleValue(), LengthType::Fixed);

    return wordSpacing;
}

inline std::optional<float> BuilderConverter::convertPerspective(BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueNone)
        return RenderStyle::initialPerspective();

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
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNormal || CSSPropertyParserHelpers::isSystemFontShorthand(downcast<CSSPrimitiveValue>(value).valueID()));
        return { };
    }

    FontFeatureSettings settings;
    for (auto& item : downcast<CSSValueList>(value)) {
        auto& feature = downcast<CSSFontFeatureValue>(item.get());
        settings.insert(FontFeature(feature.tag(), feature.value()));
    }
    return settings;
}

inline FontSelectionValue BuilderConverter::convertFontWeightFromValue(const CSSValue& value)
{
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

inline FontSelectionValue BuilderConverter::convertFontStretchFromValue(const CSSValue& value)
{
    const auto& primitiveValue = downcast<CSSPrimitiveValue>(value);

    if (primitiveValue.isPercentage())
        return FontSelectionValue::clampFloat(primitiveValue.floatValue());

    ASSERT(primitiveValue.isValueID());
    if (auto value = fontStretchValue(primitiveValue.valueID()))
        return value.value();
    ASSERT(CSSPropertyParserHelpers::isSystemFontShorthand(primitiveValue.valueID()));
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

    auto valueID = downcast<CSSPrimitiveValue>(value).valueID();
    if (valueID == CSSValueNormal)
        return std::nullopt;
    ASSERT(valueID == CSSValueItalic || valueID == CSSValueOblique);
    return italicValue();
}

inline FontSelectionValue BuilderConverter::convertFontWeight(BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.isValueID()) {
        auto valueID = primitiveValue.valueID();
        if (valueID == CSSValueBolder)
            return FontCascadeDescription::bolderWeight(builderState.parentStyle().fontDescription().weight());
        if (valueID == CSSValueLighter)
            return FontCascadeDescription::lighterWeight(builderState.parentStyle().fontDescription().weight());
        if (CSSPropertyParserHelpers::isSystemFontShorthand(valueID))
            return SystemFontDatabase::singleton().systemFontShorthandWeight(CSSPropertyParserHelpers::lowerFontShorthand(valueID));
    }
    return convertFontWeightFromValue(value);
}

inline FontSelectionValue BuilderConverter::convertFontStretch(BuilderState&, const CSSValue& value)
{
    return convertFontStretchFromValue(value);
}

inline FontVariationSettings BuilderConverter::convertFontVariationSettings(BuilderState&, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNormal || CSSPropertyParserHelpers::isSystemFontShorthand(downcast<CSSPrimitiveValue>(value).valueID()));
        return { };
    }

    FontVariationSettings settings;
    for (auto& item : downcast<CSSValueList>(value)) {
        auto& feature = downcast<CSSFontVariationValue>(item.get());
        settings.insert({ feature.tag(), feature.value() });
    }
    return settings;
}

#if PLATFORM(IOS_FAMILY)
inline bool BuilderConverter::convertTouchCallout(BuilderState&, const CSSValue& value)
{
    return !equalLettersIgnoringASCIICase(downcast<CSSPrimitiveValue>(value).stringValue(), "none"_s);
}
#endif

#if ENABLE(TOUCH_EVENTS)
inline StyleColor BuilderConverter::convertTapHighlightColor(BuilderState& builderState, const CSSValue& value)
{
    return builderState.colorFromPrimitiveValue(downcast<CSSPrimitiveValue>(value));
}
#endif

inline OptionSet<TouchAction> BuilderConverter::convertTouchAction(BuilderState&, const CSSValue& value)
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

#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
inline bool BuilderConverter::convertOverflowScrolling(BuilderState&, const CSSValue& value)
{
    return downcast<CSSPrimitiveValue>(value).valueID() == CSSValueTouch;
}
#endif

inline bool BuilderConverter::convertSmoothScrolling(BuilderState&, const CSSValue& value)
{
    return downcast<CSSPrimitiveValue>(value).valueID() == CSSValueSmooth;
}

inline SVGLengthValue BuilderConverter::convertSVGLengthValue(BuilderState&, const CSSValue& value)
{
    return SVGLengthValue::fromCSSPrimitiveValue(downcast<CSSPrimitiveValue>(value));
}

inline Vector<SVGLengthValue> BuilderConverter::convertSVGLengthVector(BuilderState& builderState, const CSSValue& value)
{
    auto& valueList = downcast<CSSValueList>(value);

    Vector<SVGLengthValue> svgLengths;
    svgLengths.reserveInitialCapacity(valueList.length());
    for (auto& item : valueList)
        svgLengths.uncheckedAppend(convertSVGLengthValue(builderState, item));

    return svgLengths;
}

inline Vector<SVGLengthValue> BuilderConverter::convertStrokeDashArray(BuilderState& builderState, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone);
        return SVGRenderStyle::initialStrokeDashArray();
    }

    return convertSVGLengthVector(builderState, value);
}

inline PaintOrder BuilderConverter::convertPaintOrder(BuilderState&, const CSSValue& value)
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

inline float BuilderConverter::convertOpacity(BuilderState&, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    float opacity = primitiveValue.floatValue();
    if (primitiveValue.isPercentage())
        opacity /= 100.0f;
    return std::max(0.0f, std::min(1.0f, opacity));
}

inline String BuilderConverter::convertSVGURIReference(BuilderState& builderState, const CSSValue& value)
{
    String s;
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.isURI())
        s = primitiveValue.stringValue();

    return SVGURIReference::fragmentIdentifierFromIRIString(s, builderState.document());
}

inline StyleSelfAlignmentData BuilderConverter::convertSelfOrDefaultAlignmentData(BuilderState&, const CSSValue& value)
{
    StyleSelfAlignmentData alignmentData = RenderStyle::initialSelfAlignment();
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (Pair* pairValue = primitiveValue.pairValue()) {
        if (pairValue->first()->valueID() == CSSValueLegacy) {
            alignmentData.setPositionType(ItemPositionType::Legacy);
            alignmentData.setPosition(*pairValue->second());
        } else if (pairValue->first()->valueID() == CSSValueFirst)
            alignmentData.setPosition(ItemPosition::Baseline);
        else if (pairValue->first()->valueID() == CSSValueLast)
            alignmentData.setPosition(ItemPosition::LastBaseline);
        else {
            alignmentData.setOverflow(*pairValue->first());
            alignmentData.setPosition(*pairValue->second());
        }
    } else
        alignmentData.setPosition(primitiveValue);
    return alignmentData;
}

inline StyleContentAlignmentData BuilderConverter::convertContentAlignmentData(BuilderState&, const CSSValue& value)
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

inline GlyphOrientation BuilderConverter::convertGlyphOrientation(BuilderState&, const CSSValue& value)
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

inline GlyphOrientation BuilderConverter::convertGlyphOrientationOrAuto(BuilderState& builderState, const CSSValue& value)
{
    if (downcast<CSSPrimitiveValue>(value).valueID() == CSSValueAuto)
        return GlyphOrientation::Auto;
    return convertGlyphOrientation(builderState, value);
}

inline std::optional<Length> BuilderConverter::convertLineHeight(BuilderState& builderState, const CSSValue& value, float multiplier)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    auto valueID = primitiveValue.valueID();
    if (valueID == CSSValueNormal)
        return RenderStyle::initialLineHeight();

    if (CSSPropertyParserHelpers::isSystemFontShorthand(valueID))
        return RenderStyle::initialLineHeight();

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
    if (primitiveValue.isNumber())
        return Length(primitiveValue.doubleValue() * 100.0, LengthType::Percent);

    // FIXME: The parser should only emit the above types, so this should never be reached. We should change the
    // type of this function to return just a Length (and not an Optional).
    return std::nullopt;
}

inline FontPalette BuilderConverter::convertFontPalette(BuilderState&, const CSSValue& value)
{
    const auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    switch (primitiveValue.valueID()) {
    case CSSValueLight:
        return { FontPalette::Type::Light, nullAtom() };
    case CSSValueDark:
        return { FontPalette::Type::Dark, nullAtom() };
    case CSSValueInvalid:
        ASSERT(primitiveValue.isCustomIdent());
        return { FontPalette::Type::Custom, AtomString { primitiveValue.stringValue() } };
    default:
        ASSERT(primitiveValue.valueID() == CSSValueNormal || CSSPropertyParserHelpers::isSystemFontShorthand(primitiveValue.valueID()));
        return { FontPalette::Type::Normal, nullAtom() };
    }
}
    
inline OptionSet<SpeakAs> BuilderConverter::convertSpeakAs(BuilderState&, const CSSValue& value)
{
    auto result = RenderStyle::initialSpeakAs();
    if (is<CSSValueList>(value)) {
        for (auto& currentValue : downcast<CSSValueList>(value))
            result.add(downcast<CSSPrimitiveValue>(currentValue.get()));
    }
    return result;
}

inline OptionSet<HangingPunctuation> BuilderConverter::convertHangingPunctuation(BuilderState&, const CSSValue& value)
{
    auto result = RenderStyle::initialHangingPunctuation();
    if (is<CSSValueList>(value)) {
        for (auto& currentValue : downcast<CSSValueList>(value))
            result.add(downcast<CSSPrimitiveValue>(currentValue.get()));
    }
    return result;
}

inline GapLength BuilderConverter::convertGapLength(BuilderState& builderState, const CSSValue& value)
{
    return (downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNormal) ? GapLength() : GapLength(convertLength(builderState, value));
}

inline OffsetRotation BuilderConverter::convertOffsetRotate(BuilderState&, const CSSValue& value)
{
    bool hasAuto = false;
    float angleInDegrees = 0;

    const auto& offsetRotateValue = downcast<CSSOffsetRotateValue>(value);

    if (auto* angleValue = offsetRotateValue.angle())
        angleInDegrees = static_cast<float>(angleValue->computeDegrees());

    if (auto* modifierValue = offsetRotateValue.modifier()) {
        switch (modifierValue->valueID()) {
        case CSSValueAuto:
            hasAuto = true;
            break;
        case CSSValueReverse:
            hasAuto = true;
            angleInDegrees += 180.0;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }

    return OffsetRotation(hasAuto, angleInDegrees);
}

inline Vector<AtomString> BuilderConverter::convertContainerName(BuilderState&, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone);
        return { };
    }
    if (!is<CSSValueList>(value))
        return { };

    return WTF::map(downcast<CSSValueList>(value), [](auto& item) {
        return AtomString { downcast<CSSPrimitiveValue>(item.get()).stringValue() };
    });
}

} // namespace Style
} // namespace WebCore
