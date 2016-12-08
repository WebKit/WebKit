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
#include "CSSFontVariationValue.h"
#include "CSSFunctionValue.h"
#include "CSSGridAutoRepeatValue.h"
#include "CSSGridLineNamesValue.h"
#include "CSSGridTemplateAreasValue.h"
#include "CSSImageGeneratorValue.h"
#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSReflectValue.h"
#include "Frame.h"
#include "LayoutUnit.h"
#include "Length.h"
#include "LengthRepeat.h"
#include "Pair.h"
#include "QuotesData.h"
#include "RuntimeEnabledFeatures.h"
#include "SVGURIReference.h"
#include "Settings.h"
#include "StyleResolver.h"
#include "StyleScrollSnapPoints.h"
#include "TransformFunctions.h"
#include <wtf/Optional.h>

namespace WebCore {

// Note that we assume the CSS parser only allows valid CSSValue types.
class StyleBuilderConverter {
public:
    static Length convertLength(StyleResolver&, const CSSValue&);
    static Length convertLengthOrAuto(StyleResolver&, const CSSValue&);
    static Length convertLengthSizing(StyleResolver&, const CSSValue&);
    static Length convertLengthMaxSizing(StyleResolver&, const CSSValue&);
    template <typename T> static T convertComputedLength(StyleResolver&, const CSSValue&);
    template <typename T> static T convertLineWidth(StyleResolver&, const CSSValue&);
    static float convertSpacing(StyleResolver&, const CSSValue&);
    static LengthSize convertRadius(StyleResolver&, const CSSValue&);
    static LengthPoint convertObjectPosition(StyleResolver&, const CSSValue&);
    static TextDecoration convertTextDecoration(StyleResolver&, const CSSValue&);
    template <typename T> static T convertNumber(StyleResolver&, const CSSValue&);
    template <typename T> static T convertNumberOrAuto(StyleResolver&, const CSSValue&);
    static short convertWebkitHyphenateLimitLines(StyleResolver&, const CSSValue&);
    template <CSSPropertyID property> static NinePieceImage convertBorderImage(StyleResolver&, CSSValue&);
    template <CSSPropertyID property> static NinePieceImage convertBorderMask(StyleResolver&, CSSValue&);
    template <CSSPropertyID property> static PassRefPtr<StyleImage> convertStyleImage(StyleResolver&, CSSValue&);
    static TransformOperations convertTransform(StyleResolver&, const CSSValue&);
    static String convertString(StyleResolver&, const CSSValue&);
    static String convertStringOrAuto(StyleResolver&, const CSSValue&);
    static String convertStringOrNone(StyleResolver&, const CSSValue&);
    static TextEmphasisPosition convertTextEmphasisPosition(StyleResolver&, const CSSValue&);
    static ETextAlign convertTextAlign(StyleResolver&, const CSSValue&);
    static RefPtr<ClipPathOperation> convertClipPath(StyleResolver&, const CSSValue&);
    static EResize convertResize(StyleResolver&, const CSSValue&);
    static int convertMarqueeRepetition(StyleResolver&, const CSSValue&);
    static int convertMarqueeSpeed(StyleResolver&, const CSSValue&);
    static Ref<QuotesData> convertQuotes(StyleResolver&, const CSSValue&);
    static TextUnderlinePosition convertTextUnderlinePosition(StyleResolver&, const CSSValue&);
    static RefPtr<StyleReflection> convertReflection(StyleResolver&, const CSSValue&);
    static IntSize convertInitialLetter(StyleResolver&, const CSSValue&);
    static float convertTextStrokeWidth(StyleResolver&, const CSSValue&);
    static LineBoxContain convertLineBoxContain(StyleResolver&, const CSSValue&);
    static TextDecorationSkip convertTextDecorationSkip(StyleResolver&, const CSSValue&);
    static PassRefPtr<ShapeValue> convertShapeValue(StyleResolver&, CSSValue&);
#if ENABLE(CSS_SCROLL_SNAP)
    static std::unique_ptr<ScrollSnapPoints> convertScrollSnapPoints(StyleResolver&, const CSSValue&);
    static LengthSize convertSnapCoordinatePair(StyleResolver&, const CSSValue&, size_t offset = 0);
    static Vector<LengthSize> convertScrollSnapCoordinates(StyleResolver&, const CSSValue&);
#endif
#if ENABLE(CSS_GRID_LAYOUT)
    static GridTrackSize convertGridTrackSize(StyleResolver&, const CSSValue&);
    static Vector<GridTrackSize> convertGridTrackSizeList(StyleResolver&, const CSSValue&);
    static std::optional<GridPosition> convertGridPosition(StyleResolver&, const CSSValue&);
    static GridAutoFlow convertGridAutoFlow(StyleResolver&, const CSSValue&);
#endif // ENABLE(CSS_GRID_LAYOUT)
    static std::optional<Length> convertWordSpacing(StyleResolver&, const CSSValue&);
    static std::optional<float> convertPerspective(StyleResolver&, const CSSValue&);
    static std::optional<Length> convertMarqueeIncrement(StyleResolver&, const CSSValue&);
    static std::optional<FilterOperations> convertFilterOperations(StyleResolver&, const CSSValue&);
#if PLATFORM(IOS)
    static bool convertTouchCallout(StyleResolver&, const CSSValue&);
#endif
#if ENABLE(TOUCH_EVENTS)
    static Color convertTapHighlightColor(StyleResolver&, const CSSValue&);
#endif
#if ENABLE(ACCELERATED_OVERFLOW_SCROLLING)
    static bool convertOverflowScrolling(StyleResolver&, const CSSValue&);
#endif
    static FontFeatureSettings convertFontFeatureSettings(StyleResolver&, const CSSValue&);
#if ENABLE(VARIATION_FONTS)
    static FontVariationSettings convertFontVariationSettings(StyleResolver&, const CSSValue&);
#endif
    static SVGLengthValue convertSVGLengthValue(StyleResolver&, const CSSValue&);
    static Vector<SVGLengthValue> convertSVGLengthVector(StyleResolver&, const CSSValue&);
    static Vector<SVGLengthValue> convertStrokeDashArray(StyleResolver&, const CSSValue&);
    static PaintOrder convertPaintOrder(StyleResolver&, const CSSValue&);
    static float convertOpacity(StyleResolver&, const CSSValue&);
    static String convertSVGURIReference(StyleResolver&, const CSSValue&);
    static Color convertSVGColor(StyleResolver&, const CSSValue&);
    static StyleSelfAlignmentData convertSelfOrDefaultAlignmentData(StyleResolver&, const CSSValue&);
    static StyleContentAlignmentData convertContentAlignmentData(StyleResolver&, const CSSValue&);
    static EGlyphOrientation convertGlyphOrientation(StyleResolver&, const CSSValue&);
    static EGlyphOrientation convertGlyphOrientationOrAuto(StyleResolver&, const CSSValue&);
    static std::optional<Length> convertLineHeight(StyleResolver&, const CSSValue&, float multiplier = 1.f);
    static FontSynthesis convertFontSynthesis(StyleResolver&, const CSSValue&);
    
