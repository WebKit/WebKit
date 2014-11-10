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

#ifndef StyleBuilderCustom_h
#define StyleBuilderCustom_h

#include "BasicShapeFunctions.h"
#include "CSSImageGeneratorValue.h"
#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
#include "StyleResolver.h"

namespace WebCore {

// Note that we assume the CSS parser only allows valid CSSValue types.
namespace StyleBuilderFunctions {

inline void applyValueWebkitMarqueeIncrement(StyleResolver& styleResolver, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    Length marqueeLength(Undefined);
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
    case CSSValueInvalid:
        marqueeLength = primitiveValue.convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion>(styleResolver.state().cssToLengthConversionData().copyWithAdjustedZoom(1.0f));
        break;
    default:
        break;
    }
    if (!marqueeLength.isUndefined())
        styleResolver.style()->setMarqueeIncrement(marqueeLength);
}

inline void applyValueDirection(StyleResolver& styleResolver, CSSValue& value)
{
    styleResolver.style()->setDirection(downcast<CSSPrimitiveValue>(value));

    Element* element = styleResolver.element();
    if (element && styleResolver.element() == element->document().documentElement())
        element->document().setDirectionSetOnDocumentElement(true);
}

static inline void resetEffectiveZoom(StyleResolver& styleResolver)
{
    // Reset the zoom in effect. This allows the setZoom method to accurately compute a new zoom in effect.
    styleResolver.setEffectiveZoom(styleResolver.parentStyle() ? styleResolver.parentStyle()->effectiveZoom() : RenderStyle::initialZoom());
}

inline void applyInitialZoom(StyleResolver& styleResolver)
{
    resetEffectiveZoom(styleResolver);
    styleResolver.setZoom(RenderStyle::initialZoom());
}

inline void applyInheritZoom(StyleResolver& styleResolver)
{
    resetEffectiveZoom(styleResolver);
    styleResolver.setZoom(styleResolver.parentStyle()->zoom());
}

inline void applyValueZoom(StyleResolver& styleResolver, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);

    if (primitiveValue.getValueID() == CSSValueNormal) {
        resetEffectiveZoom(styleResolver);
        styleResolver.setZoom(RenderStyle::initialZoom());
    } else if (primitiveValue.getValueID() == CSSValueReset) {
        styleResolver.setEffectiveZoom(RenderStyle::initialZoom());
        styleResolver.setZoom(RenderStyle::initialZoom());
    } else if (primitiveValue.getValueID() == CSSValueDocument) {
        float docZoom = styleResolver.rootElementStyle() ? styleResolver.rootElementStyle()->zoom() : RenderStyle::initialZoom();
        styleResolver.setEffectiveZoom(docZoom);
        styleResolver.setZoom(docZoom);
    } else if (primitiveValue.isPercentage()) {
        resetEffectiveZoom(styleResolver);
        if (float percent = primitiveValue.getFloatValue())
            styleResolver.setZoom(percent / 100.0f);
    } else if (primitiveValue.isNumber()) {
        resetEffectiveZoom(styleResolver);
        if (float number = primitiveValue.getFloatValue())
            styleResolver.setZoom(number);
    }
}

#if ENABLE(CSS_SHAPES)
inline void applyValueWebkitShapeOutside(StyleResolver& styleResolver, CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        // FIXME: Shouldn't this be CSSValueNone?
        // http://www.w3.org/TR/css-shapes/#shape-outside-property
        if (downcast<CSSPrimitiveValue>(value).getValueID() == CSSValueAuto)
            styleResolver.style()->setShapeOutside(nullptr);
    } if (is<CSSImageValue>(value) || is<CSSImageGeneratorValue>(value) || is<CSSImageSetValue>(value)) {
        RefPtr<ShapeValue> shape = ShapeValue::createImageValue(styleResolver.styleImage(CSSPropertyWebkitShapeOutside, value));
        styleResolver.style()->setShapeOutside(shape.release());
    } else if (is<CSSValueList>(value)) {
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
                referenceBox = CSSBoxType(primitiveValue);
            else
                return;
        }

        if (shape)
            styleResolver.style()->setShapeOutside(ShapeValue::createShapeValue(shape.release(), referenceBox));
        else if (referenceBox != BoxMissing)
            styleResolver.style()->setShapeOutside(ShapeValue::createBoxShapeValue(referenceBox));
    }
}
#endif // ENABLE(CSS_SHAPES)

