/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2014-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2023 ChangSeok Oh <changseok@webkit.org>
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
#include "BasicShapeConversion.h"
#include "BlockEllipsis.h"
#include "CSSBasicShapes.h"
#include "CSSCalcSymbolTable.h"
#include "CSSCalcValue.h"
#include "CSSContentDistributionValue.h"
#include "CSSCounterStyleRegistry.h"
#include "CSSFontFeatureValue.h"
#include "CSSFontStyleWithAngleValue.h"
#include "CSSFontVariationValue.h"
#include "CSSFunctionValue.h"
#include "CSSGridAutoRepeatValue.h"
#include "CSSGridIntegerRepeatValue.h"
#include "CSSGridLineNamesValue.h"
#include "CSSGridLineValue.h"
#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
#include "CSSOffsetRotateValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyParserConsumer+Font.h"
#include "CSSPropertyParserHelpers.h"
#include "CSSRayValue.h"
#include "CSSReflectValue.h"
#include "CSSSubgridValue.h"
#include "CSSTimingFunctionValue.h"
#include "CSSValuePair.h"
#include "CalculationValue.h"
#include "FontPalette.h"
#include "FontSelectionValueInlines.h"
#include "FontSizeAdjust.h"
#include "FrameDestructionObserverInlines.h"
#include "GridPositionsResolver.h"
#include "LineClampValue.h"
#include "LocalFrame.h"
#include "Quirks.h"
#include "QuotesData.h"
#include "RenderStyleInlines.h"
#include "SVGElementTypeHelpers.h"
#include "SVGPathElement.h"
#include "SVGRenderStyle.h"
#include "SVGURIReference.h"
#include "ScrollAxis.h"
#include "ScrollbarColor.h"
#include "ScrollbarGutter.h"
#include "Settings.h"
#include "StyleBuilderState.h"
#include "StyleReflection.h"
#include "StyleResolveForFont.h"
#include "StyleScrollSnapPoints.h"
#include "StyleTextEdge.h"
#include "TabSize.h"
#include "TextSpacing.h"
#include "TimelineRange.h"
#include "TouchAction.h"
#include "TransformOperationsBuilder.h"
#include "ViewTimeline.h"
#include "WillChangeData.h"
#include <wtf/text/MakeString.h>

namespace WebCore {
namespace Style {

// FIXME: Some of those functions assume the CSS parser only allows valid CSSValue types.
// This might not be true if we pass the CSSValue from js via CSS Typed OM.

class BuilderConverter {
public:
    static WebCore::Length convertLength(const BuilderState&, const CSSValue&);
    static WebCore::Length convertLengthOrAuto(const BuilderState&, const CSSValue&);
    static WebCore::Length convertLengthOrAutoOrContent(const BuilderState&, const CSSValue&);
    static WebCore::Length convertLengthSizing(const BuilderState&, const CSSValue&);
    static WebCore::Length convertLengthMaxSizing(const BuilderState&, const CSSValue&);
    static WebCore::Length convertLengthAllowingNumber(const BuilderState&, const CSSValue&); // Assumes unit is 'px' if input is a number.
    static WebCore::Length convertTextLengthOrNormal(const BuilderState&, const CSSValue&); // Converts length by text zoom factor, normal to zero
    static TabSize convertTabSize(const BuilderState&, const CSSValue&);
    template<typename T> static T convertComputedLength(const BuilderState&, const CSSValue&);
    template<typename T> static T convertLineWidth(const BuilderState&, const CSSValue&);
    static LengthSize convertRadius(const BuilderState&, const CSSValue&);
    static LengthPoint convertPosition(const BuilderState&, const CSSValue&);
    static LengthPoint convertPositionOrAuto(const BuilderState&, const CSSValue&);
    static LengthPoint convertPositionOrAutoOrNormal(const BuilderState&, const CSSValue&);
    static OptionSet<TextDecorationLine> convertTextDecorationLine(const BuilderState&, const CSSValue&);
    static OptionSet<TextTransform> convertTextTransform(const BuilderState&, const CSSValue&);
    template<typename T> static T convertNumber(const BuilderState&, const CSSValue&);
    template<typename T> static T convertNumberOrAuto(const BuilderState&, const CSSValue&);
    static short convertWebkitHyphenateLimitLines(const BuilderState&, const CSSValue&);
    template<CSSPropertyID> static RefPtr<StyleImage> convertStyleImage(const BuilderState&, CSSValue&);
    static ImageOrientation convertImageOrientation(const BuilderState&, const CSSValue&);
    static TransformOperations convertTransform(const BuilderState&, const CSSValue&);
    static RefPtr<RotateTransformOperation> convertRotate(const BuilderState&, const CSSValue&);
    static RefPtr<ScaleTransformOperation> convertScale(const BuilderState&, const CSSValue&);
    static RefPtr<TranslateTransformOperation> convertTranslate(const BuilderState&, const CSSValue&);
#if ENABLE(DARK_MODE_CSS)
    static StyleColorScheme convertColorScheme(const BuilderState&, const CSSValue&);
#endif
    static String convertString(const BuilderState&, const CSSValue&);
    static String convertStringOrAuto(const BuilderState&, const CSSValue&);
    static AtomString convertStringOrAutoAtom(const BuilderState& state, const CSSValue& value) { return AtomString { convertStringOrAuto(state, value) }; }
    static String convertStringOrNone(const BuilderState&, const CSSValue&);
    static AtomString convertStringOrNoneAtom(const BuilderState& state, const CSSValue& value) { return AtomString { convertStringOrNone(state, value) }; }
    static OptionSet<TextEmphasisPosition> convertTextEmphasisPosition(const BuilderState&, const CSSValue&);
    static TextAlignMode convertTextAlign(const BuilderState&, const CSSValue&);
    static TextAlignLast convertTextAlignLast(const BuilderState&, const CSSValue&);
    static RefPtr<BasicShapePath> convertSVGPath(const BuilderState&, const CSSValue&);
    static RefPtr<PathOperation> convertPathOperation(const BuilderState&, const CSSValue&);
    static RefPtr<PathOperation> convertRayPathOperation(const BuilderState&, const CSSValue&);
    static Resize convertResize(const BuilderState&, const CSSValue&);
    static int convertMarqueeRepetition(const BuilderState&, const CSSValue&);
    static int convertMarqueeSpeed(const BuilderState&, const CSSValue&);
    static RefPtr<QuotesData> convertQuotes(const BuilderState&, const CSSValue&);
    static OptionSet<TextUnderlinePosition> convertTextUnderlinePosition(const BuilderState&, const CSSValue&);
    static TextUnderlineOffset convertTextUnderlineOffset(const BuilderState&, const CSSValue&);
    static TextDecorationThickness convertTextDecorationThickness(const BuilderState&, const CSSValue&);
    static RefPtr<StyleReflection> convertReflection(const BuilderState&, const CSSValue&);
    static TextEdge convertTextEdge(const BuilderState&, const CSSValue&);
    static IntSize convertInitialLetter(const BuilderState&, const CSSValue&);
    static float convertTextStrokeWidth(const BuilderState&, const CSSValue&);
    static OptionSet<LineBoxContain> convertLineBoxContain(const BuilderState&, const CSSValue&);
    static RefPtr<ShapeValue> convertShapeValue(const BuilderState&, CSSValue&);
    static ScrollSnapType convertScrollSnapType(const BuilderState&, const CSSValue&);
    static ScrollSnapAlign convertScrollSnapAlign(const BuilderState&, const CSSValue&);
    static ScrollSnapStop convertScrollSnapStop(const BuilderState&, const CSSValue&);
    static std::optional<ScrollbarColor> convertScrollbarColor(const BuilderState&, const CSSValue&);
    static ScrollbarGutter convertScrollbarGutter(const BuilderState&, const CSSValue&);
    // scrollbar-width converter is only needed for quirking.
    static ScrollbarWidth convertScrollbarWidth(const BuilderState&, const CSSValue&);
    static GridTrackSize convertGridTrackSize(const BuilderState&, const CSSValue&);
    static Vector<GridTrackSize> convertGridTrackSizeList(const BuilderState&, const CSSValue&);
    static std::optional<GridTrackList> convertGridTrackList(const BuilderState&, const CSSValue&);
    static GridPosition convertGridPosition(const BuilderState&, const CSSValue&);
    static GridAutoFlow convertGridAutoFlow(const BuilderState&, const CSSValue&);
    static Vector<StyleContentAlignmentData> convertContentAlignmentDataList(const BuilderState&, const CSSValue&);
    static MasonryAutoFlow convertMasonryAutoFlow(const BuilderState&, const CSSValue&);
    static std::optional<float> convertPerspective(const BuilderState&, const CSSValue&);
    static std::optional<WebCore::Length> convertMarqueeIncrement(const BuilderState&, const CSSValue&);
    static FilterOperations convertFilterOperations(const BuilderState&, const CSSValue&);
    static ListStyleType convertListStyleType(const BuilderState&, const CSSValue&);
#if PLATFORM(IOS_FAMILY)
    static bool convertTouchCallout(const BuilderState&, const CSSValue&);
#endif
#if ENABLE(TOUCH_EVENTS)
    static StyleColor convertTapHighlightColor(const BuilderState&, const CSSValue&);
#endif
    static OptionSet<TouchAction> convertTouchAction(const BuilderState&, const CSSValue&);
#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
    static bool convertOverflowScrolling(const BuilderState&, const CSSValue&);
#endif
    static bool convertSmoothScrolling(const BuilderState&, const CSSValue&);