    static BreakBetween convertPageBreakBetween(StyleResolver&, const CSSValue&);
    static BreakInside convertPageBreakInside(StyleResolver&, const CSSValue&);
    static BreakBetween convertColumnBreakBetween(StyleResolver&, const CSSValue&);
    static BreakInside convertColumnBreakInside(StyleResolver&, const CSSValue&);
#if ENABLE(CSS_REGIONS)
    static BreakBetween convertRegionBreakBetween(StyleResolver&, const CSSValue&);
    static BreakInside convertRegionBreakInside(StyleResolver&, const CSSValue&);
#endif
    
    static HangingPunctuation convertHangingPunctuation(StyleResolver&, const CSSValue&);

    static Length convertPositionComponentX(StyleResolver&, const CSSValue&);
    static Length convertPositionComponentY(StyleResolver&, const CSSValue&);
    
private:
    friend class StyleBuilderCustom;

    static Length convertToRadiusLength(CSSToLengthConversionData&, const CSSPrimitiveValue&);
    static TextEmphasisPosition valueToEmphasisPosition(const CSSPrimitiveValue&);
    static TextDecorationSkip valueToDecorationSkip(const CSSPrimitiveValue&);
#if ENABLE(CSS_SCROLL_SNAP)
    static Length parseSnapCoordinate(StyleResolver&, const CSSValue&);
#endif

    static Length convertTo100PercentMinusLength(const Length&);
    template<CSSValueID, CSSValueID> static Length convertPositionComponent(StyleResolver&, const CSSPrimitiveValue&);

#if ENABLE(CSS_GRID_LAYOUT)
    static GridLength createGridTrackBreadth(const CSSPrimitiveValue&, StyleResolver&);
    static GridTrackSize createGridTrackSize(const CSSValue&, StyleResolver&);
    struct TracksData;
    static bool createGridTrackList(const CSSValue&, TracksData&, StyleResolver&);
    static bool createGridPosition(const CSSValue&, GridPosition&);
    static void createImplicitNamedGridLinesFromGridArea(const NamedGridAreaMap&, NamedGridLinesMap&, GridTrackSizingDirection);
#endif // ENABLE(CSS_GRID_LAYOUT)
    static CSSToLengthConversionData csstoLengthConversionDataWithTextZoomFactor(StyleResolver&);
};

inline Length StyleBuilderConverter::convertLength(StyleResolver& styleResolver, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    CSSToLengthConversionData conversionData = styleResolver.useSVGZoomRulesForLength() ?
        styleResolver.state().cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
        : styleResolver.state().cssToLengthConversionData();

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

inline Length StyleBuilderConverter::convertLengthOrAuto(StyleResolver& styleResolver, const CSSValue& value)
{
    if (downcast<CSSPrimitiveValue>(value).valueID() == CSSValueAuto)
        return Length(Auto);
    return convertLength(styleResolver, value);
}

inline Length StyleBuilderConverter::convertLengthSizing(StyleResolver& styleResolver, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    switch (primitiveValue.valueID()) {
    case CSSValueInvalid:
        return convertLength(styleResolver, value);
    case CSSValueIntrinsic:
        return Length(Intrinsic);
    case CSSValueMinIntrinsic:
        return Length(MinIntrinsic);
    case CSSValueWebkitMinContent:
        return Length(MinContent);
    case CSSValueWebkitMaxContent:
        return Length(MaxContent);
    case CSSValueWebkitFillAvailable:
        return Length(FillAvailable);
    case CSSValueWebkitFitContent:
        return Length(FitContent);
    case CSSValueAuto:
        return Length(Auto);
    default:
        ASSERT_NOT_REACHED();
        return Length();
    }
}

inline Length StyleBuilderConverter::convertLengthMaxSizing(StyleResolver& styleResolver, const CSSValue& value)
{
    if (downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone)
        return Length(Undefined);
    return convertLengthSizing(styleResolver, value);
}

template <typename T>
inline T StyleBuilderConverter::convertComputedLength(StyleResolver& styleResolver, const CSSValue& value)
{
    return downcast<CSSPrimitiveValue>(value).computeLength<T>(styleResolver.state().cssToLengthConversionData());
}

template <typename T>
inline T StyleBuilderConverter::convertLineWidth(StyleResolver& styleResolver, const CSSValue& value)
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
        T result = convertComputedLength<T>(styleResolver, value);
        if (styleResolver.state().style()->effectiveZoom() < 1.0f && result < 1.0) {
            T originalLength = primitiveValue.computeLength<T>(styleResolver.state().cssToLengthConversionData().copyWithAdjustedZoom(1.0));
            if (originalLength >= 1.0)
                return 1;
        }
        float minimumLineWidth = 1 / styleResolver.document().deviceScaleFactor();
        if (result > 0 && result < minimumLineWidth)
            return minimumLineWidth;
        return floorToDevicePixel(result, styleResolver.document().deviceScaleFactor());
    }
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

inline float StyleBuilderConverter::convertSpacing(StyleResolver& styleResolver, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueNormal)
        return 0.f;

