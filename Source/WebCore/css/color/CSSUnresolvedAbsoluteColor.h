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

#include "CSSAbsoluteColorResolver.h"
#include "CSSAbsoluteColorSerialization.h"
#include "CSSColorDescriptors.h"
#include "CSSUnresolvedColorResolutionState.h"
#include "CSSUnresolvedStyleColorResolutionState.h"
#include "StyleAbsoluteColor.h"
#include "StyleColor.h"
#include <wtf/Forward.h>

namespace WebCore {

template<typename D>
struct CSSUnresolvedAbsoluteColor {
    using Descriptor = D;

    CSSColorParseTypeWithCalc<Descriptor> components;

    bool operator==(const CSSUnresolvedAbsoluteColor<Descriptor>&) const = default;
};

template<typename Descriptor>
void serializationForCSS(StringBuilder& builder, const CSSUnresolvedAbsoluteColor<Descriptor>& unresolved)
{
    serializationForCSSAbsoluteColor(builder, unresolved);
}

template<typename Descriptor>
String serializationForCSS(const CSSUnresolvedAbsoluteColor<Descriptor>& unresolved)
{
    StringBuilder builder;
    serializationForCSS(builder, unresolved);
    return builder.toString();
}

template<typename Descriptor>
StyleColor createStyleColor(const CSSUnresolvedAbsoluteColor<Descriptor>& unresolved, CSSUnresolvedStyleColorResolutionState& state)
{
    CSSUnresolvedStyleColorResolutionNester nester { state };

    auto resolver = CSSAbsoluteColorResolver<Descriptor> {
        .components = unresolved.components,
        .nestingLevel = state.nestingLevel
    };

    return StyleColor { StyleAbsoluteColor { resolve(WTFMove(resolver), state.conversionData) } };
}

template<typename Descriptor>
Color createColor(const CSSUnresolvedAbsoluteColor<Descriptor>& unresolved, CSSUnresolvedColorResolutionState& state)
{
    CSSUnresolvedColorResolutionNester nester { state };

    auto resolver = CSSAbsoluteColorResolver<Descriptor> {
        .components = unresolved.components,
        .nestingLevel = state.nestingLevel
    };

    if (state.conversionData)
        return resolve(WTFMove(resolver), *state.conversionData);

    if (!requiresConversionData(resolver))
        return resolveNoConversionDataRequired(WTFMove(resolver));

    return { };
}

template<typename Descriptor>
constexpr bool containsColorSchemeDependentColor(const CSSUnresolvedAbsoluteColor<Descriptor>&)
{
    return false;
}

template<typename Descriptor>
constexpr bool containsCurrentColor(const CSSUnresolvedAbsoluteColor<Descriptor>&)
{
    return false;
}

} // namespace WebCore
