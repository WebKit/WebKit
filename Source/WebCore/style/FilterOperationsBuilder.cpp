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

#include "CSSFilterFunctionDescriptor.h"
#include "CSSFunctionValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSShadowValue.h"
#include "CSSToLengthConversionData.h"
#include "CalculationValue.h"
#include "ColorFromPrimitiveValue.h"
#include "Document.h"
#include "RenderStyle.h"

namespace WebCore {
namespace Style {

template<CSSValueID function> static double processNumberOrPercentFilterParameter(const CSSFunctionValue& filter, const CSSToLengthConversionData& conversionData)
{
    if (auto* parameter = filter.item(0)) {
        auto& primitive = downcast<CSSPrimitiveValue>(*parameter);
        if (primitive.isPercentage()) {
            auto amount = primitive.resolveAsPercentage(conversionData) / 100.0;
            if constexpr (!filterFunctionAllowsValuesGreaterThanOne<function>())
                amount = std::min<double>(amount, 1);
            return amount;
        }

        ASSERT(primitive.isNumber());
        auto amount = primitive.resolveAsNumber(conversionData);
        if constexpr (!filterFunctionAllowsValuesGreaterThanOne<function>())
            amount = std::min<double>(amount, 1);
        return amount;
    }

    return filterFunctionDefaultValue<function>().value;
}

static Ref<FilterOperation> createFilterFunctionBlur(const CSSFunctionValue& filter, const CSSToLengthConversionData& conversionData)
{
    // blur() = blur( <length>? )

    constexpr CSSValueID function = CSSValueBlur;

    Length stdDeviation;
    if (auto* parameter = filter.item(0))
        stdDeviation = downcast<CSSPrimitiveValue>(*parameter).convertToLength<FixedFloatConversion | PercentConversion | CalculatedConversion>(conversionData);
    else
        stdDeviation = Length(filterFunctionDefaultValue<function>().value, LengthType::Fixed);

    return BlurFilterOperation::create(stdDeviation);
}

static Ref<FilterOperation> createFilterFunctionBrightness(const CSSFunctionValue& filter, const CSSToLengthConversionData& conversionData)
{
    // brightness() = brightness( [ <number> |  <percentage> ]? )

    constexpr CSSValueID function = CSSValueBrightness;

    return BasicComponentTransferFilterOperation::create(
        processNumberOrPercentFilterParameter<function>(filter, conversionData),
        filterFunctionOperationType<function>()
    );
}

static Ref<FilterOperation> createFilterFunctionContrast(const CSSFunctionValue& filter, const CSSToLengthConversionData& conversionData)
{
    // contrast() = contrast( [ <number> |  <percentage> ]? )

    constexpr CSSValueID function = CSSValueContrast;

    return BasicComponentTransferFilterOperation::create(
        processNumberOrPercentFilterParameter<function>(filter, conversionData),
        filterFunctionOperationType<function>()
    );
}

static Ref<FilterOperation> createFilterFunctionDropShadow(const CSSFunctionValue& filter, const CSSToLengthConversionData& conversionData, const Function<Color(const CSSPrimitiveValue*)>& resolveFilterColor)
{
    // drop-shadow() = drop-shadow( [ <color>? && <length>{2,3} ] )
    // NOTE: The drop shadow parameters are all stored as a single CSSShadowValue.

    ASSERT(filter.length() == 1);

    const auto& shadow = downcast<CSSShadowValue>(*filter.item(0));

    int x = shadow.x->resolveAsLength<int>(conversionData);
    int y = shadow.y->resolveAsLength<int>(conversionData);
    int blur = shadow.blur ? shadow.blur->resolveAsLength<int>(conversionData) : 0;
    auto color = resolveFilterColor(shadow.color.get());

    return DropShadowFilterOperation::create(IntPoint(x, y), blur, color.isValid() ? color : Color::transparentBlack);
}

static Ref<FilterOperation> createFilterFunctionGrayscale(const CSSFunctionValue& filter, const CSSToLengthConversionData& conversionData)
{
    // grayscale() = grayscale( [ <number> |  <percentage> ]? )

    constexpr CSSValueID function = CSSValueGrayscale;

    return BasicColorMatrixFilterOperation::create(
        processNumberOrPercentFilterParameter<function>(filter, conversionData),
        filterFunctionOperationType<function>()
    );
}

static Ref<FilterOperation> createFilterFunctionHueRotate(const CSSFunctionValue& filter, const CSSToLengthConversionData& conversionData)
{
    // hue-rotate() = hue-rotate( [ <angle> | <zero> ]? )

    constexpr CSSValueID function = CSSValueHueRotate;

    double angle = 0;
    if (auto* parameter = filter.item(0))
        angle = downcast<CSSPrimitiveValue>(*parameter).resolveAsAngle(conversionData);
    else
        angle = filterFunctionDefaultValue<function>().value;

    return BasicColorMatrixFilterOperation::create(
        angle,
        filterFunctionOperationType<function>()
    );
}

static Ref<FilterOperation> createFilterFunctionInvert(const CSSFunctionValue& filter, const CSSToLengthConversionData& conversionData)
{
    // invert() = invert( [ <number> |  <percentage> ]? )

    constexpr CSSValueID function = CSSValueInvert;

    return BasicComponentTransferFilterOperation::create(
        processNumberOrPercentFilterParameter<function>(filter, conversionData),
        filterFunctionOperationType<function>()
    );
}

static Ref<FilterOperation> createFilterFunctionOpacity(const CSSFunctionValue& filter, const CSSToLengthConversionData& conversionData)
{
    // opacity() = opacity( [ <number> |  <percentage> ]? )

    constexpr CSSValueID function = CSSValueOpacity;

    return BasicComponentTransferFilterOperation::create(
        processNumberOrPercentFilterParameter<function>(filter, conversionData),
        filterFunctionOperationType<function>()
    );
}

static Ref<FilterOperation> createFilterFunctionSaturate(const CSSFunctionValue& filter, const CSSToLengthConversionData& conversionData)
{
    // saturate() = saturate( [ <number> |  <percentage> ]? )

    constexpr CSSValueID function = CSSValueSaturate;

    return BasicColorMatrixFilterOperation::create(
        processNumberOrPercentFilterParameter<function>(filter, conversionData),
        filterFunctionOperationType<function>()
    );
}

static Ref<FilterOperation> createFilterFunctionSepia(const CSSFunctionValue& filter, const CSSToLengthConversionData& conversionData)
{
    // sepia() = sepia( [ <number> |  <percentage> ]? )

    constexpr CSSValueID function = CSSValueSepia;

    return BasicColorMatrixFilterOperation::create(
        processNumberOrPercentFilterParameter<function>(filter, conversionData),
        filterFunctionOperationType<function>()
    );
}

static Ref<FilterOperation> createFilterFunctionAppleInvertLightness(const CSSFunctionValue&, const CSSToLengthConversionData&)
{
    // <-apple-invert-lightness()> = -apple-invert-lightness()

    return InvertLightnessFilterOperation::create();
}

static Ref<FilterOperation> createFilterFunctionReference(const CSSPrimitiveValue& primitive, const Document& document)
{
    ASSERT(primitive.isURI());

    auto filterURL = primitive.stringValue();
    auto fragment = document.completeURL(filterURL).fragmentIdentifier().toAtomString();

    return ReferenceFilterOperation::create(filterURL, WTFMove(fragment));
}

static Ref<FilterOperation> createFilterOperation(const CSSValue& value, const CSSToLengthConversionData& conversionData, const Document& document, const Function<Color(const CSSPrimitiveValue*)>& resolveFilterColor)
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value))
        return createFilterFunctionReference(*primitiveValue, document);

    auto& filter = downcast<CSSFunctionValue>(value);

    switch (filter.name()) {
    case CSSValueBlur:
        return createFilterFunctionBlur(filter, conversionData);
    case CSSValueBrightness:
        return createFilterFunctionBrightness(filter, conversionData);
    case CSSValueContrast:
        return createFilterFunctionContrast(filter, conversionData);
    case CSSValueDropShadow:
        return createFilterFunctionDropShadow(filter, conversionData, resolveFilterColor);
    case CSSValueGrayscale:
        return createFilterFunctionGrayscale(filter, conversionData);
    case CSSValueHueRotate:
        return createFilterFunctionHueRotate(filter, conversionData);
    case CSSValueInvert:
        return createFilterFunctionInvert(filter, conversionData);
    case CSSValueOpacity:
        return createFilterFunctionOpacity(filter, conversionData);
    case CSSValueSaturate:
        return createFilterFunctionSaturate(filter, conversionData);
    case CSSValueSepia:
        return createFilterFunctionSepia(filter, conversionData);
    case CSSValueAppleInvertLightness:
        return createFilterFunctionAppleInvertLightness(filter, conversionData);
    default:
        break;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static FilterOperations createFilterOperations(const CSSValue& value, const CSSToLengthConversionData& conversionData, const Document& document, const Function<Color(const CSSPrimitiveValue*)>& resolveFilterColor)
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        ASSERT_UNUSED(primitiveValue, primitiveValue->valueID() == CSSValueNone);
        return FilterOperations { };
    }

    return FilterOperations { WTF::map(downcast<CSSValueList>(value), [&](const auto& value) {
        return createFilterOperation(value, conversionData, document, resolveFilterColor);
    }) };
}

FilterOperations createFilterOperations(const CSSValue& inValue, const CSSToLengthConversionData& conversionData, const Document& document, RenderStyle& style)
{
    return createFilterOperations(inValue, conversionData, document, [&](const auto* primitiveValue) -> Color {
        return primitiveValue ? colorFromPrimitiveValueWithResolvedCurrentColor(document, style, conversionData, *primitiveValue) : style.color();
    });
}

FilterOperations createFilterOperations(const CSSValue& inValue, const CSSParserContext& context, AllowedColorTypes allowedColorTypes, const CSSToLengthConversionData& conversionData, const Document& document)
{
    auto options = CSSPropertyParserHelpers::CSSColorParsingOptions {
        .allowedColorTypes = allowedColorTypes
    };

    auto resolutionState = CSSUnresolvedColorResolutionState {
        .resolvedCurrentColor = Color::black
    };

    return createFilterOperations(inValue, conversionData, document, [&](const auto* primitiveValue) -> Color {
        return primitiveValue ? CSSPropertyParserHelpers::parseColorRawSlow(primitiveValue->cssText(), context, options, resolutionState) : Color::black;
    });
}

} // namespace Style
} // namespace WebCore