    CSSToLengthConversionData conversionData = styleResolver.useSVGZoomRulesForLength() ?
        styleResolver.state().cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
        : styleResolver.state().cssToLengthConversionData();
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

inline LengthSize StyleBuilderConverter::convertRadius(StyleResolver& styleResolver, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    Pair* pair = primitiveValue.pairValue();
    if (!pair || !pair->first() || !pair->second())
        return LengthSize(Length(0, Fixed), Length(0, Fixed));

    CSSToLengthConversionData conversionData = styleResolver.state().cssToLengthConversionData();
    Length radiusWidth = convertToRadiusLength(conversionData, *pair->first());
    Length radiusHeight = convertToRadiusLength(conversionData, *pair->second());

    ASSERT(!radiusWidth.isNegative());
    ASSERT(!radiusHeight.isNegative());
    if (radiusWidth.isZero() || radiusHeight.isZero())
        return LengthSize(Length(0, Fixed), Length(0, Fixed));

    return LengthSize(radiusWidth, radiusHeight);
}

inline Length StyleBuilderConverter::convertTo100PercentMinusLength(const Length& length)
{
    if (length.isPercent())
        return Length(100 - length.value(), Percent);
    
    // Turn this into a calc expression: calc(100% - length)
    auto lhs = std::make_unique<CalcExpressionLength>(Length(100, Percent));
    auto rhs = std::make_unique<CalcExpressionLength>(length);
    auto op = std::make_unique<CalcExpressionBinaryOperation>(WTFMove(lhs), WTFMove(rhs), CalcSubtract);
    return Length(CalculationValue::create(WTFMove(op), ValueRangeAll));
}

inline Length StyleBuilderConverter::convertPositionComponentX(StyleResolver& styleResolver, const CSSValue& value)
{
    return convertPositionComponent<CSSValueLeft, CSSValueRight>(styleResolver, downcast<CSSPrimitiveValue>(value));
}

inline Length StyleBuilderConverter::convertPositionComponentY(StyleResolver& styleResolver, const CSSValue& value)
{
    return convertPositionComponent<CSSValueTop, CSSValueBottom>(styleResolver, downcast<CSSPrimitiveValue>(value));
}

template <CSSValueID cssValueFor0, CSSValueID cssValueFor100>
inline Length StyleBuilderConverter::convertPositionComponent(StyleResolver& styleResolver, const CSSPrimitiveValue& value)
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
        
    length = convertLength(styleResolver, *lengthValue);

    if (relativeToTrailingEdge)
        length = convertTo100PercentMinusLength(length);

    return length;
}

inline LengthPoint StyleBuilderConverter::convertObjectPosition(StyleResolver& styleResolver, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    Pair* pair = primitiveValue.pairValue();
    if (!pair || !pair->first() || !pair->second())
        return RenderStyle::initialObjectPosition();

    Length lengthX = convertPositionComponent<CSSValueLeft, CSSValueRight>(styleResolver, *pair->first());
    Length lengthY = convertPositionComponent<CSSValueTop, CSSValueBottom>(styleResolver, *pair->second());

    return LengthPoint(lengthX, lengthY);
}

inline TextDecoration StyleBuilderConverter::convertTextDecoration(StyleResolver&, const CSSValue& value)
{
    TextDecoration result = RenderStyle::initialTextDecoration();
    if (is<CSSValueList>(value)) {
        for (auto& currentValue : downcast<CSSValueList>(value))
            result |= downcast<CSSPrimitiveValue>(currentValue.get());
    }
    return result;
}

template <typename T>
inline T StyleBuilderConverter::convertNumber(StyleResolver&, const CSSValue& value)
{
    return downcast<CSSPrimitiveValue>(value).value<T>(CSSPrimitiveValue::CSS_NUMBER);
}

template <typename T>
inline T StyleBuilderConverter::convertNumberOrAuto(StyleResolver& styleResolver, const CSSValue& value)
{
    if (downcast<CSSPrimitiveValue>(value).valueID() == CSSValueAuto)
        return -1;
    return convertNumber<T>(styleResolver, value);
}

inline short StyleBuilderConverter::convertWebkitHyphenateLimitLines(StyleResolver&, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueNoLimit)
        return -1;
    return primitiveValue.value<short>(CSSPrimitiveValue::CSS_NUMBER);
}

template <CSSPropertyID property>
inline NinePieceImage StyleBuilderConverter::convertBorderImage(StyleResolver& styleResolver, CSSValue& value)
{
    NinePieceImage image;
    styleResolver.styleMap()->mapNinePieceImage(property, &value, image);
    return image;
}

template <CSSPropertyID property>
inline NinePieceImage StyleBuilderConverter::convertBorderMask(StyleResolver& styleResolver, CSSValue& value)
{
    NinePieceImage image;
    image.setMaskDefaults();
    styleResolver.styleMap()->mapNinePieceImage(property, &value, image);
    return image;
}

template <CSSPropertyID property>
inline PassRefPtr<StyleImage> StyleBuilderConverter::convertStyleImage(StyleResolver& styleResolver, CSSValue& value)
{
    return styleResolver.styleImage(value);
}

inline TransformOperations StyleBuilderConverter::convertTransform(StyleResolver& styleResolver, const CSSValue& value)
{
    TransformOperations operations;
    transformsForValue(value, styleResolver.state().cssToLengthConversionData(), operations);
    return operations;
}

inline String StyleBuilderConverter::convertString(StyleResolver&, const CSSValue& value)
{
    return downcast<CSSPrimitiveValue>(value).stringValue();
}

inline String StyleBuilderConverter::convertStringOrAuto(StyleResolver& styleResolver, const CSSValue& value)
{
    if (downcast<CSSPrimitiveValue>(value).valueID() == CSSValueAuto)
        return nullAtom;
    return convertString(styleResolver, value);
}

inline String StyleBuilderConverter::convertStringOrNone(StyleResolver& styleResolver, const CSSValue& value)
{
    if (downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone)
        return nullAtom;
    return convertString(styleResolver, value);
}

inline TextEmphasisPosition StyleBuilderConverter::valueToEmphasisPosition(const CSSPrimitiveValue& primitiveValue)
{
    ASSERT(primitiveValue.isValueID());

    switch (primitiveValue.valueID()) {
    case CSSValueOver:
        return TextEmphasisPositionOver;
    case CSSValueUnder:
        return TextEmphasisPositionUnder;
    case CSSValueLeft:
        return TextEmphasisPositionLeft;
    case CSSValueRight:
        return TextEmphasisPositionRight;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return RenderStyle::initialTextEmphasisPosition();
}

inline TextEmphasisPosition StyleBuilderConverter::convertTextEmphasisPosition(StyleResolver&, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value))
        return valueToEmphasisPosition(downcast<CSSPrimitiveValue>(value));

    TextEmphasisPosition position = 0;
    for (auto& currentValue : downcast<CSSValueList>(value))
        position |= valueToEmphasisPosition(downcast<CSSPrimitiveValue>(currentValue.get()));
    return position;
}