    static FontSizeAdjust convertFontSizeAdjust(const BuilderState&, const CSSValue&);
    static std::optional<FontSelectionValue> convertFontStyleFromValue(const BuilderState&, const CSSValue&);
    static FontSelectionValue convertFontWeight(const BuilderState&, const CSSValue&);
    static FontSelectionValue convertFontStretch(const BuilderState&, const CSSValue&);
    static FontSelectionValue convertFontStyle(const BuilderState&, const CSSValue&);
    static FontFeatureSettings convertFontFeatureSettings(const BuilderState&, const CSSValue&);
    static FontVariationSettings convertFontVariationSettings(const BuilderState&, const CSSValue&);
    static SVGLengthValue convertSVGLengthValue(const BuilderState&, const CSSValue&, ShouldConvertNumberToPxLength = ShouldConvertNumberToPxLength::No);
    static Vector<SVGLengthValue> convertSVGLengthVector(const BuilderState&, const CSSValue&, ShouldConvertNumberToPxLength = ShouldConvertNumberToPxLength::No);
    static Vector<SVGLengthValue> convertStrokeDashArray(const BuilderState&, const CSSValue&);
    static PaintOrder convertPaintOrder(const BuilderState&, const CSSValue&);
    static float convertOpacity(const BuilderState&, const CSSValue&);
    static String convertSVGURIReference(const BuilderState&, const CSSValue&);
    static StyleSelfAlignmentData convertSelfOrDefaultAlignmentData(const BuilderState&, const CSSValue&);
    static StyleContentAlignmentData convertContentAlignmentData(const BuilderState&, const CSSValue&);
    static GlyphOrientation convertGlyphOrientation(const BuilderState&, const CSSValue&);
    static GlyphOrientation convertGlyphOrientationOrAuto(const BuilderState&, const CSSValue&);
    static WebCore::Length convertLineHeight(const BuilderState&, const CSSValue&, float multiplier = 1.f);
    static FontPalette convertFontPalette(const BuilderState&, const CSSValue&);
    
    static BreakBetween convertPageBreakBetween(const BuilderState&, const CSSValue&);
    static BreakInside convertPageBreakInside(const BuilderState&, const CSSValue&);
    static BreakBetween convertColumnBreakBetween(const BuilderState&, const CSSValue&);
    static BreakInside convertColumnBreakInside(const BuilderState&, const CSSValue&);

    static OptionSet<HangingPunctuation> convertHangingPunctuation(const BuilderState&, const CSSValue&);

    static OptionSet<SpeakAs> convertSpeakAs(const BuilderState&, const CSSValue&);

    static WebCore::Length convertPositionComponentX(const BuilderState&, const CSSValue&);
    static WebCore::Length convertPositionComponentY(const BuilderState&, const CSSValue&);

    static GapLength convertGapLength(const BuilderState&, const CSSValue&);

    static OffsetRotation convertOffsetRotate(const BuilderState&, const CSSValue&);

    static OptionSet<Containment> convertContain(const BuilderState&, const CSSValue&);
    static Vector<Style::ScopedName> convertContainerName(const BuilderState&, const CSSValue&);

    static OptionSet<MarginTrimType> convertMarginTrim(const BuilderState&, const CSSValue&);

    static TextSpacingTrim convertTextSpacingTrim(const BuilderState&, const CSSValue&);
    static TextAutospace convertTextAutospace(const BuilderState&, const CSSValue&);

    static std::optional<WebCore::Length> convertBlockStepSize(const BuilderState&, const CSSValue&);

    static Vector<Style::ScopedName> convertViewTransitionClass(const BuilderState&, const CSSValue&);
    static Style::ViewTransitionName convertViewTransitionName(const BuilderState&, const CSSValue&);
    static RefPtr<WillChangeData> convertWillChange(const BuilderState&, const CSSValue&);
    
    static Vector<AtomString> convertScrollTimelineName(const BuilderState&, const CSSValue&);
    static Vector<ScrollAxis> convertScrollTimelineAxis(const BuilderState&, const CSSValue&);
    static Vector<ViewTimelineInsets> convertViewTimelineInset(const BuilderState&, const CSSValue&);

    static Vector<AtomString> convertAnchorName(const BuilderState&, const CSSValue&);

    static BlockEllipsis convertBlockEllipsis(const BuilderState&, const CSSValue&);
    static size_t convertMaxLines(const BuilderState&, const CSSValue&);

    static LineClampValue convertLineClamp(const BuilderState&, const CSSValue&);

    static RefPtr<TimingFunction> convertTimingFunction(const BuilderState&, const CSSValue&);

    static TimelineScope convertTimelineScope(const BuilderState&, const CSSValue&);

    static SingleTimelineRange convertAnimationRangeStart(const BuilderState&, const CSSValue&);
    static SingleTimelineRange convertAnimationRangeEnd(const BuilderState&, const CSSValue&);

    template<CSSValueID, CSSValueID> static WebCore::Length convertPositionComponent(const BuilderState&, const CSSValue&);

private:
    friend class BuilderCustom;

    static WebCore::Length convertToRadiusLength(const BuilderState&, const CSSPrimitiveValue&);
    static WebCore::Length parseSnapCoordinate(const BuilderState&, const CSSValue&);

#if ENABLE(DARK_MODE_CSS)
    static void updateColorScheme(const CSSPrimitiveValue&, StyleColorScheme&);
#endif

    static GridLength createGridTrackBreadth(const BuilderState&, const CSSPrimitiveValue&);
    static GridTrackSize createGridTrackSize(const BuilderState&, const CSSValue&);
    static std::optional<GridTrackList> createGridTrackList(const BuilderState&, const CSSValue&);
    static GridPosition createGridPosition(const BuilderState&, const CSSValue&);
    static NamedGridLinesMap createImplicitNamedGridLinesFromGridArea(const BuilderState&, const NamedGridAreaMap&, GridTrackSizingDirection);

    static CSSToLengthConversionData cssToLengthConversionDataWithTextZoomFactor(const BuilderState&);
};

inline WebCore::Length BuilderConverter::convertLength(const BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    CSSToLengthConversionData conversionData = builderState.useSVGZoomRulesForLength() ?
        builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
        : builderState.cssToLengthConversionData();

    if (primitiveValue.isLength()) {
        auto length = primitiveValue.resolveAsLength<WebCore::Length>(conversionData);
        length.setHasQuirk(primitiveValue.primitiveType() == CSSUnitType::CSS_QUIRKY_EM);
        return length;
    }

    if (primitiveValue.isPercentage())
        return WebCore::Length(primitiveValue.resolveAsPercentage(conversionData), LengthType::Percent);

    if (primitiveValue.isCalculatedPercentageWithLength())
        return WebCore::Length(primitiveValue.cssCalcValue()->createCalculationValue(conversionData, CSSCalcSymbolTable { }));

    ASSERT_NOT_REACHED();
    return WebCore::Length(0, LengthType::Fixed);
}

inline WebCore::Length BuilderConverter::convertLengthAllowingNumber(const BuilderState& builderState, const CSSValue& value)
{
    CSSToLengthConversionData conversionData = builderState.useSVGZoomRulesForLength() ?
        builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
        : builderState.cssToLengthConversionData();

    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.isNumberOrInteger())
        return WebCore::Length(primitiveValue.resolveAsNumber(conversionData), LengthType::Fixed);
    return convertLength(builderState, value);
}

inline WebCore::Length BuilderConverter::convertLengthOrAuto(const BuilderState& builderState, const CSSValue& value)
{
    if (value.valueID() == CSSValueAuto)
        return WebCore::Length(LengthType::Auto);
    return convertLength(builderState, value);
}

inline WebCore::Length BuilderConverter::convertLengthSizing(const BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    switch (primitiveValue.valueID()) {
    case CSSValueInvalid:
        return convertLength(builderState, value);
    case CSSValueIntrinsic:
        return WebCore::Length(LengthType::Intrinsic);
    case CSSValueMinIntrinsic:
        return WebCore::Length(LengthType::MinIntrinsic);
    case CSSValueMinContent:
    case CSSValueWebkitMinContent:
        return WebCore::Length(LengthType::MinContent);
    case CSSValueMaxContent:
    case CSSValueWebkitMaxContent:
        return WebCore::Length(LengthType::MaxContent);
    case CSSValueWebkitFillAvailable:
        return WebCore::Length(LengthType::FillAvailable);
    case CSSValueFitContent:
    case CSSValueWebkitFitContent:
        return WebCore::Length(LengthType::FitContent);
    case CSSValueAuto:
        return WebCore::Length(LengthType::Auto);
    case CSSValueContent:
        return WebCore::Length(LengthType::Content);
    default:
        ASSERT_NOT_REACHED();
        return { };
    }
}

inline ListStyleType BuilderConverter::convertListStyleType(const BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.isValueID()) {
        if (primitiveValue.valueID() == CSSValueNone)
            return { ListStyleType::Type::None, nullAtom() };
        return { ListStyleType::Type::CounterStyle, makeAtomString(primitiveValue.stringValue()) };
    }
    if (primitiveValue.isCustomIdent()) {
        ASSERT(builderState.document().settings().cssCounterStyleAtRulesEnabled());
        return { ListStyleType::Type::CounterStyle, makeAtomString(primitiveValue.stringValue()) };
    }
#if !ASSERT_ENABLED
    UNUSED_PARAM(builderState);
#endif
    return { ListStyleType::Type::String, makeAtomString(primitiveValue.stringValue()) };
}

