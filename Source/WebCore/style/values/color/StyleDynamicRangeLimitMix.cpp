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
#include "StyleDynamicRangeLimitMix.h"

#include "CSSDynamicRangeLimitMix.h"
#include "StyleDynamicRangeLimit.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePrimitiveNumericTypes+Logging.h"

namespace WebCore {
namespace Style {

using namespace CSS::Literals;

void addWeightedLimitTo(DynamicRangeLimitMixFunction& addingTo, const DynamicRangeLimit& limit, const DynamicRangeLimitMixPercentage& mixPercentage)
{
    WTF::switchOn(limit,
        [&](const CSS::Keyword::Standard&) {
            addingTo->standard.value        += mixPercentage.value; /* implicit multiplication by (100 / 100) elided */
        },
        [&](const CSS::Keyword::Constrained&) {
            addingTo->constrained.value     += mixPercentage.value; /* implicit multiplication by (100 / 100) elided */
        },
        [&](const CSS::Keyword::NoLimit&) {
            addingTo->noLimit.value         += mixPercentage.value; /* implicit multiplication by (100 / 100) elided */
        },
        [&](const DynamicRangeLimitMixFunction& innerMix) {
            if (!Style::isZero(innerMix->standard))
                addingTo->standard.value    += mixPercentage.value * (innerMix->standard.value    / 100.0);
            if (!Style::isZero(innerMix->constrained))
                addingTo->constrained.value += mixPercentage.value * (innerMix->constrained.value / 100.0);
            if (!Style::isZero(innerMix->noLimit))
                addingTo->noLimit.value     += mixPercentage.value * (innerMix->noLimit.value     / 100.0);
        }
    );
}

// MARK: - Conversion

auto ToCSS<DynamicRangeLimitMixFunction>::operator()(const DynamicRangeLimitMixFunction& mix, const RenderStyle& style) -> CSS::DynamicRangeLimitMixFunction
{
    CSS::DynamicRangeLimitMixFunction result;

    if (!Style::isZero(mix.parameters.standard))
        result->parameters.value.append({ { CSS::Keyword::Standard { } },    toCSS(mix.parameters.standard, style) });
    if (!Style::isZero(mix.parameters.constrained))
        result->parameters.value.append({ { CSS::Keyword::Constrained { } }, toCSS(mix.parameters.constrained, style) });
    if (!Style::isZero(mix.parameters.noLimit))
        result->parameters.value.append({ { CSS::Keyword::NoLimit { } },     toCSS(mix.parameters.noLimit, style) });

    return result;
}

auto ToStyle<CSS::DynamicRangeLimitMixFunction>::operator()(const CSS::DynamicRangeLimitMixFunction& mix, const BuilderState& state) -> DynamicRangeLimitMixFunction
{
    // https://drafts.csswg.org/css-color-hdr/#computing-dynamic-range-limit

    // This function implements steps 1 - 4 of the algorithm for computing the dynamic
    // range limit value. The final 2 steps are by `ToStyle<CSS::DynamicRangeLimit>`.

    struct ComputedMixComponent {
        DynamicRangeLimit limit;
        DynamicRangeLimitMixPercentage mixPercentage;
    };

    // 1. Let v1, ..., vN be the computed values for the parameters to be mixed
    // 2. Let p1, ..., pN be the mixing percentages, normalized to sum to 100%.

    // FIXME: Ideally we'd be able to mark the computedMixComponents() lamdba as NOESCAPE to avoid having to suppress.
    SUPPRESS_UNCOUNTED_LAMBDA_CAPTURE auto computedMixComponents = mix->parameters.map([&](auto& componentCSS) -> ComputedMixComponent {
        return { toStyle(get<0>(componentCSS), state), toStyle(get<1>(componentCSS), state) };
    });

    // If the percentages do not sum to 100%, scale all the mix percentages so they do, normalizing the values.
    DynamicRangeLimitMixPercentage mixPercentageSum = 0;
    for (auto& [limit, mixPercentage] : computedMixComponents)
        mixPercentageSum.value += mixPercentage.value;

    if (mixPercentageSum != 100_css_percentage) {
        for (auto& component : computedMixComponents)
            component.mixPercentage.value *= 100.0 / mixPercentageSum.value;
    }

    // 3. Define the contributing percentages as:
    //   - Let p1_standard,...,pN_standard be the percentages for standard in v1,...,vN
    //   - Let p1_constrained,...,pN_constrained be the percentages for constrained in v1,...,vN
    //   - Let p1_no_limit,...,pN_no_limit be the percentages for no-limit in v1,...,vN

    // 4. Compute the weighted sums as:
    //   - p_standard=(p1_standard*p1+...+pN_standard*pN)/100.
    //   - p_constrained=(p1_constrained*p1+...+pN_constrained*pN)/100.
    //   - p_no_limit=(p1_no_limit*p1+...+pN_no_limit*pN)/100.

    DynamicRangeLimitMixFunction function { .parameters = { 0_css_percentage, 0_css_percentage, 0_css_percentage } };

    for (auto& [limit, mixPercentage] : computedMixComponents)
        addWeightedLimitTo(function, limit, mixPercentage);

    // NOTE: These last two steps are performed by `ToStyle<CSS::DynamicRangeLimit>`.

    // 5. If p_standard, p_constrained, or p_no_limit equals 100%, then the computed value is standard, constrained, or no-limit, respectively.
    // 6. Otherwise, the computed value is dynamic-range-limit-mix(), with parameters standard, constrained, and no-limit, in that order, and percentages p_standard, p_constrained, and p_no_limit, omitting parameters with a percentage equal to 0%.

    return function;
}

// MARK: - Logging

TextStream& operator<<(TextStream& ts, const DynamicRangeLimitMixParameters& mix)
{
    bool needsComma = false;
    if (!Style::isZero(mix.standard)) {
        ts << "standard "_s << mix.standard;
        needsComma = true;
    }
    if (!Style::isZero(mix.constrained)) {
        if (needsComma)
            ts << ", "_s;
        ts << "constrained "_s << mix.constrained;
        needsComma = true;
    }
    if (!Style::isZero(mix.noLimit)) {
        if (needsComma)
            ts << ", "_s;
        ts << "no-limit "_s << mix.noLimit;
    }

    return ts;
}

} // namespace Style
} // namespace WebCore
