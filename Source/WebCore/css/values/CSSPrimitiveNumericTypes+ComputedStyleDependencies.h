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

struct ComputedStyleDependencies;

namespace CSS {

// MARK: - Computed Style Dependencies

// What properties does this value rely on (eg, font-size for em units)?

// Core unit based dependency analysis.
void collectComputedStyleDependencies(ComputedStyleDependencies&, CSSUnitType);

// Most raw primitives have no dependencies.
constexpr void collectComputedStyleDependencies(ComputedStyleDependencies&, const NumberRaw&) { }
constexpr void collectComputedStyleDependencies(ComputedStyleDependencies&, const PercentageRaw&) { }
constexpr void collectComputedStyleDependencies(ComputedStyleDependencies&, const AngleRaw&) { }
constexpr void collectComputedStyleDependencies(ComputedStyleDependencies&, const TimeRaw&) { }
constexpr void collectComputedStyleDependencies(ComputedStyleDependencies&, const FrequencyRaw&) { }
constexpr void collectComputedStyleDependencies(ComputedStyleDependencies&, const ResolutionRaw&) { }
constexpr void collectComputedStyleDependencies(ComputedStyleDependencies&, const FlexRaw&) { }
constexpr void collectComputedStyleDependencies(ComputedStyleDependencies&, const AnglePercentageRaw&) { }
constexpr void collectComputedStyleDependencies(ComputedStyleDependencies&, const NoneRaw&) { }
constexpr void collectComputedStyleDependencies(ComputedStyleDependencies&, const SymbolRaw&) { }

// The exception being LengthRaw/LengthPercentageRaw.
void collectComputedStyleDependencies(ComputedStyleDependencies&, const LengthRaw&);
void collectComputedStyleDependencies(ComputedStyleDependencies&, const LengthPercentageRaw&);

// All primitives that can contain calc() may have dependencies, as calc() can contain relative lengths even in non-length contexts.
template<typename T> void collectComputedStyleDependencies(ComputedStyleDependencies& dependencies, const PrimitiveNumeric<T>& primitive)
{
    collectComputedStyleDependencies(dependencies, primitive.value);
}

constexpr void collectComputedStyleDependencies(ComputedStyleDependencies&, const None&) { }
constexpr void collectComputedStyleDependencies(ComputedStyleDependencies&, const Symbol&) { }

} // namespace CSS
} // namespace WebCore
