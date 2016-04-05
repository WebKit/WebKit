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

#ifndef StyleBuilderConverter_h
#define StyleBuilderConverter_h

#include "BasicShapeFunctions.h"
#include "CSSCalculationValue.h"
#include "CSSContentDistributionValue.h"
#include "CSSFontFeatureValue.h"
#include "CSSFunctionValue.h"
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
    static Length convertLengthOrAuto(StyleResolver&, CSSValue&);
    static Length convertLengthSizing(StyleResolver&, CSSValue&);
    static Length convertLengthMaxSizing(StyleResolver&, CSSValue&);
    template <typename T> static T convertComputedLength(StyleResolver&, CSSValue&);
    template <typename T> static T convertLineWidth(StyleResolver&, CSSValue&);
    static float convertSpacing(StyleResolver&, CSSValue&);
    static LengthSize convertRadius(StyleResolver&, CSSValue&);
    static LengthPoint convertObjectPosition(StyleResolver&, CSSValue&);
    static TextDecoration convertTextDecoration(StyleResolver&, CSSValue&);
    template <typename T> static T convertNumber(StyleResolver&, CSSValue&);
    template <typename T> static T convertNumberOrAuto(StyleResolver&, CSSValue&);
    static short convertWebkitHyphenateLimitLines(StyleResolver&, CSSValue&);
    template <CSSPropertyID property> static NinePieceImage convertBorderImage(StyleResolver&, CSSValue&);
    template <CSSPropertyID property> static NinePieceImage convertBorderMask(StyleResolver&, CSSValue&);
    template <CSSPropertyID property> static PassRefPtr<StyleImage> convertStyleImage(StyleResolver&, CSSValue&);
    static TransformOperations convertTransform(StyleResolver&, CSSValue&);
    static String convertString(StyleResolver&, CSSValue&);
    static String convertStringOrAuto(StyleResolver&, CSSValue&);
    static String convertStringOrNone(StyleResolver&, CSSValue&);
    static TextEmphasisPosition convertTextEmphasisPosition(StyleResolver&, CSSValue&);
    static ETextAlign convertTextAlign(StyleResolver&, CSSValue&);
    static PassRefPtr<ClipPathOperation> convertClipPath(StyleResolver&, CSSValue&);
    static EResize convertResize(StyleResolver&, CSSValue&);
    static int convertMarqueeRepetition(StyleResolver&, CSSValue&);
    static int convertMarqueeSpeed(StyleResolver&, CSSValue&);
    static PassRefPtr<QuotesData> convertQuotes(StyleResolver&, CSSValue&);
    static TextUnderlinePosition convertTextUnderlinePosition(StyleResolver&, CSSValue&);
    static PassRefPtr<StyleReflection> convertReflection(StyleResolver&, CSSValue&);
    static IntSize convertInitialLetter(StyleResolver&, CSSValue&);
    static float convertTextStrokeWidth(StyleResolver&, CSSValue&);
    static LineBoxContain convertLineBoxContain(StyleResolver&, CSSValue&);
    static TextDecorationSkip convertTextDecorationSkip(StyleResolver&, CSSValue&);
    static PassRefPtr<ShapeValue> convertShapeValue(StyleResolver&, CSSValue&);
#if ENABLE(CSS_SCROLL_SNAP)
    static std::unique_ptr<ScrollSnapPoints> convertScrollSnapPoints(StyleResolver&, CSSValue&);
    static LengthSize convertSnapCoordinatePair(StyleResolver&, CSSValue&, size_t offset = 0);
    static Vector<LengthSize> convertScrollSnapCoordinates(StyleResolver&, CSSValue&);
#endif
#if ENABLE(CSS_GRID_LAYOUT)
    static GridTrackSize convertGridTrackSize(StyleResolver&, CSSValue&);
    static Optional<GridPosition> convertGridPosition(StyleResolver&, CSSValue&);
    static GridAutoFlow convertGridAutoFlow(StyleResolver&, CSSValue&);
#endif // ENABLE(CSS_GRID_LAYOUT)
    static Optional<Length> convertWordSpacing(StyleResolver&, CSSValue&);
    static Optional<float> convertPerspective(StyleResolver&, CSSValue&);
    static Optional<Length> convertMarqueeIncrement(StyleResolver&, CSSValue&);
    static Optional<FilterOperations> convertFilterOperations(StyleResolver&, CSSValue&);
#if PLATFORM(IOS)
    static bool convertTouchCallout(StyleResolver&, CSSValue&);
#endif
#if ENABLE(TOUCH_EVENTS)
    static Color convertTapHighlightColor(StyleResolver&, CSSValue&);
#endif
#if ENABLE(ACCELERATED_OVERFLOW_SCROLLING)
    static bool convertOverflowScrolling(StyleResolver&, CSSValue&);
