/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#pragma once

#include "CSSColor.h"
#include "CSSColorDescriptors.h"
#include "CSSPlatformColorResolutionState.h"
#include "CSSPrimitiveNumericTypes+EvaluateCalc.h"
#include "CSSRelativeColorResolver.h"
#include "ColorSerialization.h"
#include <wtf/Forward.h>

namespace WebCore {
namespace CSS {

template<typename Descriptor, unsigned Index>
using RelativeColorComponent = GetCSSColorParseTypeWithCalcAndSymbolsComponentResult<Descriptor, Index>;

template<typename D>
struct RelativeColor {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(RelativeColor);

    using Descriptor = D;

    Color origin;
    CSSColorParseTypeWithCalcAndSymbols<Descriptor> components;

    bool operator==(const RelativeColor<Descriptor>&) const = default;
};

template<typename Descriptor>
auto simplifyUnevaluatedCalc(const CSSColorParseTypeWithCalcAndSymbols<Descriptor>& components, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) -> CSSColorParseTypeWithCalcAndSymbols<Descriptor>
{
    return CSSColorParseTypeWithCalcAndSymbols<Descriptor> {
        simplifyUnevaluatedCalc(std::get<0>(components), conversionData, symbolTable),
        simplifyUnevaluatedCalc(std::get<1>(components), conversionData, symbolTable),
        simplifyUnevaluatedCalc(std::get<2>(components), conversionData, symbolTable),
        simplifyUnevaluatedCalc(std::get<3>(components), conversionData, symbolTable)
    };
}

template<typename Descriptor>
WebCore::Color createColor(const RelativeColor<Descriptor>& unresolved, PlatformColorResolutionState& state)
{
    PlatformColorResolutionStateNester nester { state };

    auto origin = createColor(unresolved.origin, state);
    if (!origin.isValid())
        return { };

    auto resolver = RelativeColorResolver<Descriptor> {
        .origin = WTFMove(origin),
        .components = unresolved.components
    };

    if (state.conversionData)
        return resolve(WTFMove(resolver), *state.conversionData);

    if (!componentsRequireConversionData<Descriptor>(resolver.components))
        return resolveNoConversionDataRequired(WTFMove(resolver));

    return { };
}

template<typename Descriptor>
bool containsColorSchemeDependentColor(const RelativeColor<Descriptor>& unresolved)
{
    return containsColorSchemeDependentColor(unresolved.origin);
}

template<typename Descriptor>
bool containsCurrentColor(const RelativeColor<Descriptor>& unresolved)
{
    return containsColorSchemeDependentColor(unresolved.origin);
}

template<typename D> struct Serialize<RelativeColor<D>> {
    void operator()(StringBuilder& builder, const SerializationContext& context, const RelativeColor<D>& value)
    {
        if constexpr (D::usesColorFunctionForSerialization) {
            builder.append("color(from "_s);
            serializationForCSS(builder, context, value.origin);
            builder.append(' ', serialization(ColorSpaceFor<typename D::ColorType>));
        } else {
            builder.append(D::serializationFunctionName, "(from "_s);
            serializationForCSS(builder, context, value.origin);
        }

        auto [c1, c2, c3, alpha] = value.components;

        builder.append(' ');
        serializationForCSS(builder, context, c1);
        builder.append(' ');
        serializationForCSS(builder, context, c2);
        builder.append(' ');
        serializationForCSS(builder, context, c3);

        if (alpha) {
            builder.append(" / "_s);
            serializationForCSS(builder, context, *alpha);
        }

        builder.append(')');
    }
};

template<typename D> struct ComputedStyleDependenciesCollector<RelativeColor<D>> {
    void operator()(ComputedStyleDependencies& dependencies, const RelativeColor<D>& value)
    {
        collectComputedStyleDependencies(dependencies, value.origin);
        collectComputedStyleDependencies(dependencies, std::get<0>(value.components));
        collectComputedStyleDependencies(dependencies, std::get<1>(value.components));
        collectComputedStyleDependencies(dependencies, std::get<2>(value.components));
        collectComputedStyleDependencies(dependencies, std::get<3>(value.components));
    }
};

template<typename D> struct CSSValueChildrenVisitor<RelativeColor<D>> {
    IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>& func, const RelativeColor<D>& value)
    {
        if (visitCSSValueChildren(func, value.origin) == IterationStatus::Done)
            return IterationStatus::Done;
        if (visitCSSValueChildren(func, std::get<0>(value.components)) == IterationStatus::Done)
            return IterationStatus::Done;
        if (visitCSSValueChildren(func, std::get<1>(value.components)) == IterationStatus::Done)
            return IterationStatus::Done;
        if (visitCSSValueChildren(func, std::get<2>(value.components)) == IterationStatus::Done)
            return IterationStatus::Done;
        if (visitCSSValueChildren(func, std::get<3>(value.components)) == IterationStatus::Done)
            return IterationStatus::Done;
        return IterationStatus::Continue;
    }
};

} // namespace CSS
} // namespace WebCore
