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
#include "CSSPrimitiveValue.h"
#include "Length.h"
#include "Pair.h"
#include "QuotesData.h"
#include "Settings.h"
#include "StyleResolver.h"
#include "TransformFunctions.h"

namespace WebCore {

// Note that we assume the CSS parser only allows valid CSSValue types.
class StyleBuilderConverter {
public:
    static Length convertLength(StyleResolver&, CSSValue&);
    static Length convertLengthOrAuto(StyleResolver&, CSSValue&);
    static Length convertLengthSizing(StyleResolver&, CSSValue&);
    static Length convertLengthMaxSizing(StyleResolver&, CSSValue&);
    template <typename T> static T convertComputedLength(StyleResolver&, CSSValue&);
    template <typename T> static T convertLineWidth(StyleResolver&, CSSValue&);
    static float convertSpacing(StyleResolver&, CSSValue&);
    static LengthSize convertRadius(StyleResolver&, CSSValue&);
    static TextDecoration convertTextDecoration(StyleResolver&, CSSValue&);
    template <typename T> static T convertNumber(StyleResolver&, CSSValue&);
    static short convertWebkitHyphenateLimitLines(StyleResolver&, CSSValue&);
    template <CSSPropertyID property> static NinePieceImage convertBorderImage(StyleResolver&, CSSValue&);
    template <CSSPropertyID property> static NinePieceImage convertBorderMask(StyleResolver&, CSSValue&);
    template <CSSPropertyID property> static PassRefPtr<StyleImage> convertBorderImageSource(StyleResolver&, CSSValue&);
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

private:
    static Length convertToRadiusLength(CSSToLengthConversionData&, CSSPrimitiveValue&);
    static TextEmphasisPosition valueToEmphasisPosition(CSSPrimitiveValue&);
};

inline Length StyleBuilderConverter::convertLength(StyleResolver& styleResolver, CSSValue& value)
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
        return result;
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
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.getValueID() == CSSValueAuto)
        return -1;
    return primitiveValue.getValue<T>(CSSPrimitiveValue::CSS_NUMBER);
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
inline PassRefPtr<StyleImage> StyleBuilderConverter::convertBorderImageSource(StyleResolver& styleResolver, CSSValue& value)
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

} // namespace WebCore

#endif // StyleBuilderConverter_h