#endif
    static FontFeatureSettings convertFontFeatureSettings(StyleResolver&, CSSValue&);
    static SVGLength convertSVGLength(StyleResolver&, CSSValue&);
    static Vector<SVGLength> convertSVGLengthVector(StyleResolver&, CSSValue&);
    static Vector<SVGLength> convertStrokeDashArray(StyleResolver&, CSSValue&);
    static PaintOrder convertPaintOrder(StyleResolver&, CSSValue&);
    static float convertOpacity(StyleResolver&, CSSValue&);
    static String convertSVGURIReference(StyleResolver&, CSSValue&);
    static Color convertSVGColor(StyleResolver&, CSSValue&);
    static StyleSelfAlignmentData convertSelfOrDefaultAlignmentData(StyleResolver&, CSSValue&);
    static StyleContentAlignmentData convertContentAlignmentData(StyleResolver&, CSSValue&);
    static EGlyphOrientation convertGlyphOrientation(StyleResolver&, CSSValue&);
    static EGlyphOrientation convertGlyphOrientationOrAuto(StyleResolver&, CSSValue&);
    static Optional<Length> convertLineHeight(StyleResolver&, CSSValue&, float multiplier = 1.f);
    static FontSynthesis convertFontSynthesis(StyleResolver&, CSSValue&);
    
    static BreakBetween convertPageBreakBetween(StyleResolver&, CSSValue&);
    static BreakInside convertPageBreakInside(StyleResolver&, CSSValue&);
    static BreakBetween convertColumnBreakBetween(StyleResolver&, CSSValue&);
    static BreakInside convertColumnBreakInside(StyleResolver&, CSSValue&);
#if ENABLE(CSS_REGIONS)
    static BreakBetween convertRegionBreakBetween(StyleResolver&, CSSValue&);
    static BreakInside convertRegionBreakInside(StyleResolver&, CSSValue&);
#endif
    
    static HangingPunctuation convertHangingPunctuation(StyleResolver&, CSSValue&);

private:
    friend class StyleBuilderCustom;

    static Length convertToRadiusLength(CSSToLengthConversionData&, CSSPrimitiveValue&);
    static TextEmphasisPosition valueToEmphasisPosition(CSSPrimitiveValue&);
    static TextDecorationSkip valueToDecorationSkip(const CSSPrimitiveValue&);
#if ENABLE(CSS_SCROLL_SNAP)
    static Length parseSnapCoordinate(StyleResolver&, const CSSValue&);
#endif

    static Length convertTo100PercentMinusLength(const Length&);
    static Length convertPositionComponent(StyleResolver&, const CSSPrimitiveValue&);

#if ENABLE(CSS_GRID_LAYOUT)
    static GridLength createGridTrackBreadth(CSSPrimitiveValue&, StyleResolver&);
    static GridTrackSize createGridTrackSize(CSSValue&, StyleResolver&);
    static bool createGridTrackList(CSSValue&, Vector<GridTrackSize>& trackSizes, NamedGridLinesMap&, OrderedNamedGridLinesMap&, StyleResolver&);
    static bool createGridPosition(CSSValue&, GridPosition&);
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
        return Length(primitiveValue.getDoubleValue(), Percent);

    if (primitiveValue.isCalculatedPercentageWithLength())
        return Length(primitiveValue.cssCalcValue()->createCalculationValue(conversionData));

    ASSERT_NOT_REACHED();
    return Length(0, Fixed);
}

inline Length StyleBuilderConverter::convertLengthOrAuto(StyleResolver& styleResolver, CSSValue& value)
{
    if (downcast<CSSPrimitiveValue>(value).getValueID() == CSSValueAuto)
        return Length(Auto);
    return convertLength(styleResolver, value);
}