inline WebCore::Length BuilderConverter::convertLengthMaxSizing(const BuilderState& builderState, const CSSValue& value)
{
    if (value.valueID() == CSSValueNone)
        return WebCore::Length(LengthType::Undefined);
    return convertLengthSizing(builderState, value);
}

inline TabSize BuilderConverter::convertTabSize(const BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.isNumber())
        return TabSize(primitiveValue.resolveAsNumber<float>(builderState.cssToLengthConversionData()), SpaceValueType);
    return TabSize(primitiveValue.resolveAsLength<float>(builderState.cssToLengthConversionData()), LengthValueType);
}

template<typename T>
inline T BuilderConverter::convertComputedLength(const BuilderState& builderState, const CSSValue& value)
{
    return downcast<CSSPrimitiveValue>(value).resolveAsLength<T>(builderState.cssToLengthConversionData());
}

template<typename T>
inline T BuilderConverter::convertLineWidth(const BuilderState& builderState, const CSSValue& value)
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
        if (builderState.style().usedZoom() < 1.0f && result < 1.0) {
            T originalLength = primitiveValue.resolveAsLength<T>(builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0));
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

inline WebCore::Length BuilderConverter::convertToRadiusLength(const BuilderState& builderState, const CSSPrimitiveValue& value)
{
    auto& conversionData = builderState.cssToLengthConversionData();
    if (value.isPercentage())
        return WebCore::Length(value.resolveAsPercentage(conversionData), LengthType::Percent);
    if (value.isCalculatedPercentageWithLength())
        return WebCore::Length(value.cssCalcValue()->createCalculationValue(conversionData, CSSCalcSymbolTable { }));
    auto length = value.resolveAsLength<WebCore::Length>(conversionData);
    if (length.isNegative())
        return { 0, LengthType::Fixed };
    return length;
}

inline LengthSize BuilderConverter::convertRadius(const BuilderState& builderState, const CSSValue& value)
{
    if (!value.isPair())
        return { { 0, LengthType::Fixed }, { 0, LengthType::Fixed } };

    LengthSize radius {
        convertToRadiusLength(builderState, downcast<CSSPrimitiveValue>(value.first())),
        convertToRadiusLength(builderState, downcast<CSSPrimitiveValue>(value.second()))
    };

    ASSERT(!radius.width.isNegative());
    ASSERT(!radius.height.isNegative());
    return radius;
}

inline WebCore::Length BuilderConverter::convertPositionComponentX(const BuilderState& builderState, const CSSValue& value)
{
    return convertPositionComponent<CSSValueLeft, CSSValueRight>(builderState, value);
}

inline WebCore::Length BuilderConverter::convertPositionComponentY(const BuilderState& builderState, const CSSValue& value)
{
    return convertPositionComponent<CSSValueTop, CSSValueBottom>(builderState, value);
}

template<CSSValueID cssValueFor0, CSSValueID cssValueFor100>
inline WebCore::Length BuilderConverter::convertPositionComponent(const BuilderState& builderState, const CSSValue& value)
{
    WebCore::Length length;

    auto* lengthValue = &value;
    bool relativeToTrailingEdge = false;
    
    if (value.isPair()) {
        auto& first = value.first();
        if (first.valueID() == CSSValueRight || first.valueID() == CSSValueBottom)
            relativeToTrailingEdge = true;
        lengthValue = &value.second();
    }
    
    if (value.isValueID()) {
        switch (value.valueID()) {
        case cssValueFor0:
            return WebCore::Length(0, LengthType::Percent);
        case cssValueFor100:
            return WebCore::Length(100, LengthType::Percent);
        case CSSValueCenter:
            return WebCore::Length(50, LengthType::Percent);
        default:
            ASSERT_NOT_REACHED();
        }
    }
        
    length = convertLength(builderState, *lengthValue);

    if (relativeToTrailingEdge)
        length = convertTo100PercentMinusLength(length);

    return length;
}

inline LengthPoint BuilderConverter::convertPosition(const BuilderState& builderState, const CSSValue& value)
{
    if (!value.isPair())
        return RenderStyle::initialObjectPosition();

    auto lengthX = convertPositionComponent<CSSValueLeft, CSSValueRight>(builderState, value.first());
    auto lengthY = convertPositionComponent<CSSValueTop, CSSValueBottom>(builderState, value.second());

    return LengthPoint(lengthX, lengthY);
}

inline LengthPoint BuilderConverter::convertPositionOrAutoOrNormal(const BuilderState& builderState, const CSSValue& value)
{
    if (value.isPair())
        return convertPosition(builderState, value);
    if (value.valueID() == CSSValueNormal)
        return { { LengthType::Normal }, { LengthType::Normal } };
    return { };
}

inline LengthPoint BuilderConverter::convertPositionOrAuto(const BuilderState& builderState, const CSSValue& value)
{
    if (value.isPair())
        return convertPosition(builderState, value);
    return { };
}

inline OptionSet<TextDecorationLine> BuilderConverter::convertTextDecorationLine(const BuilderState&, const CSSValue& value)
{
    auto result = RenderStyle::initialTextDecorationLine();
    if (auto* list = dynamicDowncast<CSSValueList>(value)) {
        for (auto& currentValue : *list)
            result.add(fromCSSValue<TextDecorationLine>(currentValue));
    }
    return result;
}

inline OptionSet<TextTransform> BuilderConverter::convertTextTransform(const BuilderState&, const CSSValue& value)
{
    auto result = RenderStyle::initialTextTransform();
    if (auto* list = dynamicDowncast<CSSValueList>(value)) {
        for (auto& currentValue : *list)
            result.add(fromCSSValue<TextTransform>(currentValue));
    }
    return result;
}

template<typename T>
inline T BuilderConverter::convertNumber(const BuilderState& builderState, const CSSValue& value)
{
    return downcast<CSSPrimitiveValue>(value).resolveAsNumber<T>(builderState.cssToLengthConversionData());
}

template<typename T>
inline T BuilderConverter::convertNumberOrAuto(const BuilderState& builderState, const CSSValue& value)
{
    if (value.valueID() == CSSValueAuto)
        return -1;
    return convertNumber<T>(builderState, value);
}

inline short BuilderConverter::convertWebkitHyphenateLimitLines(const BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueNoLimit)
        return -1;
    return primitiveValue.resolveAsNumber<short>(builderState.cssToLengthConversionData());
}

template<CSSPropertyID>
inline RefPtr<StyleImage> BuilderConverter::convertStyleImage(const BuilderState& builderState, CSSValue& value)
{
    return builderState.createStyleImage(value);
}

inline ImageOrientation BuilderConverter::convertImageOrientation(const BuilderState&, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueFromImage)
        return ImageOrientation::Orientation::FromImage;
    return ImageOrientation::Orientation::None;
}

inline TransformOperations BuilderConverter::convertTransform(const BuilderState& builderState, const CSSValue& value)
{
    CSSToLengthConversionData conversionData = builderState.useSVGZoomRulesForLength() ?
        builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
        : builderState.cssToLengthConversionData();
    return createTransformOperations(value, conversionData);
}

inline RefPtr<TranslateTransformOperation> BuilderConverter::convertTranslate(const BuilderState& builderState, const CSSValue& value)
{
    CSSToLengthConversionData conversionData = builderState.useSVGZoomRulesForLength() ?
        builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
        : builderState.cssToLengthConversionData();
    return createTranslate(value, conversionData);
}

inline RefPtr<RotateTransformOperation> BuilderConverter::convertRotate(const BuilderState& builderState, const CSSValue& value)
{
    CSSToLengthConversionData conversionData = builderState.useSVGZoomRulesForLength() ?
        builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
        : builderState.cssToLengthConversionData();
    return createRotate(value, conversionData);
}