inline ETextAlign StyleBuilderConverter::convertTextAlign(StyleResolver& styleResolver, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    ASSERT(primitiveValue.isValueID());

    if (primitiveValue.valueID() != CSSValueWebkitMatchParent)
        return primitiveValue;

    auto* parentStyle = styleResolver.parentStyle();
    if (parentStyle->textAlign() == TASTART)
        return parentStyle->isLeftToRightDirection() ? LEFT : RIGHT;
    if (parentStyle->textAlign() == TAEND)
        return parentStyle->isLeftToRightDirection() ? RIGHT : LEFT;
    return parentStyle->textAlign();
}

inline RefPtr<ClipPathOperation> StyleBuilderConverter::convertClipPath(StyleResolver& styleResolver, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
        if (primitiveValue.primitiveType() == CSSPrimitiveValue::CSS_URI) {
            String cssURLValue = primitiveValue.stringValue();
            URL url = styleResolver.document().completeURL(cssURLValue);
            // FIXME: It doesn't work with external SVG references (see https://bugs.webkit.org/show_bug.cgi?id=126133)
            return ReferenceClipPathOperation::create(cssURLValue, url.fragmentIdentifier());
        }
        ASSERT(primitiveValue.valueID() == CSSValueNone);
        return nullptr;
    }

    CSSBoxType referenceBox = BoxMissing;
    RefPtr<ClipPathOperation> operation;

    for (auto& currentValue : downcast<CSSValueList>(value)) {
        auto& primitiveValue = downcast<CSSPrimitiveValue>(currentValue.get());
        if (primitiveValue.isShape()) {
            ASSERT(!operation);
            operation = ShapeClipPathOperation::create(basicShapeForValue(styleResolver.state().cssToLengthConversionData(), *primitiveValue.shapeValue()));
        } else {
            ASSERT(primitiveValue.valueID() == CSSValueContentBox
                || primitiveValue.valueID() == CSSValueBorderBox
                || primitiveValue.valueID() == CSSValuePaddingBox
                || primitiveValue.valueID() == CSSValueMarginBox
                || primitiveValue.valueID() == CSSValueFill
                || primitiveValue.valueID() == CSSValueStroke
                || primitiveValue.valueID() == CSSValueViewBox);
            ASSERT(referenceBox == BoxMissing);
            referenceBox = primitiveValue;
        }
    }
    if (operation)
        downcast<ShapeClipPathOperation>(*operation).setReferenceBox(referenceBox);
    else {
        ASSERT(referenceBox != BoxMissing);
        operation = BoxClipPathOperation::create(referenceBox);
    }

    return operation;
}

inline EResize StyleBuilderConverter::convertResize(StyleResolver& styleResolver, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);

    EResize resize = RESIZE_NONE;
    if (primitiveValue.valueID() == CSSValueAuto) {
        if (Settings* settings = styleResolver.document().settings())
            resize = settings->textAreasAreResizable() ? RESIZE_BOTH : RESIZE_NONE;
    } else
        resize = primitiveValue;

    return resize;
}

inline int StyleBuilderConverter::convertMarqueeRepetition(StyleResolver&, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueInfinite)
        return -1; // -1 means repeat forever.

    ASSERT(primitiveValue.isNumber());
    return primitiveValue.intValue();
}

inline int StyleBuilderConverter::convertMarqueeSpeed(StyleResolver&, const CSSValue& value)
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

inline Ref<QuotesData> StyleBuilderConverter::convertQuotes(StyleResolver&, const CSSValue& value)
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

inline TextUnderlinePosition StyleBuilderConverter::convertTextUnderlinePosition(StyleResolver&, const CSSValue& value)
{
    // This is true if value is 'auto' or 'alphabetic'.
    if (is<CSSPrimitiveValue>(value))
        return downcast<CSSPrimitiveValue>(value);

    unsigned combinedPosition = 0;
    for (auto& currentValue : downcast<CSSValueList>(value)) {
        TextUnderlinePosition position = downcast<CSSPrimitiveValue>(currentValue.get());
        combinedPosition |= position;
    }
    return static_cast<TextUnderlinePosition>(combinedPosition);
}

inline RefPtr<StyleReflection> StyleBuilderConverter::convertReflection(StyleResolver& styleResolver, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone);
        return nullptr;
    }

    auto& reflectValue = downcast<CSSReflectValue>(value);

    auto reflection = StyleReflection::create();
    reflection->setDirection(reflectValue.direction());
    reflection->setOffset(reflectValue.offset().convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion>(styleResolver.state().cssToLengthConversionData()));

    NinePieceImage mask;
    mask.setMaskDefaults();
    styleResolver.styleMap()->mapNinePieceImage(CSSPropertyWebkitBoxReflect, reflectValue.mask(), mask);
    reflection->setMask(mask);

    return WTFMove(reflection);
}

inline IntSize StyleBuilderConverter::convertInitialLetter(StyleResolver&, const CSSValue& value)
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

inline float StyleBuilderConverter::convertTextStrokeWidth(StyleResolver& styleResolver, const CSSValue& value)
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
        width = convertComputedLength<float>(styleResolver, emsValue);
        break;
    }
    case CSSValueInvalid: {
        width = convertComputedLength<float>(styleResolver, primitiveValue);
        break;
    }
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }

    return width;
}

inline LineBoxContain StyleBuilderConverter::convertLineBoxContain(StyleResolver&, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone);
        return LineBoxContainNone;
    }

    return downcast<CSSLineBoxContainValue>(value).value();
}