inline Length StyleBuilderConverter::convertLengthSizing(StyleResolver& styleResolver, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    switch (primitiveValue.getValueID()) {
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

inline Length StyleBuilderConverter::convertLengthMaxSizing(StyleResolver& styleResolver, CSSValue& value)
{
    if (downcast<CSSPrimitiveValue>(value).getValueID() == CSSValueNone)
        return Length(Undefined);
    return convertLengthSizing(styleResolver, value);
}

template <typename T>
inline T StyleBuilderConverter::convertComputedLength(StyleResolver& styleResolver, CSSValue& value)
{
    return downcast<CSSPrimitiveValue>(value).computeLength<T>(styleResolver.state().cssToLengthConversionData());
}

template <typename T>
inline T StyleBuilderConverter::convertLineWidth(StyleResolver& styleResolver, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    switch (primitiveValue.getValueID()) {
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

inline float StyleBuilderConverter::convertSpacing(StyleResolver& styleResolver, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.getValueID() == CSSValueNormal)
        return 0.f;

    CSSToLengthConversionData conversionData = styleResolver.useSVGZoomRulesForLength() ?
        styleResolver.state().cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
        : styleResolver.state().cssToLengthConversionData();
    return primitiveValue.computeLength<float>(conversionData);
}

inline Length StyleBuilderConverter::convertToRadiusLength(CSSToLengthConversionData& conversionData, CSSPrimitiveValue& value)
{
    if (value.isPercentage())
        return Length(value.getDoubleValue(), Percent);
    if (value.isCalculatedPercentageWithLength())
        return Length(value.cssCalcValue()->createCalculationValue(conversionData));
    return value.computeLength<Length>(conversionData);
}

inline LengthSize StyleBuilderConverter::convertRadius(StyleResolver& styleResolver, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    Pair* pair = primitiveValue.getPairValue();
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
    return Length(CalculationValue::create(WTFMove(op), CalculationRangeAll));
}

inline Length StyleBuilderConverter::convertPositionComponent(StyleResolver& styleResolver, const CSSPrimitiveValue& value)
{
    Length length;

    auto* lengthValue = &value;
    bool relativeToTrailingEdge = false;
    
    if (value.isPair()) {
        auto& first = *value.getPairValue()->first();
        if (first.getValueID() == CSSValueRight || first.getValueID() == CSSValueBottom)
            relativeToTrailingEdge = true;

        lengthValue = value.getPairValue()->second();
    }

    length = convertLength(styleResolver, *lengthValue);

    if (relativeToTrailingEdge)
        length = convertTo100PercentMinusLength(length);

    return length;
}

inline LengthPoint StyleBuilderConverter::convertObjectPosition(StyleResolver& styleResolver, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    Pair* pair = primitiveValue.getPairValue();
    if (!pair || !pair->first() || !pair->second())
        return RenderStyle::initialObjectPosition();

    Length lengthX = convertPositionComponent(styleResolver, *pair->first());
    Length lengthY = convertPositionComponent(styleResolver, *pair->second());

    return LengthPoint(lengthX, lengthY);
}

inline TextDecoration StyleBuilderConverter::convertTextDecoration(StyleResolver&, CSSValue& value)
{
    TextDecoration result = RenderStyle::initialTextDecoration();
    if (is<CSSValueList>(value)) {
        for (auto& currentValue : downcast<CSSValueList>(value))
            result |= downcast<CSSPrimitiveValue>(currentValue.get());
    }
    return result;
}

template <typename T>
inline T StyleBuilderConverter::convertNumber(StyleResolver&, CSSValue& value)
{
    return downcast<CSSPrimitiveValue>(value).getValue<T>(CSSPrimitiveValue::CSS_NUMBER);
}

template <typename T>
inline T StyleBuilderConverter::convertNumberOrAuto(StyleResolver& styleResolver, CSSValue& value)
{
    if (downcast<CSSPrimitiveValue>(value).getValueID() == CSSValueAuto)
        return -1;
    return convertNumber<T>(styleResolver, value);
}

inline short StyleBuilderConverter::convertWebkitHyphenateLimitLines(StyleResolver&, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.getValueID() == CSSValueNoLimit)
        return -1;
    return primitiveValue.getValue<short>(CSSPrimitiveValue::CSS_NUMBER);
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
    return styleResolver.styleImage(property, value);
}

inline TransformOperations StyleBuilderConverter::convertTransform(StyleResolver& styleResolver, CSSValue& value)
{
    TransformOperations operations;
    transformsForValue(value, styleResolver.state().cssToLengthConversionData(), operations);
    return operations;
}

inline String StyleBuilderConverter::convertString(StyleResolver&, CSSValue& value)
{
    return downcast<CSSPrimitiveValue>(value).getStringValue();
}

inline String StyleBuilderConverter::convertStringOrAuto(StyleResolver& styleResolver, CSSValue& value)
{
    if (downcast<CSSPrimitiveValue>(value).getValueID() == CSSValueAuto)
        return nullAtom;
    return convertString(styleResolver, value);
}

inline String StyleBuilderConverter::convertStringOrNone(StyleResolver& styleResolver, CSSValue& value)
{
    if (downcast<CSSPrimitiveValue>(value).getValueID() == CSSValueNone)
        return nullAtom;
    return convertString(styleResolver, value);
}

inline TextEmphasisPosition StyleBuilderConverter::valueToEmphasisPosition(CSSPrimitiveValue& primitiveValue)
{
    ASSERT(primitiveValue.isValueID());

    switch (primitiveValue.getValueID()) {
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

inline TextEmphasisPosition StyleBuilderConverter::convertTextEmphasisPosition(StyleResolver&, CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value))
        return valueToEmphasisPosition(downcast<CSSPrimitiveValue>(value));

    TextEmphasisPosition position = 0;
    for (auto& currentValue : downcast<CSSValueList>(value))
        position |= valueToEmphasisPosition(downcast<CSSPrimitiveValue>(currentValue.get()));
    return position;
}

inline ETextAlign StyleBuilderConverter::convertTextAlign(StyleResolver& styleResolver, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    ASSERT(primitiveValue.isValueID());

    if (primitiveValue.getValueID() != CSSValueWebkitMatchParent)
        return primitiveValue;

    auto* parentStyle = styleResolver.parentStyle();
    if (parentStyle->textAlign() == TASTART)
        return parentStyle->isLeftToRightDirection() ? LEFT : RIGHT;
    if (parentStyle->textAlign() == TAEND)
        return parentStyle->isLeftToRightDirection() ? RIGHT : LEFT;
    return parentStyle->textAlign();
}

inline PassRefPtr<ClipPathOperation> StyleBuilderConverter::convertClipPath(StyleResolver& styleResolver, CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
        if (primitiveValue.primitiveType() == CSSPrimitiveValue::CSS_URI) {
            String cssURLValue = primitiveValue.getStringValue();
            URL url = styleResolver.document().completeURL(cssURLValue);
            // FIXME: It doesn't work with external SVG references (see https://bugs.webkit.org/show_bug.cgi?id=126133)
            return ReferenceClipPathOperation::create(cssURLValue, url.fragmentIdentifier());
        }
        ASSERT(primitiveValue.getValueID() == CSSValueNone);
        return nullptr;
    }

    CSSBoxType referenceBox = BoxMissing;
    RefPtr<ClipPathOperation> operation;

    for (auto& currentValue : downcast<CSSValueList>(value)) {
        auto& primitiveValue = downcast<CSSPrimitiveValue>(currentValue.get());
        if (primitiveValue.isShape()) {
            ASSERT(!operation);
            operation = ShapeClipPathOperation::create(basicShapeForValue(styleResolver.state().cssToLengthConversionData(), primitiveValue.getShapeValue()));
        } else {
            ASSERT(primitiveValue.getValueID() == CSSValueContentBox
                   || primitiveValue.getValueID() == CSSValueBorderBox
                   || primitiveValue.getValueID() == CSSValuePaddingBox
                   || primitiveValue.getValueID() == CSSValueMarginBox
                   || primitiveValue.getValueID() == CSSValueFill
                   || primitiveValue.getValueID() == CSSValueStroke
                   || primitiveValue.getValueID() == CSSValueViewBox);
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

    return operation.release();
}

inline EResize StyleBuilderConverter::convertResize(StyleResolver& styleResolver, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);

    EResize resize = RESIZE_NONE;
    if (primitiveValue.getValueID() == CSSValueAuto) {
        if (Settings* settings = styleResolver.document().settings())
            resize = settings->textAreasAreResizable() ? RESIZE_BOTH : RESIZE_NONE;
    } else
        resize = primitiveValue;

    return resize;
}

inline int StyleBuilderConverter::convertMarqueeRepetition(StyleResolver&, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.getValueID() == CSSValueInfinite)
        return -1; // -1 means repeat forever.

    ASSERT(primitiveValue.isNumber());
    return primitiveValue.getIntValue();
}

inline int StyleBuilderConverter::convertMarqueeSpeed(StyleResolver&, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    int speed = 85;
    if (CSSValueID ident = primitiveValue.getValueID()) {
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
        speed = primitiveValue.getIntValue();
    }
    return speed;
}

inline PassRefPtr<QuotesData> StyleBuilderConverter::convertQuotes(StyleResolver&, CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).getValueID() == CSSValueNone);
        return QuotesData::create(Vector<std::pair<String, String>>());
    }

    CSSValueList& list = downcast<CSSValueList>(value);
    Vector<std::pair<String, String>> quotes;
    quotes.reserveInitialCapacity(list.length() / 2);
    for (unsigned i = 0; i < list.length(); i += 2) {
        CSSValue* first = list.itemWithoutBoundsCheck(i);
        // item() returns null if out of bounds so this is safe.
        CSSValue* second = list.item(i + 1);
        if (!second)
            break;
        String startQuote = downcast<CSSPrimitiveValue>(*first).getStringValue();
        String endQuote = downcast<CSSPrimitiveValue>(*second).getStringValue();
        quotes.append(std::make_pair(startQuote, endQuote));
    }
    return QuotesData::create(quotes);
}