inline RefPtr<ScaleTransformOperation> BuilderConverter::convertScale(const BuilderState& builderState, const CSSValue& value)
{
    CSSToLengthConversionData conversionData = builderState.useSVGZoomRulesForLength() ?
        builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
        : builderState.cssToLengthConversionData();
    return createScale(value, conversionData);
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

inline StyleColorScheme BuilderConverter::convertColorScheme(const BuilderState&, const CSSValue& value)
{
    StyleColorScheme colorScheme;

    if (auto* list = dynamicDowncast<CSSValueList>(value)) {
        for (auto& currentValue : *list)
            updateColorScheme(downcast<CSSPrimitiveValue>(currentValue), colorScheme);
    } else if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value))
        updateColorScheme(*primitiveValue, colorScheme);

    // If the value was just "only", that is synonymous for "only light".
    if (colorScheme.isOnly())
        colorScheme.add(ColorScheme::Light);

    return colorScheme;
}
#endif

inline String BuilderConverter::convertString(const BuilderState&, const CSSValue& value)
{
    return downcast<CSSPrimitiveValue>(value).stringValue();
}

inline String BuilderConverter::convertStringOrAuto(const BuilderState& builderState, const CSSValue& value)
{
    if (value.valueID() == CSSValueAuto)
        return nullAtom();
    return convertString(builderState, value);
}

inline String BuilderConverter::convertStringOrNone(const BuilderState& builderState, const CSSValue& value)
{
    if (value.valueID() == CSSValueNone)
        return nullAtom();
    return convertString(builderState, value);
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

inline OptionSet<TextEmphasisPosition> BuilderConverter::convertTextEmphasisPosition(const BuilderState&, const CSSValue& value)
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value))
        return valueToEmphasisPosition(*primitiveValue);

    OptionSet<TextEmphasisPosition> position;
    for (auto& currentValue : downcast<CSSValueList>(value))
        position.add(valueToEmphasisPosition(downcast<CSSPrimitiveValue>(currentValue)));
    return position;
}

inline TextAlignMode BuilderConverter::convertTextAlign(const BuilderState& builderState, const CSSValue& value)
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
            return parentStyle.writingMode().isBidiLTR() ? TextAlignMode::Left : TextAlignMode::Right;
        if (parentStyle.textAlign() == TextAlignMode::End)
            return parentStyle.writingMode().isBidiLTR() ? TextAlignMode::Right : TextAlignMode::Left;

        return parentStyle.textAlign();
    }

    return fromCSSValue<TextAlignMode>(value);
}

inline TextAlignLast BuilderConverter::convertTextAlignLast(const BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    ASSERT(primitiveValue.isValueID());

    if (primitiveValue.valueID() != CSSValueMatchParent)
        return fromCSSValue<TextAlignLast>(value);

    auto& parentStyle = builderState.parentStyle();
    if (parentStyle.textAlignLast() == TextAlignLast::Start)
        return parentStyle.writingMode().isBidiLTR() ? TextAlignLast::Left : TextAlignLast::Right;
    if (parentStyle.textAlignLast() == TextAlignLast::End)
        return parentStyle.writingMode().isBidiLTR() ? TextAlignLast::Right : TextAlignLast::Left;
    return parentStyle.textAlignLast();
}

inline RefPtr<PathOperation> BuilderConverter::convertRayPathOperation(const BuilderState& builderState, const CSSValue& value)
{
    auto& rayValue = downcast<CSSRayValue>(value);

    auto size = RayPathOperation::Size::ClosestCorner;
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

    auto position = rayValue.position();
    if (position)
        return RayPathOperation::create(rayValue.angle()->resolveAsAngle(builderState.cssToLengthConversionData()), size, rayValue.isContaining(), convertPosition(builderState, *position));

    return RayPathOperation::create(rayValue.angle()->resolveAsAngle(builderState.cssToLengthConversionData()), size, rayValue.isContaining());
}

inline RefPtr<BasicShapePath> BuilderConverter::convertSVGPath(const BuilderState& builderState, const CSSValue& value)
{
    if (auto* pathValue = dynamicDowncast<CSSPathValue>(value))
        return basicShapePathForValue(*pathValue, builderState, 1);

    ASSERT(is<CSSPrimitiveValue>(value));
    ASSERT(downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone);
    return nullptr;
}

inline RefPtr<PathOperation> BuilderConverter::convertPathOperation(const BuilderState& builderState, const CSSValue& value)
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        if (primitiveValue->isURI()) {
            auto cssURLValue = primitiveValue->stringValue();
            auto fragment = SVGURIReference::fragmentIdentifierFromIRIString(cssURLValue, builderState.document());
            // FIXME: It doesn't work with external SVG references (see https://bugs.webkit.org/show_bug.cgi?id=126133)
            const TreeScope* treeScope = nullptr;
            if (builderState.element())
                treeScope = &builderState.element()->treeScopeForSVGReferences();
            else
                treeScope = &builderState.document();
            auto target = SVGURIReference::targetElementFromIRIString(cssURLValue, *treeScope);
            return ReferencePathOperation::create(cssURLValue, fragment, dynamicDowncast<SVGElement>(target.element.get()));
        }
        ASSERT(primitiveValue->valueID() == CSSValueNone);
        return nullptr;
    }

    if (is<CSSRayValue>(value))
        return convertRayPathOperation(builderState, value);

    RefPtr<PathOperation> operation;
    auto referenceBox = CSSBoxType::BoxMissing;
    auto processSingleValue = [&](const CSSValue& singleValue) {
        ASSERT(!is<CSSValueList>(singleValue));
        if (is<CSSRayValue>(singleValue))
            operation = convertRayPathOperation(builderState, singleValue);
        else if (!singleValue.isValueID())
            operation = ShapePathOperation::create(basicShapeForValue(singleValue, builderState));
        else
            referenceBox = fromCSSValue<CSSBoxType>(singleValue);
    };

    if (auto* list = dynamicDowncast<CSSValueList>(value)) {
        for (auto& currentValue : *list)
            processSingleValue(currentValue);
    } else
        processSingleValue(value);

    if (operation)
        operation->setReferenceBox(referenceBox);
    else {
        ASSERT(referenceBox != CSSBoxType::BoxMissing);
        operation = BoxPathOperation::create(referenceBox);
    }

    return operation;
}

inline Resize BuilderConverter::convertResize(const BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);

    auto resize = Resize::None;
    if (primitiveValue.valueID() == CSSValueInternalTextareaAuto)
        resize = builderState.document().settings().textAreasAreResizable() ? Resize::Both : Resize::None;
    else
        resize = fromCSSValue<Resize>(value);

    return resize;
}

inline int BuilderConverter::convertMarqueeRepetition(const BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueInfinite)
        return -1; // -1 means repeat forever.

    ASSERT(primitiveValue.isNumber());
    return primitiveValue.resolveAsNumber<int>(builderState.cssToLengthConversionData());
}

inline int BuilderConverter::convertMarqueeSpeed(const BuilderState& builderState, const CSSValue& value)
{
    auto& conversionData = builderState.cssToLengthConversionData();

    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.isTime())
        return primitiveValue.resolveAsTime<int, CSSPrimitiveValue::TimeUnit::Milliseconds>(conversionData);

    // For scrollamount support.
    ASSERT(primitiveValue.isNumber());
    return primitiveValue.resolveAsNumber<int>(conversionData);
}

inline RefPtr<QuotesData> BuilderConverter::convertQuotes(const BuilderState&, const CSSValue& value)
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        if (primitiveValue->valueID() == CSSValueNone)
            return QuotesData::create({ });
        ASSERT(primitiveValue->valueID() == CSSValueAuto);
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

inline OptionSet<TextUnderlinePosition> BuilderConverter::convertTextUnderlinePosition(const BuilderState&, const CSSValue& value)
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value))
        return valueToUnderlinePosition(*primitiveValue);

    auto* pair = dynamicDowncast<CSSValuePair>(value);
    if (!pair)
        return { };

    auto position = valueToUnderlinePosition(downcast<CSSPrimitiveValue>(pair->first()));
    position.add(valueToUnderlinePosition(downcast<CSSPrimitiveValue>(pair->second())));
    return position;
}

inline TextUnderlineOffset BuilderConverter::convertTextUnderlineOffset(const BuilderState& builderState, const CSSValue& value)
{
    return TextUnderlineOffset::createWithLength(BuilderConverter::convertLengthOrAuto(builderState, value));
}

inline TextDecorationThickness BuilderConverter::convertTextDecorationThickness(const BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    switch (primitiveValue.valueID()) {
    case CSSValueAuto:
        return TextDecorationThickness::createWithAuto();
    case CSSValueFromFont:
        return TextDecorationThickness::createFromFont();
    default: {
        auto conversionData = builderState.cssToLengthConversionData();

        if (primitiveValue.isPercentage())
            return TextDecorationThickness::createWithLength(WebCore::Length(clampTo<float>(primitiveValue.resolveAsPercentage(conversionData), minValueForCssLength, maxValueForCssLength), LengthType::Percent));

        if (primitiveValue.isCalculatedPercentageWithLength())
            return TextDecorationThickness::createWithLength(WebCore::Length(primitiveValue.cssCalcValue()->createCalculationValue(conversionData, CSSCalcSymbolTable { })));

        ASSERT(primitiveValue.isLength());
        return TextDecorationThickness::createWithLength(primitiveValue.resolveAsLength<WebCore::Length>(conversionData));
    }
    }
}