inline TextDecorationSkip StyleBuilderConverter::valueToDecorationSkip(const CSSPrimitiveValue& primitiveValue)
{
    ASSERT(primitiveValue.isValueID());

    switch (primitiveValue.valueID()) {
    case CSSValueAuto:
        return TextDecorationSkipAuto;
    case CSSValueNone:
        return TextDecorationSkipNone;
    case CSSValueInk:
        return TextDecorationSkipInk;
    case CSSValueObjects:
        return TextDecorationSkipObjects;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TextDecorationSkipNone;
}

inline TextDecorationSkip StyleBuilderConverter::convertTextDecorationSkip(StyleResolver&, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value))
        return valueToDecorationSkip(downcast<CSSPrimitiveValue>(value));

    TextDecorationSkip skip = TextDecorationSkipNone;
    for (auto& currentValue : downcast<CSSValueList>(value))
        skip |= valueToDecorationSkip(downcast<CSSPrimitiveValue>(currentValue.get()));
    return skip;
}

static inline bool isImageShape(const CSSValue& value)
{
    return is<CSSImageValue>(value) || is<CSSImageSetValue>(value) || is<CSSImageGeneratorValue>(value);
}

inline PassRefPtr<ShapeValue> StyleBuilderConverter::convertShapeValue(StyleResolver& styleResolver, CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone);
        return nullptr;
    }

    if (isImageShape(value))
        return ShapeValue::createImageValue(styleResolver.styleImage(value));

    RefPtr<BasicShape> shape;
    CSSBoxType referenceBox = BoxMissing;
    for (auto& currentValue : downcast<CSSValueList>(value)) {
        CSSPrimitiveValue& primitiveValue = downcast<CSSPrimitiveValue>(currentValue.get());
        if (primitiveValue.isShape())
            shape = basicShapeForValue(styleResolver.state().cssToLengthConversionData(), *primitiveValue.shapeValue());
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
        return ShapeValue::createShapeValue(WTFMove(shape), referenceBox);

    if (referenceBox != BoxMissing)
        return ShapeValue::createBoxShapeValue(referenceBox);

    ASSERT_NOT_REACHED();
    return nullptr;
}

#if ENABLE(CSS_SCROLL_SNAP)
inline Length StyleBuilderConverter::parseSnapCoordinate(StyleResolver& styleResolver, const CSSValue& value)
{
    return downcast<CSSPrimitiveValue>(value).convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion | AutoConversion>(styleResolver.state().cssToLengthConversionData());
}

inline std::unique_ptr<ScrollSnapPoints> StyleBuilderConverter::convertScrollSnapPoints(StyleResolver& styleResolver, const CSSValue& value)
{
    // FIXME-NEWPARSER: Old parser supports an identifier value called "elements" when it seems like
    // "none" is what others use. For now, support both in the converter.
    auto points = std::make_unique<ScrollSnapPoints>();
    
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).valueID() == CSSValueElements || downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone);
        points->usesElements = true;
        return points;
    }
    
    for (auto& currentValue : downcast<CSSValueList>(value)) {
        if (is<CSSFunctionValue>(currentValue.get())) {
            auto& functionValue = downcast<CSSFunctionValue>(currentValue.get());
            points->repeatOffset = parseSnapCoordinate(styleResolver, *functionValue.arguments()->item(0));
            points->hasRepeat = true;
            break;
        }
        
        auto& itemValue = downcast<CSSPrimitiveValue>(currentValue.get());
        if (auto* lengthRepeat = itemValue.lengthRepeatValue()) {
            // FIXME-NEWPARSER: Old parsing code, which uses a special primitive value.
            // Can remove this once new parser turns on.
            if (auto* interval = lengthRepeat->interval()) {
                points->repeatOffset = parseSnapCoordinate(styleResolver, *interval);
                points->hasRepeat = true;
                break;
            }
        }
        points->offsets.append(parseSnapCoordinate(styleResolver, itemValue));
    }

    return points;
}

inline LengthSize StyleBuilderConverter::convertSnapCoordinatePair(StyleResolver& styleResolver, const CSSValue& value, size_t offset)
{
    // FIXME-NEWPARSER: Can make this unconditional once old parser is gone. This is just how
    // we detect that we're dealing with the new parser's pairs.
    if (value.isPrimitiveValue() && downcast<CSSPrimitiveValue>(value).pairValue()) {
        Pair* pair = downcast<CSSPrimitiveValue>(value).pairValue();
        Length lengthX = convertPositionComponent<CSSValueLeft, CSSValueRight>(styleResolver, *pair->first());
        Length lengthY = convertPositionComponent<CSSValueTop, CSSValueBottom>(styleResolver, *pair->second());
        return LengthSize(lengthX, lengthY);
    }

    // FIXME-NEWPARSER: Remove once old parser is gone.
    auto& valueList = downcast<CSSValueList>(value);
    return LengthSize(parseSnapCoordinate(styleResolver, *valueList.item(offset)), parseSnapCoordinate(styleResolver, *valueList.item(offset + 1)));
}

inline Vector<LengthSize> StyleBuilderConverter::convertScrollSnapCoordinates(StyleResolver& styleResolver, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone);
        return Vector<LengthSize>();
    }

    auto& valueList = downcast<CSSValueList>(value);
    if (!valueList.length())
        return Vector<LengthSize>();
    
    // FIXME-NEWPARSER: Can make this unconditional once old parser is gone. This is just how
    // we detect that we're dealing with the new parser's pairs.
    if (valueList.item(0)->isPrimitiveValue() && downcast<CSSPrimitiveValue>(*valueList.item(0)).pairValue()) {
        Vector<LengthSize> coordinates;
        coordinates.reserveInitialCapacity(valueList.length());
        for (auto& snapCoordinate : valueList) {
            Pair* pair = downcast<CSSPrimitiveValue>(*snapCoordinate.ptr()).pairValue();
            Length lengthX = convertPositionComponent<CSSValueLeft, CSSValueRight>(styleResolver, *pair->first());
            Length lengthY = convertPositionComponent<CSSValueTop, CSSValueBottom>(styleResolver, *pair->second());
            coordinates.uncheckedAppend(LengthSize(lengthX, lengthY));
        }
        return coordinates;
    }
    
    // FIXME-NEWPARSER: Remove all of this once old parser is gone, including all the functions
    // it calls.
    ASSERT(!(valueList.length() % 2));
    size_t pointCount = valueList.length() / 2;
    Vector<LengthSize> coordinates;
    coordinates.reserveInitialCapacity(pointCount);
    for (size_t i = 0; i < pointCount; ++i)
        coordinates.uncheckedAppend(convertSnapCoordinatePair(styleResolver, valueList, i * 2));
    return coordinates;
}
#endif