static inline Length mmLength(double mm)
{
    Ref<CSSPrimitiveValue> value(CSSPrimitiveValue::create(mm, CSSPrimitiveValue::CSS_MM));
    return value.get().computeLength<Length>(CSSToLengthConversionData());
}
static inline Length inchLength(double inch)
{
    Ref<CSSPrimitiveValue> value(CSSPrimitiveValue::create(inch, CSSPrimitiveValue::CSS_IN));
    return value.get().computeLength<Length>(CSSToLengthConversionData());
}
static bool getPageSizeFromName(CSSPrimitiveValue* pageSizeName, CSSPrimitiveValue* pageOrientation, Length& width, Length& height)
{
    static NeverDestroyed<Length> a5Width(mmLength(148));
    static NeverDestroyed<Length> a5Height(mmLength(210));
    static NeverDestroyed<Length> a4Width(mmLength(210));
    static NeverDestroyed<Length> a4Height(mmLength(297));
    static NeverDestroyed<Length> a3Width(mmLength(297));
    static NeverDestroyed<Length> a3Height(mmLength(420));
    static NeverDestroyed<Length> b5Width(mmLength(176));
    static NeverDestroyed<Length> b5Height(mmLength(250));
    static NeverDestroyed<Length> b4Width(mmLength(250));
    static NeverDestroyed<Length> b4Height(mmLength(353));
    static NeverDestroyed<Length> letterWidth(inchLength(8.5));
    static NeverDestroyed<Length> letterHeight(inchLength(11));
    static NeverDestroyed<Length> legalWidth(inchLength(8.5));
    static NeverDestroyed<Length> legalHeight(inchLength(14));
    static NeverDestroyed<Length> ledgerWidth(inchLength(11));
    static NeverDestroyed<Length> ledgerHeight(inchLength(17));

    if (!pageSizeName)
        return false;

    switch (pageSizeName->getValueID()) {
    case CSSValueA5:
        width = a5Width;
        height = a5Height;
        break;
    case CSSValueA4:
        width = a4Width;
        height = a4Height;
        break;
    case CSSValueA3:
        width = a3Width;
        height = a3Height;
        break;
    case CSSValueB5:
        width = b5Width;
        height = b5Height;
        break;
    case CSSValueB4:
        width = b4Width;
        height = b4Height;
        break;
    case CSSValueLetter:
        width = letterWidth;
        height = letterHeight;
        break;
    case CSSValueLegal:
        width = legalWidth;
        height = legalHeight;
        break;
    case CSSValueLedger:
        width = ledgerWidth;
        height = ledgerHeight;
        break;
    default:
        return false;
    }

    if (pageOrientation) {
        switch (pageOrientation->getValueID()) {
        case CSSValueLandscape:
            std::swap(width, height);
            break;
        case CSSValuePortrait:
            // Nothing to do.
            break;
        default:
            return false;
        }
    }
    return true;
}