inline RefPtr<StyleReflection> BuilderConverter::convertReflection(const BuilderState& builderState, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(value.valueID() == CSSValueNone);
        return nullptr;
    }

    auto& reflectValue = downcast<CSSReflectValue>(value);

    NinePieceImage mask(NinePieceImage::Type::Mask);
    mask.setFill(true);

    builderState.styleMap().mapNinePieceImage(reflectValue.mask(), mask);

    auto reflection = StyleReflection::create();
    reflection->setDirection(fromCSSValueID<ReflectionDirection>(reflectValue.direction()));
    reflection->setOffset(reflectValue.offset().convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion>(builderState.cssToLengthConversionData()));
    reflection->setMask(mask);
    return reflection;
}

inline TextEdge BuilderConverter::convertTextEdge(const BuilderState&, const CSSValue& value)
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
    auto& pair = downcast<CSSValuePair>(value);
    return {
        overValue(downcast<CSSPrimitiveValue>(pair.first()).valueID()),
        underValue(downcast<CSSPrimitiveValue>(pair.second()).valueID())
    };
}

inline IntSize BuilderConverter::convertInitialLetter(const BuilderState& builderState, const CSSValue& value)
{
    if (value.valueID() == CSSValueNormal)
        return IntSize();

    auto& conversionData = builderState.cssToLengthConversionData();
    return {
        downcast<CSSPrimitiveValue>(value.first()).resolveAsNumber<int>(conversionData),
        downcast<CSSPrimitiveValue>(value.second()).resolveAsNumber<int>(conversionData)
    };
}

inline float BuilderConverter::convertTextStrokeWidth(const BuilderState& builderState, const CSSValue& value)
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
        auto emsValue = CSSPrimitiveValue::create(result, CSSUnitType::CSS_EM);
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

inline OptionSet<LineBoxContain> BuilderConverter::convertLineBoxContain(const BuilderState&, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(value.valueID() == CSSValueNone);
        return { };
    }

    return downcast<CSSLineBoxContainValue>(value).value();
}

inline RefPtr<ShapeValue> BuilderConverter::convertShapeValue(const BuilderState& builderState, CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(value.valueID() == CSSValueNone);
        return nullptr;
    }

    if (value.isImage())
        return ShapeValue::create(builderState.createStyleImage(value).releaseNonNull());

    RefPtr<BasicShape> shape;
    auto referenceBox = CSSBoxType::BoxMissing;
    auto processSingleValue = [&](const CSSValue& currentValue) {
        if (!currentValue.isValueID())
            shape = basicShapeForValue(currentValue, builderState, 1);
        else
            referenceBox = fromCSSValue<CSSBoxType>(currentValue);
    };
    if (auto* list = dynamicDowncast<CSSValueList>(value)) {
        for (auto& currentValue : *list)
            processSingleValue(currentValue);
    } else
        processSingleValue(value);

    
    if (shape)
        return ShapeValue::create(shape.releaseNonNull(), referenceBox);

    if (referenceBox != CSSBoxType::BoxMissing)
        return ShapeValue::create(referenceBox);

    ASSERT_NOT_REACHED();
    return nullptr;
}

inline ScrollSnapType BuilderConverter::convertScrollSnapType(const BuilderState&, const CSSValue& value)
{
    ScrollSnapType type;
    auto& values = downcast<CSSValueList>(value);
    auto& firstValue = downcast<CSSPrimitiveValue>(*values.item(0));
    if (firstValue.valueID() == CSSValueNone)
        return type;

    type.axis = fromCSSValue<ScrollSnapAxis>(firstValue);
    if (values.length() == 2)
        type.strictness = fromCSSValue<ScrollSnapStrictness>(*values.item(1));
    else
        type.strictness = ScrollSnapStrictness::Proximity;

    return type;
}

inline ScrollSnapAlign BuilderConverter::convertScrollSnapAlign(const BuilderState&, const CSSValue& value)
{
    auto& values = downcast<CSSValueList>(value);
    ScrollSnapAlign alignment;
    alignment.blockAlign = fromCSSValue<ScrollSnapAxisAlignType>(*values.item(0));
    if (values.length() == 1)
        alignment.inlineAlign = alignment.blockAlign;
    else
        alignment.inlineAlign = fromCSSValue<ScrollSnapAxisAlignType>(*values.item(1));
    return alignment;
}

inline ScrollSnapStop BuilderConverter::convertScrollSnapStop(const BuilderState&, const CSSValue& value)
{
    return fromCSSValue<ScrollSnapStop>(value);
}

inline std::optional<ScrollbarColor> BuilderConverter::convertScrollbarColor(const BuilderState& builderState, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(value.valueID() == CSSValueAuto);
        return std::nullopt;
    }

    auto& pair = downcast<CSSValuePair>(value);

    return ScrollbarColor {
        builderState.colorFromPrimitiveValue(downcast<CSSPrimitiveValue>(pair.first())),
        builderState.colorFromPrimitiveValue(downcast<CSSPrimitiveValue>(pair.second()))
    };
}

inline ScrollbarGutter BuilderConverter::convertScrollbarGutter(const BuilderState&, const CSSValue& value)
{
    ScrollbarGutter gutter;
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        if (primitiveValue->valueID() == CSSValueStable)
            gutter.isAuto = false;
        return gutter;
    }

    ASSERT(value.isPair());

    gutter.isAuto = false;
    gutter.bothEdges = true;

    return gutter;
}

inline ScrollbarWidth BuilderConverter::convertScrollbarWidth(const BuilderState& builderState, const CSSValue& value)
{
    ScrollbarWidth scrollbarWidth = fromCSSValueDeducingType(builderState, value);
    if (scrollbarWidth == ScrollbarWidth::Thin && builderState.document().quirks().needsScrollbarWidthThinDisabledQuirk())
        return ScrollbarWidth::Auto;

    return scrollbarWidth;
}

inline GridLength BuilderConverter::createGridTrackBreadth(const BuilderState& builderState, const CSSPrimitiveValue& primitiveValue)
{
    if (primitiveValue.valueID() == CSSValueMinContent || primitiveValue.valueID() == CSSValueWebkitMinContent)
        return WebCore::Length(LengthType::MinContent);

    if (primitiveValue.valueID() == CSSValueMaxContent || primitiveValue.valueID() == CSSValueWebkitMaxContent)
        return WebCore::Length(LengthType::MaxContent);

    auto& conversionData = builderState.cssToLengthConversionData();

    // Fractional unit.
    if (primitiveValue.isFlex())
        return GridLength(primitiveValue.resolveAsFlex<double>(conversionData));

    auto length = primitiveValue.convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion | AutoConversion>(conversionData);
    if (!length.isUndefined())
        return length;
    return WebCore::Length(0.0, LengthType::Fixed);
}

inline GridTrackSize BuilderConverter::createGridTrackSize(const BuilderState& builderState, const CSSValue& value)
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value))
        return GridTrackSize(createGridTrackBreadth(builderState, *primitiveValue));

    auto* function = dynamicDowncast<CSSFunctionValue>(value);
    if (!function)
        return GridTrackSize(GridLength(0));

    if (function->length() == 1)
        return GridTrackSize(createGridTrackBreadth(builderState, downcast<CSSPrimitiveValue>(*function->itemWithoutBoundsCheck(0))), FitContentTrackSizing);

    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(function->length() == 2);
    GridLength minTrackBreadth(createGridTrackBreadth(builderState, downcast<CSSPrimitiveValue>(*function->itemWithoutBoundsCheck(0))));
    GridLength maxTrackBreadth(createGridTrackBreadth(builderState, downcast<CSSPrimitiveValue>(*function->itemWithoutBoundsCheck(1))));
    return GridTrackSize(minTrackBreadth, maxTrackBreadth);
}