inline TextUnderlinePosition StyleBuilderConverter::convertTextUnderlinePosition(StyleResolver&, CSSValue& value)
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

inline PassRefPtr<StyleReflection> StyleBuilderConverter::convertReflection(StyleResolver& styleResolver, CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).getValueID() == CSSValueNone);
        return nullptr;
    }

    auto& reflectValue = downcast<CSSReflectValue>(value);

    RefPtr<StyleReflection> reflection = StyleReflection::create();
    reflection->setDirection(*reflectValue.direction());

    if (reflectValue.offset())
        reflection->setOffset(reflectValue.offset()->convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion>(styleResolver.state().cssToLengthConversionData()));

    NinePieceImage mask;
    mask.setMaskDefaults();
    styleResolver.styleMap()->mapNinePieceImage(CSSPropertyWebkitBoxReflect, reflectValue.mask(), mask);
    reflection->setMask(mask);

    return reflection.release();
}

inline IntSize StyleBuilderConverter::convertInitialLetter(StyleResolver&, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);

    if (primitiveValue.getValueID() == CSSValueNormal)
        return IntSize();

    Pair* pair = primitiveValue.getPairValue();
    ASSERT(pair);
    ASSERT(pair->first());
    ASSERT(pair->second());

    return IntSize(pair->first()->getIntValue(), pair->second()->getIntValue());
}

inline float StyleBuilderConverter::convertTextStrokeWidth(StyleResolver& styleResolver, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);

    float width = 0;
    switch (primitiveValue.getValueID()) {
    case CSSValueThin:
    case CSSValueMedium:
    case CSSValueThick: {
        double result = 1.0 / 48;
        if (primitiveValue.getValueID() == CSSValueMedium)
            result *= 3;
        else if (primitiveValue.getValueID() == CSSValueThick)
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

inline LineBoxContain StyleBuilderConverter::convertLineBoxContain(StyleResolver&, CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).getValueID() == CSSValueNone);
        return LineBoxContainNone;
    }

    return downcast<CSSLineBoxContainValue>(value).value();
}