inline void applyInheritSize(StyleResolver&) { }
inline void applyInitialSize(StyleResolver&) { }
inline void applyValueSize(StyleResolver& styleResolver, CSSValue& value)
{
    styleResolver.style()->resetPageSizeType();
    Length width;
    Length height;
    PageSizeType pageSizeType = PAGE_SIZE_AUTO;
    if (!is<CSSValueList>(value))
        return;

    auto& valueList = downcast<CSSValueList>(value);
    switch (valueList.length()) {
    case 2: {
        CSSValue* firstValue = valueList.itemWithoutBoundsCheck(0);
        CSSValue* secondValue = valueList.itemWithoutBoundsCheck(1);
        // <length>{2} | <page-size> <orientation>
        if (!is<CSSPrimitiveValue>(*firstValue) || !is<CSSPrimitiveValue>(*secondValue))
            return;
        auto& firstPrimitiveValue = downcast<CSSPrimitiveValue>(*firstValue);
        auto& secondPrimitiveValue = downcast<CSSPrimitiveValue>(*secondValue);
        if (firstPrimitiveValue.isLength()) {
            // <length>{2}
            if (!secondPrimitiveValue.isLength())
                return;
            CSSToLengthConversionData conversionData = styleResolver.state().cssToLengthConversionData().copyWithAdjustedZoom(1.0f);
            width = firstPrimitiveValue.computeLength<Length>(conversionData);
            height = secondPrimitiveValue.computeLength<Length>(conversionData);
        } else {
            // <page-size> <orientation>
            // The value order is guaranteed. See CSSParser::parseSizeParameter.
            if (!getPageSizeFromName(&firstPrimitiveValue, &secondPrimitiveValue, width, height))
                return;
        }
        pageSizeType = PAGE_SIZE_RESOLVED;
        break;
    }
    case 1: {
        CSSValue* value = valueList.itemWithoutBoundsCheck(0);
        // <length> | auto | <page-size> | [ portrait | landscape]
        if (!is<CSSPrimitiveValue>(*value))
            return;
        auto& primitiveValue = downcast<CSSPrimitiveValue>(*value);
        if (primitiveValue.isLength()) {
            // <length>
            pageSizeType = PAGE_SIZE_RESOLVED;
            width = height = primitiveValue.computeLength<Length>(styleResolver.state().cssToLengthConversionData().copyWithAdjustedZoom(1.0f));
        } else {
            switch (primitiveValue.getValueID()) {
            case 0:
                return;
            case CSSValueAuto:
                pageSizeType = PAGE_SIZE_AUTO;
                break;
            case CSSValuePortrait:
                pageSizeType = PAGE_SIZE_AUTO_PORTRAIT;
                break;
            case CSSValueLandscape:
                pageSizeType = PAGE_SIZE_AUTO_LANDSCAPE;
                break;
            default:
                // <page-size>
                pageSizeType = PAGE_SIZE_RESOLVED;
                if (!getPageSizeFromName(&primitiveValue, nullptr, width, height))
                    return;
            }
        }
        break;
    }
    default:
        return;
    }
    styleResolver.style()->setPageSizeType(pageSizeType);
    styleResolver.style()->setPageSize(LengthSize(width, height));
}

inline void applyInheritTextIndent(StyleResolver& styleResolver)
{
    styleResolver.style()->setTextIndent(styleResolver.parentStyle()->textIndent());
#if ENABLE(CSS3_TEXT)
    styleResolver.style()->setTextIndentLine(styleResolver.parentStyle()->textIndentLine());
    styleResolver.style()->setTextIndentType(styleResolver.parentStyle()->textIndentType());
#endif
}

inline void applyInitialTextIndent(StyleResolver& styleResolver)
{
    styleResolver.style()->setTextIndent(RenderStyle::initialTextIndent());
#if ENABLE(CSS3_TEXT)
    styleResolver.style()->setTextIndentLine(RenderStyle::initialTextIndentLine());
    styleResolver.style()->setTextIndentType(RenderStyle::initialTextIndentType());
#endif
}

inline void applyValueTextIndent(StyleResolver& styleResolver, CSSValue& value)
{
    Length lengthOrPercentageValue;
#if ENABLE(CSS3_TEXT)
    TextIndentLine textIndentLineValue = RenderStyle::initialTextIndentLine();
    TextIndentType textIndentTypeValue = RenderStyle::initialTextIndentType();
#endif
    for (auto& item : downcast<CSSValueList>(value)) {
        auto& primitiveValue = downcast<CSSPrimitiveValue>(item.get());
        if (!primitiveValue.getValueID())
            lengthOrPercentageValue = primitiveValue.convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion>(styleResolver.state().cssToLengthConversionData());
#if ENABLE(CSS3_TEXT)
        else if (primitiveValue.getValueID() == CSSValueWebkitEachLine)
            textIndentLineValue = TextIndentEachLine;
        else if (primitiveValue.getValueID() == CSSValueWebkitHanging)
            textIndentTypeValue = TextIndentHanging;
#endif
    }

    ASSERT(!lengthOrPercentageValue.isUndefined());
    styleResolver.style()->setTextIndent(lengthOrPercentageValue);
#if ENABLE(CSS3_TEXT)
    styleResolver.style()->setTextIndentLine(textIndentLineValue);
    styleResolver.style()->setTextIndentType(textIndentTypeValue);
#endif
}

} // namespace StyleBuilderFunctions

} // namespace WebCore

#endif // StyleBuilderCustom_h