#if ENABLE(CSS_GRID_LAYOUT)
inline GridLength StyleBuilderConverter::createGridTrackBreadth(const CSSPrimitiveValue& primitiveValue, StyleResolver& styleResolver)
{
    if (primitiveValue.valueID() == CSSValueWebkitMinContent)
        return Length(MinContent);

    if (primitiveValue.valueID() == CSSValueWebkitMaxContent)
        return Length(MaxContent);

    // Fractional unit.
    if (primitiveValue.isFlex())
        return GridLength(primitiveValue.doubleValue());

    return primitiveValue.convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion | AutoConversion>(styleResolver.state().cssToLengthConversionData());
}

inline GridTrackSize StyleBuilderConverter::createGridTrackSize(const CSSValue& value, StyleResolver& styleResolver)
{
    if (is<CSSPrimitiveValue>(value))
        return GridTrackSize(createGridTrackBreadth(downcast<CSSPrimitiveValue>(value), styleResolver));

    ASSERT(is<CSSFunctionValue>(value));
    CSSValueList& arguments = *downcast<CSSFunctionValue>(value).arguments();

    if (arguments.length() == 1)
        return GridTrackSize(createGridTrackBreadth(downcast<CSSPrimitiveValue>(*arguments.itemWithoutBoundsCheck(0)), styleResolver), FitContentTrackSizing);

    ASSERT_WITH_SECURITY_IMPLICATION(arguments.length() == 2);
    GridLength minTrackBreadth(createGridTrackBreadth(downcast<CSSPrimitiveValue>(*arguments.itemWithoutBoundsCheck(0)), styleResolver));
    GridLength maxTrackBreadth(createGridTrackBreadth(downcast<CSSPrimitiveValue>(*arguments.itemWithoutBoundsCheck(1)), styleResolver));
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

inline bool StyleBuilderConverter::createGridTrackList(const CSSValue& value, TracksData& tracksData, StyleResolver& styleResolver)
{
    // Handle 'none'.
    if (is<CSSPrimitiveValue>(value))
        return downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone;

    if (!is<CSSValueList>(value))
        return false;

    unsigned currentNamedGridLine = 0;
    for (auto& currentValue : downcast<CSSValueList>(value)) {
        if (is<CSSGridLineNamesValue>(currentValue.get())) {
            createGridLineNamesList(currentValue.get(), currentNamedGridLine, tracksData.m_namedGridLines, tracksData.m_orderedNamedGridLines);
            continue;
        }

        if (is<CSSGridAutoRepeatValue>(currentValue)) {
            ASSERT(tracksData.m_autoRepeatTrackSizes.isEmpty());
            unsigned autoRepeatIndex = 0;
            CSSValueID autoRepeatID = downcast<CSSGridAutoRepeatValue>(currentValue.get()).autoRepeatID();
            ASSERT(autoRepeatID == CSSValueAutoFill || autoRepeatID == CSSValueAutoFit);
            tracksData.m_autoRepeatType = autoRepeatID == CSSValueAutoFill ? AutoFill : AutoFit;
            for (auto& autoRepeatValue : downcast<CSSValueList>(currentValue.get())) {
                if (is<CSSGridLineNamesValue>(autoRepeatValue.get())) {
                    createGridLineNamesList(autoRepeatValue.get(), autoRepeatIndex, tracksData.m_autoRepeatNamedGridLines, tracksData.m_autoRepeatOrderedNamedGridLines);
                    continue;
                }
                ++autoRepeatIndex;
                tracksData.m_autoRepeatTrackSizes.append(createGridTrackSize(autoRepeatValue.get(), styleResolver));
            }
            tracksData.m_autoRepeatInsertionPoint = currentNamedGridLine++;
            continue;
        }

        ++currentNamedGridLine;
        tracksData.m_trackSizes.append(createGridTrackSize(currentValue, styleResolver));
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

inline Vector<GridTrackSize> StyleBuilderConverter::convertGridTrackSizeList(StyleResolver& styleResolver, const CSSValue& value)
{
    ASSERT(value.isValueList());
    auto& valueList = downcast<CSSValueList>(value);
    Vector<GridTrackSize> trackSizes;
    trackSizes.reserveInitialCapacity(valueList.length());
    for (auto& currValue : valueList) {
        ASSERT(!currValue->isGridLineNamesValue());
        ASSERT(!currValue->isGridAutoRepeatValue());
        trackSizes.uncheckedAppend(convertGridTrackSize(styleResolver, currValue));
    }
    return trackSizes;
}

inline GridTrackSize StyleBuilderConverter::convertGridTrackSize(StyleResolver& styleResolver, const CSSValue& value)
{
    return createGridTrackSize(value, styleResolver);
}

inline std::optional<GridPosition> StyleBuilderConverter::convertGridPosition(StyleResolver&, const CSSValue& value)
{
    GridPosition gridPosition;
    if (createGridPosition(value, gridPosition))
        return gridPosition;
    return std::nullopt;
}

inline GridAutoFlow StyleBuilderConverter::convertGridAutoFlow(StyleResolver&, const CSSValue& value)
{
    auto& list = downcast<CSSValueList>(value);
    if (!list.length())
        return RenderStyle::initialGridAutoFlow();

    auto& first = downcast<CSSPrimitiveValue>(*list.item(0));
    auto* second = downcast<CSSPrimitiveValue>(list.item(1));

    GridAutoFlow autoFlow = RenderStyle::initialGridAutoFlow();
    switch (first.valueID()) {
    case CSSValueRow:
        if (second && second->valueID() == CSSValueDense)
            autoFlow =  AutoFlowRowDense;
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
            return AutoFlowColumnDense;
        return AutoFlowRowDense;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    return autoFlow;
}
#endif // ENABLE(CSS_GRID_LAYOUT)

inline CSSToLengthConversionData StyleBuilderConverter::csstoLengthConversionDataWithTextZoomFactor(StyleResolver& styleResolver)
{
    if (auto* frame = styleResolver.document().frame()) {
        float textZoomFactor = styleResolver.style()->textZoom() != TextZoomReset ? frame->textZoomFactor() : 1.0f;
        return styleResolver.state().cssToLengthConversionData().copyWithAdjustedZoom(styleResolver.style()->effectiveZoom() * textZoomFactor);
    }
    return styleResolver.state().cssToLengthConversionData();
}

inline std::optional<Length> StyleBuilderConverter::convertWordSpacing(StyleResolver& styleResolver, const CSSValue& value)
{
    std::optional<Length> wordSpacing;
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueNormal)
        wordSpacing = RenderStyle::initialWordSpacing();
    else if (primitiveValue.isLength())
        wordSpacing = primitiveValue.computeLength<Length>(csstoLengthConversionDataWithTextZoomFactor(styleResolver));
    else if (primitiveValue.isPercentage())
        wordSpacing = Length(clampTo<float>(primitiveValue.doubleValue(), minValueForCssLength, maxValueForCssLength), Percent);
    else if (primitiveValue.isNumber())
        wordSpacing = Length(primitiveValue.doubleValue(), Fixed);

    return wordSpacing;
}

inline std::optional<float> StyleBuilderConverter::convertPerspective(StyleResolver& styleResolver, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueNone)
        return 0.f;

    float perspective = -1;
    if (primitiveValue.isLength())
        perspective = primitiveValue.computeLength<float>(styleResolver.state().cssToLengthConversionData());
    else if (primitiveValue.isNumber())
        perspective = primitiveValue.doubleValue() * styleResolver.state().cssToLengthConversionData().zoom();
    else
        ASSERT_NOT_REACHED();

    return perspective < 0 ? std::optional<float>(std::nullopt) : std::optional<float>(perspective);
}

inline std::optional<Length> StyleBuilderConverter::convertMarqueeIncrement(StyleResolver& styleResolver, const CSSValue& value)
{
    std::optional<Length> marqueeLength;
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
        Length length = primitiveValue.convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion>(styleResolver.state().cssToLengthConversionData().copyWithAdjustedZoom(1.0f));
        if (!length.isUndefined())
            marqueeLength = length;
        break;
    }
    default:
        break;
    }
    return marqueeLength;
}

