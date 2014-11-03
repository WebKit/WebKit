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
        RefPtr<ShapeValue> shape = ShapeValue::createImageValue(styleResolver.styleImage(CSSPropertyWebkitShapeOutside, &value));
        styleResolver.style()->setShapeOutside(shape.release());
    } else if (is<CSSValueList>(value)) {
        RefPtr<BasicShape> shape;
        CSSBoxType referenceBox = BoxMissing;
        auto& valueList = downcast<CSSValueList>(value);
        for (unsigned i = 0; i < valueList.length(); ++i) {
            CSSPrimitiveValue& primitiveValue = downcast<CSSPrimitiveValue>(*valueList.itemWithoutBoundsCheck(i));
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

} // namespace StyleBuilderFunctions

} // namespace WebCore

#endif // StyleBuilderCustom_h
