/*
 * Copyright (C) 2025 Sam Weinig. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StylePrimitiveNumericTypes.h>

namespace WebCore {

namespace CSS {
struct DynamicRangeLimitMixFunction;
}

namespace Style {

struct DynamicRangeLimit;

using DynamicRangeLimitMixPercentage = Percentage<CSS::Range{0, 100}>;

struct DynamicRangeLimitMixParameters {
    DynamicRangeLimitMixPercentage standard;
    DynamicRangeLimitMixPercentage constrained;
    DynamicRangeLimitMixPercentage noLimit;

    bool operator==(const DynamicRangeLimitMixParameters&) const = default;
};
using DynamicRangeLimitMixFunction = FunctionNotation<CSSValueDynamicRangeLimitMix, DynamicRangeLimitMixParameters>;

template<size_t I> const auto& get(const DynamicRangeLimitMixParameters& value)
{
    if constexpr (I == 0)
        return value.standard;
    else if constexpr (I == 1)
        return value.constrained;
    else if constexpr (I == 2)
        return value.noLimit;
}

// Overload of operator== for UniqueRef<DynamicRangeLimitMixFunction> to make DynamicRangeLimit::Kind's operator== work.
// FIXME: Replace use of direct UniqueRef with something like std::indirect from https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/p3019r12.pdf to get this for free.
inline bool operator==(const UniqueRef<DynamicRangeLimitMixFunction>& a, const UniqueRef<DynamicRangeLimitMixFunction>& b)
{
    return a.get() == b.get();
}

// Adds the `DynamicRangeLimit` to the `DynamicRangeLimitMixFunction` weighted by the `Percentage`.
void addWeightedLimitTo(DynamicRangeLimitMixFunction&, const DynamicRangeLimit&, const DynamicRangeLimitMixPercentage&);

// MARK: Conversion

template<> struct ToCSS<DynamicRangeLimitMixFunction> { auto operator()(const DynamicRangeLimitMixFunction&, const RenderStyle&) -> CSS::DynamicRangeLimitMixFunction; };
template<> struct ToStyle<CSS::DynamicRangeLimitMixFunction> { auto operator()(const CSS::DynamicRangeLimitMixFunction&, const BuilderState&) -> DynamicRangeLimitMixFunction; };

// MARK: Logging

TextStream& operator<<(TextStream&, const DynamicRangeLimitMixParameters&);

} // namespace Style
} // namespace WebCore

DEFINE_COMMA_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::DynamicRangeLimitMixParameters, 3)
