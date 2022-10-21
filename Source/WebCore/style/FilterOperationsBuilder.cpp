/*
 * Copyright (C) 2022 Apple Inc.  All rights reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FilterOperationsBuilder.h"

#include "CSSFunctionValue.h"
#include "CSSShadowValue.h"
#include "CSSToLengthConversionData.h"
#include "ColorFromPrimitiveValue.h"
#include "Document.h"
#include "TransformFunctions.h"

namespace WebCore {

namespace Style {

static FilterOperation::OperationType filterOperationForType(CSSValueID type)
{
    switch (type) {
    case CSSValueUrl:
        return FilterOperation::REFERENCE;
    case CSSValueGrayscale:
        return FilterOperation::GRAYSCALE;
    case CSSValueSepia:
        return FilterOperation::SEPIA;
    case CSSValueSaturate:
        return FilterOperation::SATURATE;
    case CSSValueHueRotate:
        return FilterOperation::HUE_ROTATE;
    case CSSValueInvert:
        return FilterOperation::INVERT;
    case CSSValueAppleInvertLightness:
        return FilterOperation::APPLE_INVERT_LIGHTNESS;
    case CSSValueOpacity:
        return FilterOperation::OPACITY;
    case CSSValueBrightness:
        return FilterOperation::BRIGHTNESS;
    case CSSValueContrast:
        return FilterOperation::CONTRAST;
    case CSSValueBlur:
        return FilterOperation::BLUR;
    case CSSValueDropShadow:
        return FilterOperation::DROP_SHADOW;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return FilterOperation::NONE;
}

std::optional<FilterOperations> createFilterOperations(const Document& document, RenderStyle& style, const CSSToLengthConversionData& cssToLengthConversionData, const CSSValue& inValue)
{
    FilterOperations operations;

    if (is<CSSPrimitiveValue>(inValue)) {
        auto& primitiveValue = downcast<CSSPrimitiveValue>(inValue);
        if (primitiveValue.valueID() == CSSValueNone)
            return operations;
    }

    if (!is<CSSValueList>(inValue))
        return std::nullopt;

    for (auto& currentValue : downcast<CSSValueList>(inValue)) {
        if (is<CSSPrimitiveValue>(currentValue)) {
            auto& primitiveValue = downcast<CSSPrimitiveValue>(currentValue.get());
            if (!primitiveValue.isURI())
                continue;

            auto filterURL = primitiveValue.stringValue();
            auto fragment = document.completeURL(filterURL).fragmentIdentifier().toAtomString();
            operations.operations().append(ReferenceFilterOperation::create(filterURL, WTFMove(fragment)));
            continue;
        }

        if (!is<CSSFunctionValue>(currentValue))
            continue;

        auto& filterValue = downcast<CSSFunctionValue>(currentValue.get());
        FilterOperation::OperationType operationType = filterOperationForType(filterValue.name());

        // Check that all parameters are primitive values, with the
        // exception of drop shadow which has a CSSShadowValue parameter.
        const CSSPrimitiveValue* firstValue = nullptr;
        if (operationType != FilterOperation::DROP_SHADOW) {
            bool haveNonPrimitiveValue = false;
            for (unsigned j = 0; j < filterValue.length(); ++j) {
                if (!is<CSSPrimitiveValue>(*filterValue.itemWithoutBoundsCheck(j))) {
                    haveNonPrimitiveValue = true;
                    break;
                }
            }
            if (haveNonPrimitiveValue)
                continue;
            if (filterValue.length())
                firstValue = downcast<CSSPrimitiveValue>(filterValue.itemWithoutBoundsCheck(0));
        }

        switch (operationType) {
        case FilterOperation::GRAYSCALE:
        case FilterOperation::SEPIA:
        case FilterOperation::SATURATE: {
            double amount = 1;
            if (filterValue.length() == 1) {
                amount = firstValue->doubleValue();
                if (firstValue->isPercentage())
                    amount /= 100;
            }

            operations.operations().append(BasicColorMatrixFilterOperation::create(amount, operationType));
            break;
        }
        case FilterOperation::HUE_ROTATE: {
            double angle = 0;
            if (filterValue.length() == 1)
                angle = firstValue->computeDegrees();

            operations.operations().append(BasicColorMatrixFilterOperation::create(angle, operationType));
            break;
        }
        case FilterOperation::INVERT:
        case FilterOperation::BRIGHTNESS:
        case FilterOperation::CONTRAST:
        case FilterOperation::OPACITY: {
            double amount = 1;
            if (filterValue.length() == 1) {
                amount = firstValue->doubleValue();
                if (firstValue->isPercentage())
                    amount /= 100;
            }

            operations.operations().append(BasicComponentTransferFilterOperation::create(amount, operationType));
            break;
        }
        case FilterOperation::APPLE_INVERT_LIGHTNESS: {
            operations.operations().append(InvertLightnessFilterOperation::create());
            break;
        }
        case FilterOperation::BLUR: {
            Length stdDeviation = Length(0, LengthType::Fixed);
            if (filterValue.length() >= 1)
                stdDeviation = convertToFloatLength(firstValue, cssToLengthConversionData);
            if (stdDeviation.isUndefined())
                return std::nullopt;

            operations.operations().append(BlurFilterOperation::create(stdDeviation));
            break;
        }
        case FilterOperation::DROP_SHADOW: {
            if (filterValue.length() != 1)
                return std::nullopt;

            const auto* cssValue = filterValue.itemWithoutBoundsCheck(0);
            if (!is<CSSShadowValue>(cssValue))
                continue;

            const auto& item = downcast<CSSShadowValue>(*cssValue);
            int x = item.x->computeLength<int>(cssToLengthConversionData);
            int y = item.y->computeLength<int>(cssToLengthConversionData);
            IntPoint location(x, y);
            int blur = item.blur ? item.blur->computeLength<int>(cssToLengthConversionData) : 0;
            auto color = item.color ? colorFromPrimitiveValueWithResolvedCurrentColor(document, style, *item.color) : style.color();

            operations.operations().append(DropShadowFilterOperation::create(location, blur, color.isValid() ? color : Color::transparentBlack));
            break;
        }
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    return operations;
}

} // namespace Style

} // namespace WebCore
