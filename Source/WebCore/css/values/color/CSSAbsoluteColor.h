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

#include "CSSAbsoluteColorResolver.h"
#include "CSSColorDescriptors.h"
#include "CSSPlatformColorResolutionState.h"
#include "CSSValueTypes.h"
#include "ColorSerialization.h"

namespace WebCore {

class Color;

namespace CSS {

template<typename D, unsigned Index> using AbsoluteColorComponent = GetCSSColorParseTypeWithCalcComponentResult<D, Index>;

template<typename D>
struct AbsoluteColor {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(AbsoluteColor);

    using Descriptor = D;

    CSSColorParseTypeWithCalc<Descriptor> components;

    bool operator==(const AbsoluteColor<Descriptor>&) const = default;
};

template<typename D> WebCore::Color createColor(const AbsoluteColor<D>& unresolved, PlatformColorResolutionState& state)
{
    PlatformColorResolutionStateNester nester { state };

    auto resolver = AbsoluteColorResolver<D> {
        .components = unresolved.components,
        .nestingLevel = state.nestingLevel
    };

    if (state.conversionData)
        return resolve(WTFMove(resolver), *state.conversionData);

    if (!componentsRequireConversionData<D>(resolver.components))
        return resolveNoConversionDataRequired(WTFMove(resolver));

    return { };
}

template<typename D> constexpr bool containsColorSchemeDependentColor(const AbsoluteColor<D>&)
{
    return false;
}

template<typename D> constexpr bool containsCurrentColor(const AbsoluteColor<D>&)
{
    return false;
}

template<typename D> struct Serialize<AbsoluteColor<D>> {
    void operator()(StringBuilder& builder, const SerializationContext& context, const AbsoluteColor<D>& value)
    {
        if constexpr (D::usesColorFunctionForSerialization)
            builder.append("color("_s, serialization(ColorSpaceFor<typename D::ColorType>), ' ');
        else
            builder.append(D::serializationFunctionName, '(');

        auto [c1, c2, c3, alpha] = value.components;

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

template<typename D> struct ComputedStyleDependenciesCollector<AbsoluteColor<D>> {
    void operator()(ComputedStyleDependencies& dependencies, const AbsoluteColor<D>& value)
    {
        collectComputedStyleDependencies(dependencies, std::get<0>(value.components));
        collectComputedStyleDependencies(dependencies, std::get<1>(value.components));
        collectComputedStyleDependencies(dependencies, std::get<2>(value.components));
        collectComputedStyleDependencies(dependencies, std::get<3>(value.components));
    }
};

template<typename D> struct CSSValueChildrenVisitor<AbsoluteColor<D>> {
    IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>& func, const AbsoluteColor<D>& value)
    {
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
