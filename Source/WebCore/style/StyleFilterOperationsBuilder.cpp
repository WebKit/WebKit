/*
 * Copyright (C) 2022 Apple Inc.  All rights reserved.
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
#include "StyleFilterOperationsBuilder.h"

#include "CSSFilterFunctionDescriptor.h"
#include "CSSFunctionValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSShadowValue.h"
#include "CSSUnresolvedFilterResolutionContext.h"
#include "CalculationValue.h"
#include "Document.h"
#include "FilterOperations.h"
#include "LengthPoint.h"
#include "StyleBuilderState.h"

namespace WebCore {
namespace Style {

template<typename T> static T createStyleFilterOperationZeroArguments()
{
    return { };
}

template<CSSValueID functionID> static Length resolveSingleArgumentLength(const CSSValue* value, BuilderState& state)
{
    if (auto primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value))
        return primitiveValue->convertToLength<FixedFloatConversion | CalculatedConversion>(state.cssToLengthConversionData());
    return { filterFunctionDefaultValue<functionID>().value, LengthType::Fixed };
}

template<typename T> static T createStyleFilterOperationSingleArgumentLength(const CSSFunctionValue& value, BuilderState& state)
{
    return { resolveSingleArgumentLength<T::functionID>(value.item(0), state) };
}

template<CSSValueID functionID> static NumberRaw resolveSingleArgumentPercentageOrNumber(const CSSValue* value)
{
    if (auto primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value))
        return { primitiveValue->doubleValueDividingBy100IfPercentage() };
    return filterFunctionDefaultValue<functionID>();
}

template<typename T> static T createStyleFilterOperationSingleArgumentPercentageOrNumber(const CSSFunctionValue& value)
{
    return { resolveSingleArgumentPercentageOrNumber<T::functionID>(value.item(0)) };
}

template<CSSValueID functionID> static CanonicalAngleRaw resolveSingleArgumentAngle(const CSSValue* value)
{
    if (auto primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value))
        return { primitiveValue->computeDegrees() };
    return filterFunctionDefaultValue<functionID>();
}

template<typename T> static T createStyleFilterOperationSingleArgumentAngle(const CSSFunctionValue& value)
{
    return { resolveSingleArgumentAngle<T::functionID>(value.item(0)) };
}

static FilterOperations::Reference createStyleFilterOperationReference(const CSSPrimitiveValue& primitiveValue, BuilderState& state)
{
    ASSERT(primitiveValue.isURI());

    auto filterURL = primitiveValue.stringValue();
    auto fragment = state.document().completeURL(filterURL).fragmentIdentifier().toAtomString();
    return { WTFMove(filterURL), WTFMove(fragment) };
}

static FilterOperations::DropShadow createStyleFilterOperationDropShadow(const CSSFunctionValue& value, BuilderState& state)
{
    using Descriptor = CSSFilterFunctionDescriptor<CSSValueDropShadow>;

    auto resolvePoint = [](const CSSPrimitiveValue& primitive, BuilderState& state) -> Length {
        return primitive.convertToLength<FixedFloatConversion | CalculatedConversion>(state.cssToLengthConversionData());
    };

    auto resolveStdDeviation = [](const RefPtr<CSSPrimitiveValue>& primitive, BuilderState& state) -> Length {
        if (primitive)
            return primitive->convertToLength<FixedFloatConversion | CalculatedConversion>(state.cssToLengthConversionData());
        return { Descriptor::defaultStdDeviationValue.value, LengthType::Fixed };
    };

    auto resolveColor = [](const RefPtr<CSSPrimitiveValue>& primitive, BuilderState& state) -> StyleColor {
        if (primitive)
            return state.colorFromPrimitiveValue(*primitive);
        auto defaultColor = Descriptor::defaultColorValue;
        return { WTFMove(defaultColor) };
    };

    Ref shadow = downcast<CSSShadowValue>(value[0]);

    return {
        { resolvePoint(*shadow->x, state), resolvePoint(*shadow->y, state) },
        resolveStdDeviation(shadow->blur, state),
        resolveColor(shadow->color, state)
    };
}

template<typename Function> static decltype(auto) callWithFilterOperation(CSSValueID id, Function&& function)
{
    switch (id) {
    case CSSValueBlur:
        return function.template operator()<FilterOperations::Blur>();
    case CSSValueHueRotate:
        return function.template operator()<FilterOperations::HueRotate>();
    case CSSValueGrayscale:
        return function.template operator()<FilterOperations::Grayscale>();
    case CSSValueSepia:
        return function.template operator()<FilterOperations::Sepia>();
    case CSSValueSaturate:
        return function.template operator()<FilterOperations::Saturate>();
    case CSSValueBrightness:
        return function.template operator()<FilterOperations::Brightness>();
    case CSSValueInvert:
        return function.template operator()<FilterOperations::Invert>();
    case CSSValueContrast:
        return function.template operator()<FilterOperations::Contrast>();
    case CSSValueOpacity:
        return function.template operator()<FilterOperations::Opacity>();
    case CSSValueDropShadow:
        return function.template operator()<FilterOperations::DropShadow>();
    case CSSValueAppleInvertLightness:
        return function.template operator()<FilterOperations::AppleInvertLightness>();
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return function.template operator()<FilterOperations::Blur>();
}

FilterOperations createStyleFilterOperations(const CSSValue& value, BuilderState& state)
{
    if (isValueID(value, CSSValueNone))
        return { };

    Ref list = downcast<CSSValueList>(value);

    return { FilterOperations::Storage { list->size(), [&](size_t i) -> FilterOperations::FilterOperation {
        auto& currentValue = *list->itemWithoutBoundsCheck(i);

        if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(currentValue))
            return createStyleFilterOperationReference(*primitiveValue, state);

        auto& filterValue = downcast<CSSFunctionValue>(currentValue);
        return callWithFilterOperation(filterValue.name(),
            [&]<typename T>() -> FilterOperations::FilterOperation {
                if constexpr (std::same_as<T, FilterOperations::DropShadow>)
                    return createStyleFilterOperationDropShadow(filterValue, state);
                else if constexpr (std::same_as<T, FilterOperations::Blur>)
                    return createStyleFilterOperationSingleArgumentLength<T>(filterValue, state);
                else if constexpr (std::same_as<T, FilterOperations::HueRotate>)
                    return createStyleFilterOperationSingleArgumentAngle<T>(filterValue);
                else if constexpr (std::same_as<T, FilterOperations::AppleInvertLightness>)
                    return createStyleFilterOperationZeroArguments<T>();
                else
                    return createStyleFilterOperationSingleArgumentPercentageOrNumber<T>(filterValue);
            }
        );
    } } };
}

static WebCore::FilterOperations::FilterOperation createFilterOperationReference(const CSSPrimitiveValue& primitiveValue, const CSSUnresolvedFilterResolutionContext& context)
{
    ASSERT(primitiveValue.isURI());

    auto filterURL = primitiveValue.stringValue();
    auto fragment = context.url.completeURL(filterURL).fragmentIdentifier().toAtomString();
    return WebCore::FilterOperations::Reference { WTFMove(filterURL), WTFMove(fragment) };
}

static WebCore::FilterOperations::FilterOperation createFilterOperationBlur(const CSSFunctionValue& value, const CSSUnresolvedFilterResolutionContext& context)
{
    auto resolveLength = [](const CSSValue* value, const CSSUnresolvedFilterResolutionContext& context) -> Length {
        if (auto primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value))
            return context.length.resolveLength(*primitiveValue);
        return { filterFunctionDefaultValue<CSSValueBlur>().value, LengthType::Fixed };
    };

    return WebCore::FilterOperations::Blur { resolveLength(value.item(0), context) };
}

static WebCore::FilterOperations::FilterOperation createFilterOperationDropShadow(const CSSFunctionValue& value, const CSSUnresolvedFilterResolutionContext& context)
{
    using Descriptor = CSSFilterFunctionDescriptor<CSSValueDropShadow>;

    auto resolvePoint = [](const CSSPrimitiveValue& primitive, const CSSUnresolvedFilterResolutionContext& context) -> Length {
        return context.length.resolveLength(primitive);
    };

    auto resolveStdDeviation = [](const RefPtr<CSSPrimitiveValue>& primitive, const CSSUnresolvedFilterResolutionContext& context) -> Length {
        if (primitive)
            return context.length.resolveLength(*primitive);
        return { Descriptor::defaultStdDeviationValue.value, LengthType::Fixed };
    };

    auto resolveColor = [](const RefPtr<CSSPrimitiveValue>& primitive, const CSSUnresolvedFilterResolutionContext& context) -> Color {
        if (primitive)
            return context.color.resolveColor(*primitive);
        auto defaultColor = Descriptor::defaultColorValue;
        return context.color.resolveColor(StyleColor { WTFMove(defaultColor) });
    };

    Ref shadow = downcast<CSSShadowValue>(value[0]);

    return WebCore::FilterOperations::DropShadow {
        { resolvePoint(*shadow->x, context), resolvePoint(*shadow->y, context) },
        resolveStdDeviation(shadow->blur, context),
        resolveColor(shadow->color, context)
    };
}

WebCore::FilterOperations createFilterOperations(const CSSValue& value, const CSSUnresolvedFilterResolutionContext& context)
{
    if (isValueID(value, CSSValueNone))
        return { };

    Ref list = downcast<CSSValueList>(value);

    return { WebCore::FilterOperations::Storage { list->size(), [&](size_t i) -> WebCore::FilterOperations::FilterOperation {
        auto& currentValue = *list->itemWithoutBoundsCheck(i);

        if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(currentValue))
            return createFilterOperationReference(*primitiveValue, context);

        auto& filterValue = downcast<CSSFunctionValue>(currentValue);
        return callWithFilterOperation(filterValue.name(),
            [&]<typename T>() -> WebCore::FilterOperations::FilterOperation {
                if constexpr (std::same_as<T, FilterOperations::DropShadow>)
                    return createFilterOperationDropShadow(filterValue, context);
                else if constexpr (std::same_as<T, FilterOperations::Blur>)
                    return createFilterOperationBlur(filterValue, context);
                else if constexpr (std::same_as<T, FilterOperations::HueRotate>)
                    return FilterOperations::resolve(createStyleFilterOperationSingleArgumentAngle<T>(filterValue), context);
                else if constexpr (std::same_as<T, FilterOperations::AppleInvertLightness>)
                    return FilterOperations::resolve(createStyleFilterOperationZeroArguments<T>(), context);
                else
                    return FilterOperations::resolve(createStyleFilterOperationSingleArgumentPercentageOrNumber<T>(filterValue), context);
            }
        );
    } } };
}

} // namespace Style
} // namespace WebCore