inline TextDecorationSkip StyleBuilderConverter::valueToDecorationSkip(const CSSPrimitiveValue& primitiveValue)
{
    ASSERT(primitiveValue.isValueID());

    switch (primitiveValue.getValueID()) {
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

inline TextDecorationSkip StyleBuilderConverter::convertTextDecorationSkip(StyleResolver&, CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value))
        return valueToDecorationSkip(downcast<CSSPrimitiveValue>(value));

    TextDecorationSkip skip = RenderStyle::initialTextDecorationSkip();
    for (auto& currentValue : downcast<CSSValueList>(value))
        skip |= valueToDecorationSkip(downcast<CSSPrimitiveValue>(currentValue.get()));
    return skip;
}

#if ENABLE(CSS_SHAPES)
static inline bool isImageShape(const CSSValue& value)
{
    return is<CSSImageValue>(value)
#if ENABLE(CSS_IMAGE_SET)
        || is<CSSImageSetValue>(value)
#endif 
        || is<CSSImageGeneratorValue>(value);
}

inline PassRefPtr<ShapeValue> StyleBuilderConverter::convertShapeValue(StyleResolver& styleResolver, CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).getValueID() == CSSValueNone);
        return nullptr;
    }

    if (isImageShape(value))
        return ShapeValue::createImageValue(styleResolver.styleImage(CSSPropertyWebkitShapeOutside, value));

    RefPtr<BasicShape> shape;
    CSSBoxType referenceBox = BoxMissing;
    for (auto& currentValue : downcast<CSSValueList>(value)) {
        CSSPrimitiveValue& primitiveValue = downcast<CSSPrimitiveValue>(currentValue.get());
        if (primitiveValue.isShape())
            shape = basicShapeForValue(styleResolver.state().cssToLengthConversionData(), primitiveValue.getShapeValue());
        else if (primitiveValue.getValueID() == CSSValueContentBox
            || primitiveValue.getValueID() == CSSValueBorderBox
            || primitiveValue.getValueID() == CSSValuePaddingBox
            || primitiveValue.getValueID() == CSSValueMarginBox)
            referenceBox = primitiveValue;
        else {
            ASSERT_NOT_REACHED();
            return nullptr;
        }
    }

    if (shape)
        return ShapeValue::createShapeValue(shape.release(), referenceBox);

    if (referenceBox != BoxMissing)
        return ShapeValue::createBoxShapeValue(referenceBox);

    ASSERT_NOT_REACHED();
    return nullptr;
}
#endif // ENABLE(CSS_SHAPES)

#if ENABLE(CSS_SCROLL_SNAP)
inline Length StyleBuilderConverter::parseSnapCoordinate(StyleResolver& styleResolver, const CSSValue& value)
{
    return downcast<CSSPrimitiveValue>(value).convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion | AutoConversion>(styleResolver.state().cssToLengthConversionData());
}

