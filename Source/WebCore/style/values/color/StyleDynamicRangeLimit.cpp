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

#include "config.h"
#include "StyleDynamicRangeLimit.h"

#include "AnimationUtilities.h"
#include "CSSDynamicRangeLimit.h"
#include "CSSDynamicRangeLimitMix.h"
#include "CSSDynamicRangeLimitValue.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"
#include "PlatformDynamicRangeLimit.h"
#include "StyleBuilderChecking.h"
#include "StyleDynamicRangeLimitMix.h"

namespace WebCore {
namespace Style {

using namespace CSS::Literals;

// Resolves a `dynamic-range-limit-mix` function value for the `dynamic-range-limit` property.
static DynamicRangeLimit resolve(DynamicRangeLimitMixFunction&& mix)
{
    // https://drafts.csswg.org/css-color-hdr/#computing-dynamic-range-limit

    // This function implements steps 5 and 6 of the algorithm for computing the dynamic
    // range limit value. The first 4 steps are in `ToStyle<CSS::DynamicRangeLimitMixFunction>`.

    if (mix->standard == 100_css_percentage)
        return DynamicRangeLimit(CSS::Keyword::Standard { });
    if (mix->constrained == 100_css_percentage)
        return DynamicRangeLimit(CSS::Keyword::Constrained { });
    if (mix->noLimit == 100_css_percentage)
        return DynamicRangeLimit(CSS::Keyword::NoLimit { });
    return DynamicRangeLimit(WTFMove(mix));
}

// MARK: - Conversion

auto ToCSS<DynamicRangeLimit>::operator()(const DynamicRangeLimit& limit, const RenderStyle& style) -> CSS::DynamicRangeLimit
{
    return WTF::switchOn(limit,
        [&](const auto& value) -> CSS::DynamicRangeLimit {
            return CSS::DynamicRangeLimit(toCSS(value, style));
        }
    );
}

auto ToStyle<CSS::DynamicRangeLimit>::operator()(const CSS::DynamicRangeLimit& limit, const BuilderState& state) -> DynamicRangeLimit
{
    return WTF::switchOn(limit,
        [&]<CSSValueID Id>(const Constant<Id>& keyword) -> DynamicRangeLimit {
            return DynamicRangeLimit(keyword);
        },
        [&](const CSS::DynamicRangeLimitMixFunction& mix) -> DynamicRangeLimit {
            return resolve(toStyle(mix, state));
        }
    );
}

Ref<CSSValue> CSSValueCreation<DynamicRangeLimit>::operator()(CSSValuePool&, const RenderStyle& style, const DynamicRangeLimit& value)
{
    return CSSDynamicRangeLimitValue::create(toCSS(value, style));
}

auto CSSValueConversion<DynamicRangeLimit>::operator()(BuilderState& state, const CSSValue& value) -> DynamicRangeLimit
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        switch (primitiveValue->valueID()) {
        case CSSValueStandard:      return CSS::Keyword::Standard { };
        case CSSValueConstrained:   return CSS::Keyword::Constrained { };
        case CSSValueNoLimit:       return CSS::Keyword::NoLimit { };
        default:
            break;
        }

        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::NoLimit { };
    }

    RefPtr dynamicRangeLimit = requiredDowncast<CSSDynamicRangeLimitValue>(state, value);
    if (!dynamicRangeLimit)
        return CSS::Keyword::NoLimit { };
    return toStyle(dynamicRangeLimit->dynamicRangeLimit(), state);
}

// MARK: - Serialization

void Serialize<DynamicRangeLimit>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const DynamicRangeLimit& value)
{
    CSS::serializationForCSS(builder, context, toCSS(value, style));
}

// MARK: - Blending

auto Blending<DynamicRangeLimit>::blend(const DynamicRangeLimit& from, const DynamicRangeLimit& to, const BlendingContext& context) -> DynamicRangeLimit
{
    // Blending is defined as "by dynamic-range-limit-mix()". This requires computing the equivalent of:
    //
    //   dynamic-range-limit-mix(`from` (1 - context.progress) * 100%, `to` context.progress * 100%)
    //
    // and then resolving that as we would when converting from CSS::DynamicRangeLimit. Rather than
    // construct a full fledged CSS::DynamicRangeLimitMixFunction, we can utilize the weighted sum
    // addition functionality to construct just what we need and then resolve that directly.

    DynamicRangeLimitMixPercentage fromMixPercentage = (1.0 - context.progress) * 100.0;
    DynamicRangeLimitMixPercentage toMixPercentage   = (context.progress) * 100.0;

    DynamicRangeLimitMixFunction function { .parameters = { 0_css_percentage, 0_css_percentage, 0_css_percentage } };

    addWeightedLimitTo(function, from, fromMixPercentage);
    addWeightedLimitTo(function, to, toMixPercentage);

    return resolve(WTFMove(function));
}

// MARK: - Conversion to platform object

PlatformDynamicRangeLimit DynamicRangeLimit::toPlatformDynamicRangeLimit() const
{
    return WTF::switchOn(*this, [&]<typename Kind>(const Kind& kind) -> PlatformDynamicRangeLimit {
        if constexpr (std::is_same_v<Kind, CSS::Keyword::Standard>)
            return PlatformDynamicRangeLimit::standard();
        else if constexpr (std::is_same_v<Kind, CSS::Keyword::Constrained>)
            return PlatformDynamicRangeLimit::constrained();
        else if constexpr (std::is_same_v<Kind, CSS::Keyword::NoLimit>)
            return PlatformDynamicRangeLimit::noLimit();
        else if constexpr (std::is_same_v<Kind, Style::DynamicRangeLimitMixFunction>)
            return PlatformDynamicRangeLimit(float(kind->standard.value), float(kind->constrained.value), float(kind->noLimit.value));
    });
}

} // namespace Style
} // namespace WebCore
