/*
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSPrimitiveNumericTypes.h"

namespace WebCore {
namespace CSS {

// MARK: - Computed Style Dependencies

// What properties does this value rely on (eg, font-size for em units)?

// Core unit based dependency analysis.
template<> struct ComputedStyleDependenciesCollector<CSSUnitType> { void operator()(ComputedStyleDependencies& dependencies, CSSUnitType); };

// Most raw primitives have no dependencies.
template<RawNumeric RawType> struct ComputedStyleDependenciesCollector<RawType> {
    constexpr void operator()(ComputedStyleDependencies&, const RawType&)
    {
        // Nothing to do.
    }
};

// The exception being LengthRaw/LengthPercentageRaw.
template<auto R> struct ComputedStyleDependenciesCollector<LengthRaw<R>> {
    void operator()(ComputedStyleDependencies& dependencies, const LengthRaw<R>& value)
    {
        collectComputedStyleDependencies(dependencies, value.type);
    }
};
template<auto R> struct ComputedStyleDependenciesCollector<LengthPercentageRaw<R>> {
    void operator()(ComputedStyleDependencies& dependencies, const LengthPercentageRaw<R>& value)
    {
        collectComputedStyleDependencies(dependencies, value.type);
    }

};

// All primitives that can contain calc() may have dependencies, as calc() can contain relative lengths even in non-length contexts.
template<RawNumeric RawType> struct ComputedStyleDependenciesCollector<PrimitiveNumeric<RawType>> {
    void operator()(ComputedStyleDependencies& dependencies, const PrimitiveNumeric<RawType>& value)
    {
        WTF::switchOn(value, [&](const auto& value) { collectComputedStyleDependencies(dependencies, value); });
    }
};

// NumberOrPercentageResolvedToNumber trivially forwards to its inner variant.
template<auto R> struct ComputedStyleDependenciesCollector<NumberOrPercentageResolvedToNumber<R>> {
    void operator()(ComputedStyleDependencies& dependencies, const NumberOrPercentageResolvedToNumber<R>& value)
    {
        collectComputedStyleDependencies(dependencies, value.value);
    }
};

} // namespace CSS
} // namespace WebCore