inline std::unique_ptr<ScrollSnapPoints> StyleBuilderConverter::convertScrollSnapPoints(StyleResolver& styleResolver, CSSValue& value)
{
    auto points = std::make_unique<ScrollSnapPoints>();

    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).getValueID() == CSSValueElements);
        points->usesElements = true;
        return points;
    }

    points->hasRepeat = false;
    for (auto& currentValue : downcast<CSSValueList>(value)) {
        auto& itemValue = downcast<CSSPrimitiveValue>(currentValue.get());
        if (auto* lengthRepeat = itemValue.getLengthRepeatValue()) {
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

inline LengthSize StyleBuilderConverter::convertSnapCoordinatePair(StyleResolver& styleResolver, CSSValue& value, size_t offset)
{
    auto& valueList = downcast<CSSValueList>(value);
    return LengthSize(parseSnapCoordinate(styleResolver, *valueList.item(offset)), parseSnapCoordinate(styleResolver, *valueList.item(offset + 1)));
}

inline Vector<LengthSize> StyleBuilderConverter::convertScrollSnapCoordinates(StyleResolver& styleResolver, CSSValue& value)
{
    auto& valueList = downcast<CSSValueList>(value);
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
inline GridLength StyleBuilderConverter::createGridTrackBreadth(CSSPrimitiveValue& primitiveValue, StyleResolver& styleResolver)
{
    if (primitiveValue.getValueID() == CSSValueWebkitMinContent)
        return Length(MinContent);

    if (primitiveValue.getValueID() == CSSValueWebkitMaxContent)
        return Length(MaxContent);

    // Fractional unit.
    if (primitiveValue.isFlex())
        return GridLength(primitiveValue.getDoubleValue());

    return primitiveValue.convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion | AutoConversion>(styleResolver.state().cssToLengthConversionData());
}

inline GridTrackSize StyleBuilderConverter::createGridTrackSize(CSSValue& value, StyleResolver& styleResolver)
{
    if (is<CSSPrimitiveValue>(value))
        return GridTrackSize(createGridTrackBreadth(downcast<CSSPrimitiveValue>(value), styleResolver));

    CSSValueList& arguments = *downcast<CSSFunctionValue>(value).arguments();
    ASSERT_WITH_SECURITY_IMPLICATION(arguments.length() == 2);

    GridLength minTrackBreadth(createGridTrackBreadth(downcast<CSSPrimitiveValue>(*arguments.itemWithoutBoundsCheck(0)), styleResolver));
    GridLength maxTrackBreadth(createGridTrackBreadth(downcast<CSSPrimitiveValue>(*arguments.itemWithoutBoundsCheck(1)), styleResolver));
    return GridTrackSize(minTrackBreadth, maxTrackBreadth);
}

inline bool StyleBuilderConverter::createGridTrackList(CSSValue& value, Vector<GridTrackSize>& trackSizes, NamedGridLinesMap& namedGridLines, OrderedNamedGridLinesMap& orderedNamedGridLines, StyleResolver& styleResolver)
{
    // Handle 'none'.
    if (is<CSSPrimitiveValue>(value))
        return downcast<CSSPrimitiveValue>(value).getValueID() == CSSValueNone;

    if (!is<CSSValueList>(value))
        return false;

    unsigned currentNamedGridLine = 0;
    for (auto& currentValue : downcast<CSSValueList>(value)) {
        if (is<CSSGridLineNamesValue>(currentValue.get())) {
            for (auto& currentGridLineName : downcast<CSSGridLineNamesValue>(currentValue.get())) {
                String namedGridLine = downcast<CSSPrimitiveValue>(currentGridLineName.get()).getStringValue();
                NamedGridLinesMap::AddResult result = namedGridLines.add(namedGridLine, Vector<unsigned>());
                result.iterator->value.append(currentNamedGridLine);
                OrderedNamedGridLinesMap::AddResult orderedResult = orderedNamedGridLines.add(currentNamedGridLine, Vector<String>());
                orderedResult.iterator->value.append(namedGridLine);
            }
            continue;
        }

        ++currentNamedGridLine;
        trackSizes.append(createGridTrackSize(currentValue, styleResolver));
    }

    // The parser should have rejected any <track-list> without any <track-size> as
    // this is not conformant to the syntax.
    ASSERT(!trackSizes.isEmpty());
    return true;
}

inline bool StyleBuilderConverter::createGridPosition(CSSValue& value, GridPosition& position)
{
    // We accept the specification's grammar:
    // auto | <custom-ident> | [ <integer> && <custom-ident>? ] | [ span && [ <integer> || <custom-ident> ] ]
    if (is<CSSPrimitiveValue>(value)) {
        auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
        // We translate <ident> to <string> during parsing as it makes handling it simpler.
        if (primitiveValue.isString()) {
            position.setNamedGridArea(primitiveValue.getStringValue());
            return true;
        }

        ASSERT(primitiveValue.getValueID() == CSSValueAuto);
        return true;
    }

    auto& values = downcast<CSSValueList>(value);
    ASSERT(values.length());

    auto it = values.begin();
    CSSPrimitiveValue* currentValue = &downcast<CSSPrimitiveValue>(it->get());
    bool isSpanPosition = false;
    if (currentValue->getValueID() == CSSValueSpan) {
        isSpanPosition = true;
        ++it;
        currentValue = it != values.end() ? &downcast<CSSPrimitiveValue>(it->get()) : nullptr;
    }

    int gridLineNumber = 0;
    if (currentValue && currentValue->isNumber()) {
        gridLineNumber = currentValue->getIntValue();
        ++it;
        currentValue = it != values.end() ? &downcast<CSSPrimitiveValue>(it->get()) : nullptr;
    }

    String gridLineName;
    if (currentValue && currentValue->isString()) {
        gridLineName = currentValue->getStringValue();
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

inline GridTrackSize StyleBuilderConverter::convertGridTrackSize(StyleResolver& styleResolver, CSSValue& value)
{
    return createGridTrackSize(value, styleResolver);
}

inline Optional<GridPosition> StyleBuilderConverter::convertGridPosition(StyleResolver&, CSSValue& value)
{
    GridPosition gridPosition;
    if (createGridPosition(value, gridPosition))
        return gridPosition;
    return Nullopt;
}

inline GridAutoFlow StyleBuilderConverter::convertGridAutoFlow(StyleResolver&, CSSValue& value)
{
    auto& list = downcast<CSSValueList>(value);
    if (!list.length())
        return RenderStyle::initialGridAutoFlow();

    auto& first = downcast<CSSPrimitiveValue>(*list.item(0));
    auto* second = downcast<CSSPrimitiveValue>(list.item(1));

    GridAutoFlow autoFlow = RenderStyle::initialGridAutoFlow();
    switch (first.getValueID()) {
    case CSSValueRow:
        if (second && second->getValueID() == CSSValueDense)
            autoFlow =  AutoFlowRowDense;
        else
            autoFlow = AutoFlowRow;
        break;
    case CSSValueColumn:
        if (second && second->getValueID() == CSSValueDense)
            autoFlow = AutoFlowColumnDense;
        else
            autoFlow = AutoFlowColumn;
        break;
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

inline Optional<Length> StyleBuilderConverter::convertWordSpacing(StyleResolver& styleResolver, CSSValue& value)
{
    Optional<Length> wordSpacing;
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.getValueID() == CSSValueNormal)
        wordSpacing = RenderStyle::initialWordSpacing();
    else if (primitiveValue.isLength())
        wordSpacing = primitiveValue.computeLength<Length>(csstoLengthConversionDataWithTextZoomFactor(styleResolver));
    else if (primitiveValue.isPercentage())
        wordSpacing = Length(clampTo<float>(primitiveValue.getDoubleValue(), minValueForCssLength, maxValueForCssLength), Percent);
    else if (primitiveValue.isNumber())
        wordSpacing = Length(primitiveValue.getDoubleValue(), Fixed);

    return wordSpacing;
}

inline Optional<float> StyleBuilderConverter::convertPerspective(StyleResolver& styleResolver, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.getValueID() == CSSValueNone)
        return 0.f;

    float perspective = -1;
    if (primitiveValue.isLength())
        perspective = primitiveValue.computeLength<float>(styleResolver.state().cssToLengthConversionData());
    else if (primitiveValue.isNumber())
        perspective = primitiveValue.getDoubleValue() * styleResolver.state().cssToLengthConversionData().zoom();
    else
        ASSERT_NOT_REACHED();

    return perspective < 0 ? Optional<float>(Nullopt) : Optional<float>(perspective);
}

inline Optional<Length> StyleBuilderConverter::convertMarqueeIncrement(StyleResolver& styleResolver, CSSValue& value)
{
    Optional<Length> marqueeLength;
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    switch (primitiveValue.getValueID()) {
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

inline Optional<FilterOperations> StyleBuilderConverter::convertFilterOperations(StyleResolver& styleResolver, CSSValue& value)
{
    FilterOperations operations;
    if (styleResolver.createFilterOperations(value, operations))
        return operations;
    return Nullopt;
}

inline FontFeatureSettings StyleBuilderConverter::convertFontFeatureSettings(StyleResolver&, CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).getValueID() == CSSValueNormal);
        return { };
    }

    FontFeatureSettings settings;
    for (auto& item : downcast<CSSValueList>(value)) {
        auto& feature = downcast<CSSFontFeatureValue>(item.get());
        settings.insert(FontFeature(feature.tag(), feature.value()));
    }
    return settings;
}

#if PLATFORM(IOS)
inline bool StyleBuilderConverter::convertTouchCallout(StyleResolver&, CSSValue& value)
{
    return !equalLettersIgnoringASCIICase(downcast<CSSPrimitiveValue>(value).getStringValue(), "none");
}
#endif

#if ENABLE(TOUCH_EVENTS)
inline Color StyleBuilderConverter::convertTapHighlightColor(StyleResolver& styleResolver, CSSValue& value)
{
    return styleResolver.colorFromPrimitiveValue(downcast<CSSPrimitiveValue>(value));
}
#endif

#if ENABLE(ACCELERATED_OVERFLOW_SCROLLING)
inline bool StyleBuilderConverter::convertOverflowScrolling(StyleResolver&, CSSValue& value)
{
    return downcast<CSSPrimitiveValue>(value).getValueID() == CSSValueTouch;
}
#endif

inline SVGLength StyleBuilderConverter::convertSVGLength(StyleResolver&, CSSValue& value)
{
    return SVGLength::fromCSSPrimitiveValue(downcast<CSSPrimitiveValue>(value));
}

inline Vector<SVGLength> StyleBuilderConverter::convertSVGLengthVector(StyleResolver& styleResolver, CSSValue& value)
{
    auto& valueList = downcast<CSSValueList>(value);

    Vector<SVGLength> svgLengths;
    svgLengths.reserveInitialCapacity(valueList.length());
    for (auto& item : valueList)
        svgLengths.uncheckedAppend(convertSVGLength(styleResolver, item));

    return svgLengths;
}

inline Vector<SVGLength> StyleBuilderConverter::convertStrokeDashArray(StyleResolver& styleResolver, CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).getValueID() == CSSValueNone);
        return SVGRenderStyle::initialStrokeDashArray();
    }

    return convertSVGLengthVector(styleResolver, value);
}

inline PaintOrder StyleBuilderConverter::convertPaintOrder(StyleResolver&, CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).getValueID() == CSSValueNormal);
        return PaintOrderNormal;
    }

    auto& orderTypeList = downcast<CSSValueList>(value);
    switch (downcast<CSSPrimitiveValue>(*orderTypeList.itemWithoutBoundsCheck(0)).getValueID()) {
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

inline float StyleBuilderConverter::convertOpacity(StyleResolver&, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    float opacity = primitiveValue.getFloatValue();
    if (primitiveValue.isPercentage())
        opacity /= 100.0f;
    return opacity;
}

inline String StyleBuilderConverter::convertSVGURIReference(StyleResolver& styleResolver, CSSValue& value)
{
    String s;
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.isURI())
        s = primitiveValue.getStringValue();

    return SVGURIReference::fragmentIdentifierFromIRIString(s, styleResolver.document());
}