inline std::optional<FilterOperations> StyleBuilderConverter::convertFilterOperations(StyleResolver& styleResolver, const CSSValue& value)
{
    FilterOperations operations;
    if (styleResolver.createFilterOperations(value, operations))
        return operations;
    return std::nullopt;
}

inline FontFeatureSettings StyleBuilderConverter::convertFontFeatureSettings(StyleResolver&, const CSSValue& value)
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

#if ENABLE(VARIATION_FONTS)
inline FontVariationSettings StyleBuilderConverter::convertFontVariationSettings(StyleResolver&, const CSSValue& value)
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

#if PLATFORM(IOS)
inline bool StyleBuilderConverter::convertTouchCallout(StyleResolver&, const CSSValue& value)
{
    return !equalLettersIgnoringASCIICase(downcast<CSSPrimitiveValue>(value).stringValue(), "none");
}
#endif

#if ENABLE(TOUCH_EVENTS)
inline Color StyleBuilderConverter::convertTapHighlightColor(StyleResolver& styleResolver, const CSSValue& value)
{
    return styleResolver.colorFromPrimitiveValue(downcast<CSSPrimitiveValue>(value));
}
#endif

#if ENABLE(ACCELERATED_OVERFLOW_SCROLLING)
inline bool StyleBuilderConverter::convertOverflowScrolling(StyleResolver&, const CSSValue& value)
{
    return downcast<CSSPrimitiveValue>(value).valueID() == CSSValueTouch;
}
#endif

inline SVGLengthValue StyleBuilderConverter::convertSVGLengthValue(StyleResolver&, const CSSValue& value)
{
    return SVGLengthValue::fromCSSPrimitiveValue(downcast<CSSPrimitiveValue>(value));
}

inline Vector<SVGLengthValue> StyleBuilderConverter::convertSVGLengthVector(StyleResolver& styleResolver, const CSSValue& value)
{
    auto& valueList = downcast<CSSValueList>(value);

    Vector<SVGLengthValue> svgLengths;
    svgLengths.reserveInitialCapacity(valueList.length());
    for (auto& item : valueList)
        svgLengths.uncheckedAppend(convertSVGLengthValue(styleResolver, item));

    return svgLengths;
}

inline Vector<SVGLengthValue> StyleBuilderConverter::convertStrokeDashArray(StyleResolver& styleResolver, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone);
        return SVGRenderStyle::initialStrokeDashArray();
    }

    return convertSVGLengthVector(styleResolver, value);
}

inline PaintOrder StyleBuilderConverter::convertPaintOrder(StyleResolver&, const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNormal);
        return PaintOrderNormal;
    }

    auto& orderTypeList = downcast<CSSValueList>(value);
    switch (downcast<CSSPrimitiveValue>(*orderTypeList.itemWithoutBoundsCheck(0)).valueID()) {
    case CSSValueFill:
        return orderTypeList.length() > 1 ? PaintOrderFillMarkers : PaintOrderFill;
    case CSSValueStroke:
        return orderTypeList.length() > 1 ? PaintOrderStrokeMarkers : PaintOrderStroke;
    case CSSValueMarkers:
        return orderTypeList.length() > 1 ? PaintOrderMarkersStroke : PaintOrderMarkers;
    default:
        ASSERT_NOT_REACHED();
        return PaintOrderNormal;
    }
}

inline float StyleBuilderConverter::convertOpacity(StyleResolver&, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    float opacity = primitiveValue.floatValue();
    if (primitiveValue.isPercentage())
        opacity /= 100.0f;
    return opacity;
}

inline String StyleBuilderConverter::convertSVGURIReference(StyleResolver& styleResolver, const CSSValue& value)
{
    String s;
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.isURI())
        s = primitiveValue.stringValue();

    return SVGURIReference::fragmentIdentifierFromIRIString(s, styleResolver.document());
}