inline std::optional<GridTrackList> BuilderConverter::createGridTrackList(const BuilderState& builderState, const CSSValue& value)
{
    RefPtr<const CSSValueContainingVector> valueList;

    GridTrackList trackList;

    auto* subgridValue = dynamicDowncast<CSSSubgridValue>(value);
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        if (primitiveValue->valueID() == CSSValueMasonry) {
            trackList.list.append(GridTrackEntryMasonry());
            return { WTFMove(trackList) };
        }
        if (primitiveValue->valueID() == CSSValueNone)
            return { WTFMove(trackList) };
    } else if (subgridValue) {
        valueList = subgridValue;
        trackList.list.append(GridTrackEntrySubgrid());
    } else if (auto* list = dynamicDowncast<CSSValueList>(value))
        valueList = list;
    else
        return std::nullopt;

    // https://drafts.csswg.org/css-grid-2/#computed-tracks
    // The computed track list of a non-subgrid axis is a list alternating between line name sets
    // and track sections, with the first and last items being line name sets.
    auto ensureLineNames = [&](auto& list) {
        if (subgridValue)
            return;
        if (list.isEmpty() || !std::holds_alternative<Vector<String>>(list.last()))
            list.append(Vector<String>());
    };

    auto buildRepeatList = [&](const CSSValue& repeatValue, RepeatTrackList& repeatList) {
        for (auto& currentValue : downcast<CSSValueContainingVector>(repeatValue)) {
            if (auto* namesValue = dynamicDowncast<CSSGridLineNamesValue>(currentValue))
                repeatList.append(Vector<String>(namesValue->names()));
            else {
                ensureLineNames(repeatList);
                repeatList.append(createGridTrackSize(builderState, currentValue));
            }
        }

        if (!repeatList.isEmpty())
            ensureLineNames(repeatList);
    };

    auto addOne = [&](const CSSValue& currentValue) {
        if (auto* namesValue = dynamicDowncast<CSSGridLineNamesValue>(currentValue)) {
            trackList.list.append(Vector<String>(namesValue->names()));
            return;
        }

        ensureLineNames(trackList.list);

        if (auto* repeatValue = dynamicDowncast<CSSGridAutoRepeatValue>(currentValue)) {
            CSSValueID autoRepeatID = repeatValue->autoRepeatID();
            ASSERT(autoRepeatID == CSSValueAutoFill || autoRepeatID == CSSValueAutoFit);

            GridTrackEntryAutoRepeat repeat;
            repeat.type = autoRepeatID == CSSValueAutoFill ? AutoRepeatType::Fill : AutoRepeatType::Fit;

            buildRepeatList(currentValue, repeat.list);
            trackList.list.append(WTFMove(repeat));
        } else if (auto* repeatValue = dynamicDowncast<CSSGridIntegerRepeatValue>(currentValue)) {
            auto repetitions = clampTo(repeatValue->repetitions().resolveAsInteger(builderState.cssToLengthConversionData()), 1, GridPosition::max());

            GridTrackEntryRepeat repeat;
            repeat.repeats = repetitions;

            buildRepeatList(currentValue, repeat.list);
            trackList.list.append(WTFMove(repeat));
        } else
            trackList.list.append(createGridTrackSize(builderState, currentValue));
    };

    if (!valueList)
        addOne(value);
    else {
        for (auto& value : *valueList)
            addOne(value);
    }

    if (!trackList.list.isEmpty())
        ensureLineNames(trackList.list);

    return { WTFMove(trackList) };
}

inline GridPosition BuilderConverter::createGridPosition(const BuilderState& builderState, const CSSValue& value)
{
    GridPosition position;

    // We accept the specification's grammar:
    // auto | <custom-ident> | [ <integer> && <custom-ident>? ] | [ span && [ <integer> || <custom-ident> ] ]
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        if (primitiveValue->isCustomIdent()) {
            position.setNamedGridArea(primitiveValue->stringValue());
            return position;
        }

        ASSERT(primitiveValue->valueID() == CSSValueAuto);
        return position;
    }

    auto& gridLineValue = downcast<CSSGridLineValue>(value);
    RefPtr uncheckedSpanValue = gridLineValue.spanValue();
    RefPtr uncheckedNumericValue = gridLineValue.numericValue();
    RefPtr uncheckedGridLineName = gridLineValue.gridLineName();

    auto gridLineNumber = uncheckedNumericValue && uncheckedNumericValue->isInteger() ? uncheckedNumericValue->resolveAsInteger(builderState.cssToLengthConversionData()) : 0;
    auto gridLineName = uncheckedGridLineName && uncheckedGridLineName->isCustomIdent() ? uncheckedGridLineName->stringValue() : String();

    if (uncheckedSpanValue && uncheckedSpanValue->valueID() == CSSValueSpan)
        position.setSpanPosition(gridLineNumber > 0 ? gridLineNumber : 1, gridLineName);
    else
        position.setExplicitPosition(gridLineNumber, gridLineName);

    return position;
}

inline NamedGridLinesMap BuilderConverter::createImplicitNamedGridLinesFromGridArea(const BuilderState&, const NamedGridAreaMap& namedGridAreas, GridTrackSizingDirection direction)
{
    NamedGridLinesMap namedGridLines;

    for (auto& area : namedGridAreas.map) {
        GridSpan areaSpan = direction == GridTrackSizingDirection::ForRows ? area.value.rows : area.value.columns;
        {
            auto& startVector = namedGridLines.map.add(makeString(area.key, "-start"_s), Vector<unsigned>()).iterator->value;
            startVector.append(areaSpan.startLine());
            std::sort(startVector.begin(), startVector.end());
        }
        {
            auto& endVector = namedGridLines.map.add(makeString(area.key, "-end"_s), Vector<unsigned>()).iterator->value;
            endVector.append(areaSpan.endLine());
            std::sort(endVector.begin(), endVector.end());
        }
    }
    // FIXME: For acceptable performance, should sort once at the end, not as we add each item, or at least insert in sorted order instead of using std::sort each time.

    return namedGridLines;
}

inline Vector<GridTrackSize> BuilderConverter::convertGridTrackSizeList(const BuilderState& builderState, const CSSValue& value)
{
    auto validateValue = [](const CSSValue& value) {
        ASSERT_UNUSED(value, !value.isGridLineNamesValue());
        ASSERT_UNUSED(value, !value.isGridAutoRepeatValue());
        ASSERT_UNUSED(value, !value.isGridIntegerRepeatValue());
    };

    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        if (primitiveValue->isValueID()) {
            ASSERT(primitiveValue->valueID() == CSSValueAuto);
            return RenderStyle::initialGridAutoRows();
        }
        // Values coming from CSS Typed OM may not have been converted to a CSSValueList yet.
        validateValue(*primitiveValue);
        return Vector<GridTrackSize>({ convertGridTrackSize(builderState, *primitiveValue) });
    }

    if (auto* valueList = dynamicDowncast<CSSValueList>(value))  {
        return WTF::map(*valueList, [&](auto& currentValue) {
            validateValue(currentValue);
            return convertGridTrackSize(builderState, currentValue);
        });
    }
    validateValue(value);
    return Vector<GridTrackSize>({ convertGridTrackSize(builderState, value) });
}

inline GridTrackSize BuilderConverter::convertGridTrackSize(const BuilderState& builderState, const CSSValue& value)
{
    return createGridTrackSize(builderState, value);
}

inline std::optional<GridTrackList> BuilderConverter::convertGridTrackList(const BuilderState& builderState, const CSSValue& value)
{
    return createGridTrackList(builderState, value);
}

inline GridPosition BuilderConverter::convertGridPosition(const BuilderState& builderState, const CSSValue& value)
{
    return createGridPosition(builderState, value);
}

inline GridAutoFlow BuilderConverter::convertGridAutoFlow(const BuilderState&, const CSSValue& value)
{
    ASSERT(!is<CSSPrimitiveValue>(value) || downcast<CSSPrimitiveValue>(value).isValueID());

    auto* list = dynamicDowncast<CSSValueList>(value);
    if (list && !list->length())
        return RenderStyle::initialGridAutoFlow();

    auto& first = downcast<CSSPrimitiveValue>(list ? *(list->item(0)) : value);
    auto* second = downcast<CSSPrimitiveValue>(list && list->length() == 2 ? list->item(1) : nullptr);

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

inline Vector<StyleContentAlignmentData> BuilderConverter::convertContentAlignmentDataList(const BuilderState& builder, const CSSValue& value)
{
    auto& list = downcast<CSSValueList>(value);
    return WTF::map(list, [&](auto& value) {
        return convertContentAlignmentData(builder, downcast<CSSContentDistributionValue>(value));
    });
}

inline MasonryAutoFlow BuilderConverter::convertMasonryAutoFlow(const BuilderState&, const CSSValue& value)
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

inline float zoomWithTextZoomFactor(const BuilderState& builderState)
{
    if (auto* frame = builderState.document().frame()) {
        float textZoomFactor = builderState.style().textZoom() != TextZoom::Reset ? frame->textZoomFactor() : 1.0f;
        return builderState.style().usedZoom() * textZoomFactor;
    }
    return builderState.cssToLengthConversionData().zoom();
}

inline CSSToLengthConversionData BuilderConverter::cssToLengthConversionDataWithTextZoomFactor(const BuilderState& builderState)
{
    float zoom = zoomWithTextZoomFactor(builderState);
    if (zoom == builderState.cssToLengthConversionData().zoom())
        return builderState.cssToLengthConversionData();

    return builderState.cssToLengthConversionData().copyWithAdjustedZoom(zoom);
}

inline WebCore::Length BuilderConverter::convertTextLengthOrNormal(const BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    auto conversionData = (builderState.useSVGZoomRulesForLength())
        ? builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
        : cssToLengthConversionDataWithTextZoomFactor(builderState);

    if (primitiveValue.valueID() == CSSValueNormal)
        return RenderStyle::zeroLength();
    if (primitiveValue.isLength())
        return primitiveValue.resolveAsLength<WebCore::Length>(conversionData);
    if (primitiveValue.isPercentage())
        return WebCore::Length(clampTo<float>(primitiveValue.resolveAsPercentage(conversionData), minValueForCssLength, maxValueForCssLength), LengthType::Percent);
    if (primitiveValue.isCalculatedPercentageWithLength())
        return WebCore::Length(primitiveValue.cssCalcValue()->createCalculationValue(conversionData, CSSCalcSymbolTable { }));
    if (primitiveValue.isNumber())
        return WebCore::Length(primitiveValue.resolveAsNumber(conversionData), LengthType::Fixed);
    ASSERT_NOT_REACHED();
    return RenderStyle::zeroLength();
}

inline std::optional<float> BuilderConverter::convertPerspective(const BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueNone)
        return RenderStyle::initialPerspective();

    auto& conversionData = builderState.cssToLengthConversionData();

    float perspective = -1;
    if (primitiveValue.isLength())
        perspective = primitiveValue.resolveAsLength<float>(conversionData);
    else if (primitiveValue.isNumber())
        perspective = primitiveValue.resolveAsNumber<float>(conversionData) * conversionData.zoom();
    else
        ASSERT_NOT_REACHED();

    return perspective < 0 ? std::optional<float>(std::nullopt) : std::optional<float>(perspective);
}