inline Color StyleBuilderConverter::convertSVGColor(StyleResolver& styleResolver, CSSValue& value)
{
    auto& svgColor = downcast<SVGColor>(value);
    return svgColor.colorType() == SVGColor::SVG_COLORTYPE_CURRENTCOLOR ? styleResolver.style()->color() : svgColor.color();
}

inline StyleSelfAlignmentData StyleBuilderConverter::convertSelfOrDefaultAlignmentData(StyleResolver&, CSSValue& value)
{
    StyleSelfAlignmentData alignmentData = RenderStyle::initialSelfAlignment();
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (Pair* pairValue = primitiveValue.getPairValue()) {
        if (pairValue->first()->getValueID() == CSSValueLegacy) {
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

inline StyleContentAlignmentData StyleBuilderConverter::convertContentAlignmentData(StyleResolver&, CSSValue& value)
{
    StyleContentAlignmentData alignmentData = RenderStyle::initialContentAlignment();
    auto& contentValue = downcast<CSSContentDistributionValue>(value);
    if (contentValue.distribution()->getValueID() != CSSValueInvalid)
        alignmentData.setDistribution(contentValue.distribution().get());
    if (contentValue.position()->getValueID() != CSSValueInvalid)
        alignmentData.setPosition(contentValue.position().get());
    if (contentValue.overflow()->getValueID() != CSSValueInvalid)
        alignmentData.setOverflow(contentValue.overflow().get());
    return alignmentData;
}

inline EGlyphOrientation StyleBuilderConverter::convertGlyphOrientation(StyleResolver&, CSSValue& value)
{
    float angle = fabsf(fmodf(downcast<CSSPrimitiveValue>(value).getFloatValue(), 360.0f));
    if (angle <= 45.0f || angle > 315.0f)
        return GO_0DEG;
    if (angle > 45.0f && angle <= 135.0f)
        return GO_90DEG;
    if (angle > 135.0f && angle <= 225.0f)
        return GO_180DEG;
    return GO_270DEG;
}

inline EGlyphOrientation StyleBuilderConverter::convertGlyphOrientationOrAuto(StyleResolver& styleResolver, CSSValue& value)
{
    if (downcast<CSSPrimitiveValue>(value).getValueID() == CSSValueAuto)
        return GO_AUTO;
    return convertGlyphOrientation(styleResolver, value);
}

inline Optional<Length> StyleBuilderConverter::convertLineHeight(StyleResolver& styleResolver, CSSValue& value, float multiplier)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.getValueID() == CSSValueNormal)
        return RenderStyle::initialLineHeight();

    if (primitiveValue.isLength()) {
        Length length = primitiveValue.computeLength<Length>(StyleBuilderConverter::csstoLengthConversionDataWithTextZoomFactor(styleResolver));
        if (multiplier != 1.f)
            length = Length(length.value() * multiplier, Fixed);
        return length;
    }
    if (primitiveValue.isPercentage()) {
        // FIXME: percentage should not be restricted to an integer here.
        return Length((styleResolver.style()->computedFontSize() * primitiveValue.getIntValue()) / 100, Fixed);
    }
    if (primitiveValue.isNumber()) {
        // FIXME: number and percentage values should produce the same type of Length (ie. Fixed or Percent).
        return Length(primitiveValue.getDoubleValue() * multiplier * 100.0, Percent);
    }
    return Nullopt;
}

FontSynthesis StyleBuilderConverter::convertFontSynthesis(StyleResolver&, CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).getValueID() == CSSValueNone);
        return FontSynthesisNone;
    }

    FontSynthesis result = FontSynthesisNone;
    ASSERT(is<CSSValueList>(value));
    for (CSSValue& v : downcast<CSSValueList>(value)) {
        switch (downcast<CSSPrimitiveValue>(v).getValueID()) {
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

inline BreakBetween StyleBuilderConverter::convertPageBreakBetween(StyleResolver&, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.getValueID() == CSSValueAlways)
        return PageBreakBetween;
    if (primitiveValue.getValueID() == CSSValueAvoid)
        return AvoidPageBreakBetween;
    return primitiveValue;
}

inline BreakInside StyleBuilderConverter::convertPageBreakInside(StyleResolver&, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.getValueID() == CSSValueAvoid)
        return AvoidPageBreakInside;
    return primitiveValue;
}

