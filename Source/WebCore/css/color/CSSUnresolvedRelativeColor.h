/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "CSSColorDescriptors.h"
#include "CSSPrimitiveNumericTypes+EvaluateCalc.h"
#include "CSSRelativeColorResolver.h"
#include "CSSRelativeColorSerialization.h"
#include "CSSUnresolvedColorResolutionState.h"
#include "CSSUnresolvedStyleColorResolutionState.h"
#include "StyleColor.h"
#include "StyleRelativeColor.h"
#include <variant>
#include <wtf/Forward.h>

namespace WebCore {

class CSSUnresolvedColor;

template<typename Descriptor, unsigned Index>
using CSSUnresolvedRelativeColorComponent = GetCSSColorParseTypeWithCalcAndSymbolsComponentResult<Descriptor, Index>;

bool relativeColorOriginsEqual(const UniqueRef<CSSUnresolvedColor>&, const UniqueRef<CSSUnresolvedColor>&);

template<typename D>
struct CSSUnresolvedRelativeColor {
    using Descriptor = D;

    UniqueRef<CSSUnresolvedColor> origin;
    CSSColorParseTypeWithCalcAndSymbols<Descriptor> components;

    bool operator==(const CSSUnresolvedRelativeColor<Descriptor>& other) const
    {
        return relativeColorOriginsEqual(origin, other.origin)
            && components == other.components;
    }
};

template<typename Descriptor>
void serializationForCSS(StringBuilder& builder, const CSSUnresolvedRelativeColor<Descriptor>& unresolved)
{
    serializationForCSSRelativeColor(builder, unresolved);
}

template<typename Descriptor>
String serializationForCSS(const CSSUnresolvedRelativeColor<Descriptor>& unresolved)
{
    StringBuilder builder;
    serializationForCSS(builder, unresolved);
    return builder.toString();
}

template<typename Descriptor>
auto simplifyUnevaluatedCalc(const CSSColorParseTypeWithCalcAndSymbols<Descriptor>& components, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) -> CSSColorParseTypeWithCalcAndSymbols<Descriptor>
{
    return CSSColorParseTypeWithCalcAndSymbols<Descriptor> {
        CSS::simplifyUnevaluatedCalc(std::get<0>(components), conversionData, symbolTable),
        CSS::simplifyUnevaluatedCalc(std::get<1>(components), conversionData, symbolTable),
        CSS::simplifyUnevaluatedCalc(std::get<2>(components), conversionData, symbolTable),
        CSS::simplifyUnevaluatedCalc(std::get<3>(components), conversionData, symbolTable)
    };
}

template<typename Descriptor>
StyleColor createStyleColor(const CSSUnresolvedRelativeColor<Descriptor>& unresolved, CSSUnresolvedStyleColorResolutionState& state)
{
    CSSUnresolvedStyleColorResolutionNester nester { state };

    auto origin = unresolved.origin->createStyleColor(state);
    if (!origin.isAbsoluteColor()) {
        // If the origin is not absolute, we cannot fully resolve the color yet. Instead, we simplify the calc values using the conversion data, and return a StyleRelativeColor to be resolved at use time.
        return StyleColor {
            StyleRelativeColor<Descriptor> {
                .origin = WTFMove(origin),
                .components = simplifyUnevaluatedCalc(unresolved.components, state.conversionData, CSSCalcSymbolTable { })
            }
        };
    }

    // If the origin is absolute, we can fully resolve the entire color.
    auto color = resolve(
        CSSRelativeColorResolver<Descriptor> {
            .origin = origin.absoluteColor(),
            .components = unresolved.components
        },
        state.conversionData
    );

    return { StyleAbsoluteColor { WTFMove(color) } };
}

template<typename Descriptor>
Color createColor(const CSSUnresolvedRelativeColor<Descriptor>& unresolved, CSSUnresolvedColorResolutionState& state)
{
    auto origin = unresolved.origin->createColor(state);
    if (!origin.isValid())
        return { };

    auto resolver = CSSRelativeColorResolver<Descriptor> {
        .origin = WTFMove(origin),
        .components = unresolved.components
    };

    if (state.conversionData)
        return resolve(WTFMove(resolver), *state.conversionData);

    if (!requiresConversionData(resolver.components))
        return resolveNoConversionDataRequired(WTFMove(resolver));

    return { };
}

template<typename Descriptor>
bool containsColorSchemeDependentColor(const CSSUnresolvedRelativeColor<Descriptor>& unresolved)
{
    return unresolved.origin->containsColorSchemeDependentColor();
}

template<typename Descriptor>
bool containsCurrentColor(const CSSUnresolvedRelativeColor<Descriptor>& unresolved)
{
    return unresolved.origin->containsColorSchemeDependentColor();
}

} // namespace WebCore