inline std::optional<WebCore::Length> BuilderConverter::convertMarqueeIncrement(const BuilderState& builderState, const CSSValue& value)
{
    auto length = downcast<CSSPrimitiveValue>(value).convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion>(builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f));
    if (length.isUndefined())
        return std::nullopt;
    return length;
}

inline FilterOperations BuilderConverter::convertFilterOperations(const BuilderState& builderState, const CSSValue& value)
{
    return builderState.createFilterOperations(value);
}

// The input value needs to parsed and valid, this function returns std::nullopt if the input was "normal".
inline std::optional<FontSelectionValue> BuilderConverter::convertFontStyleFromValue(const BuilderState& builderState, const CSSValue& value)
{
    return Style::fontStyleFromCSSValue(value, builderState.cssToLengthConversionData());
}

inline FontSelectionValue BuilderConverter::convertFontWeight(const BuilderState& builderState, const CSSValue& value)
{
    return Style::fontWeightFromCSSValue(value, builderState.cssToLengthConversionData());
}

inline FontSelectionValue BuilderConverter::convertFontStretch(const BuilderState& builderState, const CSSValue& value)
{
    return Style::fontStretchFromCSSValue(value, builderState.cssToLengthConversionData());
}

inline FontFeatureSettings BuilderConverter::convertFontFeatureSettings(const BuilderState& builderState, const CSSValue& value)
{
    return Style::fontFeatureSettingsFromCSSValue(value, builderState.cssToLengthConversionData());
}

inline FontVariationSettings BuilderConverter::convertFontVariationSettings(const BuilderState& builderState, const CSSValue& value)
{
    return Style::fontVariationSettingsFromCSSValue(value, builderState.cssToLengthConversionData());
}

inline FontSizeAdjust BuilderConverter::convertFontSizeAdjust(const BuilderState& builderState, const CSSValue& value)
{
    return Style::fontSizeAdjustFromCSSValue(value, builderState.cssToLengthConversionData());
}

#if PLATFORM(IOS_FAMILY)
inline bool BuilderConverter::convertTouchCallout(const BuilderState&, const CSSValue& value)
{
    return !equalLettersIgnoringASCIICase(downcast<CSSPrimitiveValue>(value).stringValue(), "none"_s);
}
#endif

#if ENABLE(TOUCH_EVENTS)
inline StyleColor BuilderConverter::convertTapHighlightColor(const BuilderState& builderState, const CSSValue& value)
{
    return builderState.colorFromPrimitiveValue(downcast<CSSPrimitiveValue>(value));
}
#endif

inline OptionSet<TouchAction> BuilderConverter::convertTouchAction(const BuilderState&, const CSSValue& value)
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
inline bool BuilderConverter::convertOverflowScrolling(const BuilderState&, const CSSValue& value)
{
    return value.valueID() == CSSValueTouch;
}
#endif

inline bool BuilderConverter::convertSmoothScrolling(const BuilderState&, const CSSValue& value)
{
    return value.valueID() == CSSValueSmooth;
}

inline SVGLengthValue BuilderConverter::convertSVGLengthValue(const BuilderState& builderState, const CSSValue& value, ShouldConvertNumberToPxLength shouldConvertNumberToPxLength)
{
    return SVGLengthValue::fromCSSPrimitiveValue(downcast<CSSPrimitiveValue>(value), builderState.cssToLengthConversionData(), shouldConvertNumberToPxLength);
}

inline Vector<SVGLengthValue> BuilderConverter::convertSVGLengthVector(const BuilderState& builderState, const CSSValue& value, ShouldConvertNumberToPxLength shouldConvertNumberToPxLength)
{
    auto& valueList = downcast<CSSValueList>(value);
    return WTF::map(valueList, [&](auto& item) {
        return convertSVGLengthValue(builderState, item, shouldConvertNumberToPxLength);
    });
}

inline Vector<SVGLengthValue> BuilderConverter::convertStrokeDashArray(const BuilderState& builderState, const CSSValue& value)
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        if (primitiveValue->valueID() == CSSValueNone)
            return SVGRenderStyle::initialStrokeDashArray();

        // Values coming from CSS-Typed-OM may not have been converted to a CSSValueList yet.
        return Vector { convertSVGLengthValue(builderState, value, ShouldConvertNumberToPxLength::Yes) };
    }

    return convertSVGLengthVector(builderState, value, ShouldConvertNumberToPxLength::Yes);
}

inline PaintOrder BuilderConverter::convertPaintOrder(const BuilderState&, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(value.valueID() == CSSValueNormal);
        return PaintOrder::Normal;
    }

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

inline float BuilderConverter::convertOpacity(const BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    float opacity = primitiveValue.valueDividingBy100IfPercentage(builderState.cssToLengthConversionData());
    return std::max(0.0f, std::min(1.0f, opacity));
}

inline String BuilderConverter::convertSVGURIReference(const BuilderState&, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.isURI())
        return primitiveValue.stringValue();
    return emptyString();
}

inline StyleSelfAlignmentData BuilderConverter::convertSelfOrDefaultAlignmentData(const BuilderState&, const CSSValue& value)
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
    return alignmentData;
}

inline StyleContentAlignmentData BuilderConverter::convertContentAlignmentData(const BuilderState&, const CSSValue& value)
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

inline GlyphOrientation BuilderConverter::convertGlyphOrientation(const BuilderState& builderState, const CSSValue& value)
{
    float angle = std::abs(fmodf(downcast<CSSPrimitiveValue>(value).resolveAsAngle(builderState.cssToLengthConversionData()), 360.0f));
    if (angle <= 45.0f || angle > 315.0f)
        return GlyphOrientation::Degrees0;
    if (angle > 45.0f && angle <= 135.0f)
        return GlyphOrientation::Degrees90;
    if (angle > 135.0f && angle <= 225.0f)
        return GlyphOrientation::Degrees180;
    return GlyphOrientation::Degrees270;
}

inline GlyphOrientation BuilderConverter::convertGlyphOrientationOrAuto(const BuilderState& builderState, const CSSValue& value)
{
    if (value.valueID() == CSSValueAuto)
        return GlyphOrientation::Auto;
    return convertGlyphOrientation(builderState, value);
}