inline BreakBetween StyleBuilderConverter::convertColumnBreakBetween(StyleResolver&, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.getValueID() == CSSValueAlways)
        return ColumnBreakBetween;
    if (primitiveValue.getValueID() == CSSValueAvoid)
        return AvoidColumnBreakBetween;
    return primitiveValue;
}

inline BreakInside StyleBuilderConverter::convertColumnBreakInside(StyleResolver&, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.getValueID() == CSSValueAvoid)
        return AvoidColumnBreakInside;
    return primitiveValue;
}

#if ENABLE(CSS_REGIONS)
inline BreakBetween StyleBuilderConverter::convertRegionBreakBetween(StyleResolver&, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.getValueID() == CSSValueAlways)
        return RegionBreakBetween;
    if (primitiveValue.getValueID() == CSSValueAvoid)
        return AvoidRegionBreakBetween;
    return primitiveValue;
}

inline BreakInside StyleBuilderConverter::convertRegionBreakInside(StyleResolver&, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.getValueID() == CSSValueAvoid)
        return AvoidRegionBreakInside;
    return primitiveValue;
}
#endif

inline HangingPunctuation StyleBuilderConverter::convertHangingPunctuation(StyleResolver&, CSSValue& value)
{
    HangingPunctuation result = RenderStyle::initialHangingPunctuation();
    if (is<CSSValueList>(value)) {
        for (auto& currentValue : downcast<CSSValueList>(value))
            result |= downcast<CSSPrimitiveValue>(currentValue.get());
    }
    return result;
}

} // namespace WebCore

#endif // StyleBuilderConverter_h
