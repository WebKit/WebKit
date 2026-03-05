/*
 * Copyright (C) 2023-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSPropertyInitialValues.h"
#include "CSSPropertyInitialValuesGeneratedInlines.h"

#include "CSSBorderImageSliceValue.h"
#include "CSSBorderImageWidthValue.h"
#include "CSSOffsetRotateValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyNames.h"
#include "CSSQuadValue.h"
#include "CSSUnits.h"
#include "CSSValueKeywords.h"
#include "CSSValuePair.h"
#include "RectBase.h"
#include <wtf/Variant.h>
#include <wtf/text/ASCIILiteral.h>

namespace WebCore {

static bool NODELETE isValueIDPair(const CSSValue& value, CSSValueID valueID)
{
    return value.isPair() && isValueID(value.first(), valueID) && isValueID(value.second(), valueID);
}

static bool isNumber(const CSSPrimitiveValue& value, double number, CSSUnitType type)
{
    return value.primitiveType() == type && !value.isCalculated() && value.valueNoConversionDataRequired<double>() == number;
}

static bool isNumber(const CSSPrimitiveValue* value, double number, CSSUnitType type)
{
    return value && isNumber(*value, number, type);
}

static bool isNumber(const CSSValue& value, double number, CSSUnitType type)
{
    return isNumber(dynamicDowncast<CSSPrimitiveValue>(value), number, type);
}

static bool isNumber(const RectBase& quad, double number, CSSUnitType type)
{
    return isNumber(quad.top(), number, type)
        && isNumber(quad.right(), number, type)
        && isNumber(quad.bottom(), number, type)
        && isNumber(quad.left(), number, type);
}

static bool NODELETE isValueID(const RectBase& quad, CSSValueID valueID)
{
    return isValueID(quad.top(), valueID)
        && isValueID(quad.right(), valueID)
        && isValueID(quad.bottom(), valueID)
        && isValueID(quad.left(), valueID);
}

static bool isNumericQuad(const CSSValue& value, double number, CSSUnitType type)
{
    return value.isQuad() && isNumber(value.quad(), number, type);
}

bool isInitialValueForLonghand(CSSPropertyID longhand, const CSSValue& value)
{
    if (value.isImplicitInitialValue())
        return true;
    switch (longhand) {
    case CSSPropertyBackgroundSize:
    case CSSPropertyMaskSize:
        if (isValueIDPair(value, CSSValueAuto))
            return true;
        break;
    case CSSPropertyBorderImageOutset:
    case CSSPropertyMaskBorderOutset:
        if (isNumericQuad(value, 0, CSSUnitType::CSS_NUMBER))
            return true;
        break;
    case CSSPropertyBorderImageRepeat:
    case CSSPropertyMaskBorderRepeat:
        if (isValueIDPair(value, CSSValueStretch))
            return true;
        break;
    case CSSPropertyBorderImageSlice:
        if (auto sliceValue = dynamicDowncast<CSSBorderImageSliceValue>(value)) {
            if (!sliceValue->fill() && isNumber(sliceValue->slices(), 100, CSSUnitType::CSS_PERCENTAGE))
                return true;
        }
        break;
    case CSSPropertyBorderImageWidth:
        if (auto widthValue = dynamicDowncast<CSSBorderImageWidthValue>(value)) {
            if (!widthValue->overridesBorderWidths() && isNumber(widthValue->widths(), 1, CSSUnitType::CSS_NUMBER))
                return true;
        }
        break;
    case CSSPropertyOffsetRotate:
        if (auto rotateValue = dynamicDowncast<CSSOffsetRotateValue>(value)) {
            if (rotateValue->isInitialValue())
                return true;
        }
        break;
    case CSSPropertyMaskBorderSlice:
        if (auto sliceValue = dynamicDowncast<CSSBorderImageSliceValue>(value)) {
            if (!sliceValue->fill() && isNumber(sliceValue->slices(), 0, CSSUnitType::CSS_NUMBER))
                return true;
        }
        return false;
    case CSSPropertyMaskBorderWidth:
        if (auto widthValue = dynamicDowncast<CSSBorderImageWidthValue>(value)) {
            if (!widthValue->overridesBorderWidths() && isValueID(widthValue->widths(), CSSValueAuto))
                return true;
        }
        break;
    default:
        break;
    }
    return WTF::switchOn(initialValueForLonghand(longhand),
        [&](CSSValueID initialValue) {
            return isValueID(value, initialValue);
        },
        [&](InitialNumericValue initialValue) {
            return isNumber(value, initialValue.number, initialValue.type);
        }
    );
}

ASCIILiteral initialValueTextForLonghand(CSSPropertyID longhand)
{
    return WTF::switchOn(initialValueForLonghand(longhand),
        [](CSSValueID value) {
            return nameLiteral(value);
        },
        [](InitialNumericValue initialValue) {
            switch (initialValue.type) {
            case CSSUnitType::CSS_NUMBER:
                if (initialValue.number == 0.0)
                    return "0"_s;
                if (initialValue.number == 1.0)
                    return "1"_s;
                if (initialValue.number == 2.0)
                    return "2"_s;
                if (initialValue.number == 4.0)
                    return "4"_s;
                if (initialValue.number == 8.0)
                    return "8"_s;
                break;
            case CSSUnitType::CSS_PERCENTAGE:
                if (initialValue.number == 0.0)
                    return "0%"_s;
                if (initialValue.number == 50.0)
                    return "50%"_s;
                if (initialValue.number == 100.0)
                    return "100%"_s;
                break;
            case CSSUnitType::CSS_PX:
                if (initialValue.number == 0.0)
                    return "0px"_s;
                if (initialValue.number == 1.0)
                    return "1px"_s;
                break;
            case CSSUnitType::CSS_S:
                if (initialValue.number == 0.0)
                    return "0s"_s;
                break;
            default:
                break;
            }
            ASSERT_NOT_REACHED();
            return ""_s;
        }
    );
}

CSSValueID initialValueIDForLonghand(CSSPropertyID longhand)
{
    return WTF::switchOn(initialValueForLonghand(longhand),
        [](CSSValueID value) {
            return value;
        },
        [](InitialNumericValue) {
            return CSSValueInvalid;
        }
    );
}

} // namespace WebCore