inline WebCore::Length BuilderConverter::convertLineHeight(const BuilderState& builderState, const CSSValue& value, float multiplier)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    auto valueID = primitiveValue.valueID();
    if (valueID == CSSValueNormal)
        return RenderStyle::initialLineHeight();

    if (CSSPropertyParserHelpers::isSystemFontShorthand(valueID))
        return RenderStyle::initialLineHeight();

    auto conversionData = builderState.cssToLengthConversionData().copyForLineHeight(zoomWithTextZoomFactor(builderState));

    if (primitiveValue.isLength() || primitiveValue.isCalculatedPercentageWithLength()) {
        WebCore::Length length;
        if (primitiveValue.isLength())
            length = primitiveValue.resolveAsLength<WebCore::Length>(conversionData);
        else {
            auto value = primitiveValue.cssCalcValue()->createCalculationValue(conversionData, CSSCalcSymbolTable { })->evaluate(builderState.style().computedFontSize());
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
    if (primitiveValue.isPercentage()) {
        // FIXME: percentage should not be restricted to an integer here.
        return WebCore::Length((builderState.style().computedFontSize() * primitiveValue.resolveAsPercentage<int>(conversionData)) / 100, LengthType::Fixed);
    }

    ASSERT(primitiveValue.isNumber());
    return WebCore::Length(primitiveValue.resolveAsNumber(conversionData) * 100.0, LengthType::Percent);
}

inline FontPalette BuilderConverter::convertFontPalette(const BuilderState&, const CSSValue& value)
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
    
inline OptionSet<SpeakAs> BuilderConverter::convertSpeakAs(const BuilderState&, const CSSValue& value)
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

inline OptionSet<HangingPunctuation> BuilderConverter::convertHangingPunctuation(const BuilderState&, const CSSValue& value)
{
    auto result = RenderStyle::initialHangingPunctuation();
    if (auto* list = dynamicDowncast<CSSValueList>(value)) {
        for (auto& currentValue : *list)
            result.add(fromCSSValue<HangingPunctuation>(currentValue));
    }
    return result;
}

inline GapLength BuilderConverter::convertGapLength(const BuilderState& builderState, const CSSValue& value)
{
    return (value.valueID() == CSSValueNormal) ? GapLength() : GapLength(convertLength(builderState, value));
}

inline OffsetRotation BuilderConverter::convertOffsetRotate(const BuilderState& builderState, const CSSValue& value)
{
    RefPtr<const CSSPrimitiveValue> modifierValue;
    RefPtr<const CSSPrimitiveValue> angleValue;

    if (auto* offsetRotateValue = dynamicDowncast<CSSOffsetRotateValue>(value)) {
        modifierValue = offsetRotateValue->modifier();
        angleValue = offsetRotateValue->angle();
    } else if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        // Values coming from CSSTypedOM didn't go through the parser and may not have been converted to a CSSOffsetRotateValue.
        if (primitiveValue->valueID() == CSSValueAuto || primitiveValue->valueID() == CSSValueReverse)
            modifierValue = primitiveValue;
        else if (primitiveValue->isAngle())
            angleValue = primitiveValue;
    }

    bool hasAuto = false;
    float angleInDegrees = 0;

    if (angleValue)
        angleInDegrees = angleValue->resolveAsAngle<float>(builderState.cssToLengthConversionData());

    if (modifierValue) {
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

inline Vector<Style::ScopedName> BuilderConverter::convertContainerName(const BuilderState& state, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(value.valueID() == CSSValueNone);
        return { };
    }
    auto* list = dynamicDowncast<CSSValueList>(value);
    if (!list)
        return { };

    return WTF::map(*list, [&](auto& item) {
        return Style::ScopedName { AtomString { downcast<CSSPrimitiveValue>(item).stringValue() }, state.styleScopeOrdinal() };
    });
}

inline OptionSet<MarginTrimType> BuilderConverter::convertMarginTrim(const BuilderState&, const CSSValue& value)
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
    ASSERT(list->size() <= 4);
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
    return marginTrim;
}

inline TextSpacingTrim BuilderConverter::convertTextSpacingTrim(const BuilderState&, const CSSValue& value)
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

inline TextAutospace BuilderConverter::convertTextAutospace(const BuilderState&, const CSSValue& value)
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
    for (auto& item : downcast<CSSValueList>(value)) {
        auto& value = downcast<CSSPrimitiveValue>(item);
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

inline std::optional<WebCore::Length> BuilderConverter::convertBlockStepSize(const BuilderState& builderState, const CSSValue& value)
{
    if (downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone)
        return { };
    return convertLength(builderState, value);
}

inline OptionSet<Containment> BuilderConverter::convertContain(const BuilderState&, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        if (value.valueID() == CSSValueNone)
            return RenderStyle::initialContainment();
        if (value.valueID() == CSSValueStrict)
            return RenderStyle::strictContainment();
        return RenderStyle::contentContainment();
    }

    OptionSet<Containment> containment;
    for (auto& item : downcast<CSSValueList>(value)) {
        auto& value = downcast<CSSPrimitiveValue>(item);
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

inline Vector<Style::ScopedName> BuilderConverter::convertViewTransitionClass(const BuilderState& state, const CSSValue& value)
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        if (value.valueID() == CSSValueNone)
            return { };
        return { Style::ScopedName { AtomString { primitiveValue->stringValue() }, state.styleScopeOrdinal() } };
    }

    auto* list = dynamicDowncast<CSSValueList>(value);
    if (!list)
        return { };
    return WTF::map(*list, [&](auto& item) {
        return Style::ScopedName { AtomString { downcast<CSSPrimitiveValue>(item).stringValue() }, state.styleScopeOrdinal() };
    });
}

inline Style::ViewTransitionName BuilderConverter::convertViewTransitionName(const BuilderState& state, const CSSValue& value)
{
    auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value);
    if (!primitiveValue)
        return Style::ViewTransitionName::createWithNone();

    if (value.valueID() == CSSValueNone)
        return Style::ViewTransitionName::createWithNone();

    if (value.valueID() == CSSValueAuto)
        return Style::ViewTransitionName::createWithAuto(state.styleScopeOrdinal());

    return Style::ViewTransitionName::createWithCustomIdent(state.styleScopeOrdinal(), AtomString { primitiveValue->stringValue() });
}

inline RefPtr<WillChangeData> BuilderConverter::convertWillChange(const BuilderState& builderState, const CSSValue& value)
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

inline Vector<AtomString> BuilderConverter::convertScrollTimelineName(const BuilderState&, const CSSValue& value)
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        if (value.valueID() == CSSValueNone)
            return { };
        return { AtomString { primitiveValue->stringValue() } };
    }

    auto* list = dynamicDowncast<CSSValueList>(value);
    if (!list)
        return { };

    return WTF::map(*list, [&](auto& item) {
        return AtomString { downcast<CSSPrimitiveValue>(item).stringValue() };
    });
}

inline Vector<ScrollAxis> BuilderConverter::convertScrollTimelineAxis(const BuilderState&, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value))
        return { fromCSSValueID<ScrollAxis>(value.valueID()) };

    return WTF::map(downcast<CSSValueList>(value), [&](auto& item) {
        return fromCSSValueID<ScrollAxis>(item.valueID());
    });
}

inline Vector<ViewTimelineInsets> BuilderConverter::convertViewTimelineInset(const BuilderState& state, const CSSValue& value)
{
    // While parsing, consumeViewTimelineInset() and consumeViewTimelineShorthand() yield a CSSValueList exclusively.
    auto* list = dynamicDowncast<CSSValueList>(value);
    if (!list)
        return { };

    return WTF::map(*list, [&](auto& item) -> ViewTimelineInsets {
        // Each item is either a single value or a CSSValuePair.
        if (auto* pair = dynamicDowncast<CSSValuePair>(item))
            return { convertLengthOrAuto(state, pair->first()), convertLengthOrAuto(state, pair->second()) };
        if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(item))
            return { convertLengthOrAuto(state, *primitiveValue), std::nullopt };
        return { };
    });
}

inline Vector<AtomString> BuilderConverter::convertAnchorName(const BuilderState&, const CSSValue& value)
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        if (value.valueID() == CSSValueNone)
            return { };
        return { AtomString { primitiveValue->stringValue() } };
    }

    auto* list = dynamicDowncast<CSSValueList>(value);
    if (!list)
        return { };

    return WTF::map(*list, [&](auto& item) {
        return AtomString { downcast<CSSPrimitiveValue>(item).stringValue() };
    });
}

inline BlockEllipsis BuilderConverter::convertBlockEllipsis(const BuilderState& builderState, const CSSValue& value)
{
    if (value.valueID() == CSSValueNone)
        return { };
    if (value.valueID() == CSSValueAuto)
        return { BlockEllipsis::Type::Auto, { } };
    return { BlockEllipsis::Type::String, AtomString { convertString(builderState, value) } };

}

inline size_t BuilderConverter::convertMaxLines(const BuilderState& builderState, const CSSValue& value)
{
    if (downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone)
        return 0;
    return convertNumber<size_t>(builderState, value);
}

inline LineClampValue BuilderConverter::convertLineClamp(const BuilderState& builderState, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);

    if (primitiveValue.primitiveType() == CSSUnitType::CSS_INTEGER)
        return LineClampValue(std::max(primitiveValue.resolveAsInteger<int>(builderState.cssToLengthConversionData()), 1), LineClamp::LineCount);

    if (primitiveValue.primitiveType() == CSSUnitType::CSS_PERCENTAGE)
        return LineClampValue(std::max(primitiveValue.resolveAsPercentage<int>(builderState.cssToLengthConversionData()), 0), LineClamp::Percentage);

    ASSERT(primitiveValue.valueID() == CSSValueNone);
    return LineClampValue();
}

inline RefPtr<TimingFunction> BuilderConverter::convertTimingFunction(const BuilderState&, const CSSValue& value)
{
    return createTimingFunction(value);
}

inline TimelineScope BuilderConverter::convertTimelineScope(const BuilderState&, const CSSValue& value)
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        switch (primitiveValue->valueID()) {
        case CSSValueNone:
            return { };
        case CSSValueAll:
            return { TimelineScope::Type::All, { } };
        default:
            return { TimelineScope::Type::Ident, { AtomString { primitiveValue->stringValue() } } };
        }
    }

    auto* list = dynamicDowncast<CSSValueList>(value);
    if (!list)
        return { };

    return { TimelineScope::Type::Ident, WTF::map(*list, [&](auto& item) {
        return AtomString { downcast<CSSPrimitiveValue>(item).stringValue() };
    }) };
}

} // namespace Style
} // namespace WebCore
