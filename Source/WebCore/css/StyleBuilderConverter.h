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

#include "CSSCalculationValue.h"
#include "CSSPrimitiveValue.h"
#include "Length.h"
#include "StyleResolver.h"

namespace WebCore {

class StyleBuilderConverter {
public:
    static Length convertLength(StyleResolver&, CSSValue&);
    static Length convertLengthOrAuto(StyleResolver&, CSSValue&);
    static Length convertLengthSizing(StyleResolver&, CSSValue&);
    static Length convertLengthMaxSizing(StyleResolver&, CSSValue&);
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

} // namespace WebCore

#endif // StyleBuilderConverter_h