inline Color StyleBuilderConverter::convertSVGColor(StyleResolver& styleResolver, const CSSValue& value)
{
    // FIXME-NEWPARSER: Remove the code that assumes SVGColors.
    // FIXME: What about visited link support?
    if (is<CSSPrimitiveValue>(value))
        return styleResolver.colorFromPrimitiveValue(downcast<CSSPrimitiveValue>(value));

    auto& svgColor = downcast<SVGColor>(value);
    return svgColor.colorType() == SVGColor::SVG_COLORTYPE_CURRENTCOLOR ? styleResolver.style()->color() : svgColor.color();
}

inline StyleSelfAlignmentData StyleBuilderConverter::convertSelfOrDefaultAlignmentData(StyleResolver&, const CSSValue& value)
{
    StyleSelfAlignmentData alignmentData = RenderStyle::initialSelfAlignment();
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (Pair* pairValue = primitiveValue.pairValue()) {
        if (pairValue->first()->valueID() == CSSValueLegacy) {
            alignmentData.setPositionType(LegacyPosition);
            alignmentData.setPosition(*pairValue->second());
        } else {
            alignmentData.setPosition(*pairValue->first());
            alignmentData.setOverflow(*pairValue->second());
        }
    } else
        alignmentData.setPosition(primitiveValue);
    return alignmentData;
}

inline StyleContentAlignmentData StyleBuilderConverter::convertContentAlignmentData(StyleResolver&, const CSSValue& value)
{
    StyleContentAlignmentData alignmentData = RenderStyle::initialContentAlignment();
#if ENABLE(CSS_GRID_LAYOUT)
    if (RuntimeEnabledFeatures::sharedFeatures().isCSSGridLayoutEnabled()) {
        auto& contentValue = downcast<CSSContentDistributionValue>(value);
        if (contentValue.distribution()->valueID() != CSSValueInvalid)
            alignmentData.setDistribution(contentValue.distribution().get());
        if (contentValue.position()->valueID() != CSSValueInvalid)
            alignmentData.setPosition(contentValue.position().get());
        if (contentValue.overflow()->valueID() != CSSValueInvalid)
            alignmentData.setOverflow(contentValue.overflow().get());
        return alignmentData;
    }
#endif
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    switch (primitiveValue.valueID()) {
    case CSSValueStretch:
    case CSSValueSpaceBetween:
    case CSSValueSpaceAround:
        alignmentData.setDistribution(primitiveValue);
        break;
    case CSSValueFlexStart:
    case CSSValueFlexEnd:
    case CSSValueCenter:
        alignmentData.setPosition(primitiveValue);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    return alignmentData;
}

inline EGlyphOrientation StyleBuilderConverter::convertGlyphOrientation(StyleResolver&, const CSSValue& value)
{
    float angle = fabsf(fmodf(downcast<CSSPrimitiveValue>(value).floatValue(), 360.0f));
    if (angle <= 45.0f || angle > 315.0f)
        return GO_0DEG;
    if (angle > 45.0f && angle <= 135.0f)
        return GO_90DEG;
    if (angle > 135.0f && angle <= 225.0f)
        return GO_180DEG;
    return GO_270DEG;
}

inline EGlyphOrientation StyleBuilderConverter::convertGlyphOrientationOrAuto(StyleResolver& styleResolver, const CSSValue& value)
{
    if (downcast<CSSPrimitiveValue>(value).valueID() == CSSValueAuto)
        return GO_AUTO;
    return convertGlyphOrientation(styleResolver, value);
}

inline std::optional<Length> StyleBuilderConverter::convertLineHeight(StyleResolver& styleResolver, const CSSValue& value, float multiplier)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueNormal)
        return RenderStyle::initialLineHeight();

    if (primitiveValue.isLength()) {
        Length length = primitiveValue.computeLength<Length>(StyleBuilderConverter::csstoLengthConversionDataWithTextZoomFactor(styleResolver));
        if (multiplier != 1.f)
            length = Length(length.value() * multiplier, Fixed);
        return length;
    }
    if (primitiveValue.isPercentage()) {
        // FIXME: percentage should not be restricted to an integer here.
        return Length((styleResolver.style()->computedFontSize() * primitiveValue.intValue()) / 100, Fixed);
    }
    if (primitiveValue.isNumber()) {
        // FIXME: number and percentage values should produce the same type of Length (ie. Fixed or Percent).
        return Length(primitiveValue.doubleValue() * multiplier * 100.0, Percent);
    }
    return std::nullopt;
}

inline FontSynthesis StyleBuilderConverter::convertFontSynthesis(StyleResolver&, const CSSValue& value)
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
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    return result;
}

inline BreakBetween StyleBuilderConverter::convertPageBreakBetween(StyleResolver&, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueAlways)
        return PageBreakBetween;
    if (primitiveValue.valueID() == CSSValueAvoid)
        return AvoidPageBreakBetween;
    return primitiveValue;
}

inline BreakInside StyleBuilderConverter::convertPageBreakInside(StyleResolver&, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueAvoid)
        return AvoidPageBreakInside;
    return primitiveValue;
}

inline BreakBetween StyleBuilderConverter::convertColumnBreakBetween(StyleResolver&, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueAlways)
        return ColumnBreakBetween;
    if (primitiveValue.valueID() == CSSValueAvoid)
        return AvoidColumnBreakBetween;
    return primitiveValue;
}

inline BreakInside StyleBuilderConverter::convertColumnBreakInside(StyleResolver&, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueAvoid)
        return AvoidColumnBreakInside;
    return primitiveValue;
}

#if ENABLE(CSS_REGIONS)
inline BreakBetween StyleBuilderConverter::convertRegionBreakBetween(StyleResolver&, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueAlways)
        return RegionBreakBetween;
    if (primitiveValue.valueID() == CSSValueAvoid)
        return AvoidRegionBreakBetween;
    return primitiveValue;
}

inline BreakInside StyleBuilderConverter::convertRegionBreakInside(StyleResolver&, const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueAvoid)
        return AvoidRegionBreakInside;
    return primitiveValue;
}
#endif

inline HangingPunctuation StyleBuilderConverter::convertHangingPunctuation(StyleResolver&, const CSSValue& value)
{
    HangingPunctuation result = RenderStyle::initialHangingPunctuation();
    if (is<CSSValueList>(value)) {
        for (auto& currentValue : downcast<CSSValueList>(value))
            result |= downcast<CSSPrimitiveValue>(currentValue.get());
    }
    return result;
}

} // namespace WebCore
