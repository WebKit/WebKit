/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#include <wtf/MathExtras.h>
#include <wtf/Vector.h>

namespace WebCore {
namespace CSS {

enum class ForceNormalization : bool { No, Yes };

template<typename Percentages>
struct NormalizedMixPercentages {
    Percentages percentages;
    double leftover;
};

template<ForceNormalization forceNormalization, typename Percentages = Vector<double, 4>>
auto normalizedMixPercentages(const auto& components) -> NormalizedMixPercentages<Percentages>
{
    // https://drafts.csswg.org/css-values-5/#normalize-mix-percentages

    // The percentages are normalized as follows:

    // 1. Let specified sum be the sum of the percentages specified in items (clamped to 100%), or 0% if the percentages are omitted for all items.

    double specifiedSum = 0;
    size_t numberOfOmittedPercentages = 0;

    for (const auto& component : components) {
        if (component.percentage)
            specifiedSum += component.percentage->value;
        else
            ++numberOfOmittedPercentages;
    }

    specifiedSum = clampTo<double>(specifiedSum, 0, 100);

    // 2. For each omitted percentage in items, set it to (100% - specified sum) / (number of omitted percentages).

    auto omittedPercentageValue = numberOfOmittedPercentages ? (100.0 - specifiedSum) / numberOfOmittedPercentages : 0.0;

    auto percentages = Percentages::map(components, [&](const auto& component) -> double {
        return component.percentage ? component.percentage->value : omittedPercentageValue;
    });

    // 3. Let total be the sum of the percentages of all the items.

    double total = 0;
    for (auto percentage : percentages)
        total += percentage;

    // 4. If total is greater than 100%, or if total is greater than 0% and the force normalization flag is true, multiply every percentage in items by (100% / total).

    if (total > 100 || (total > 0 && forceNormalization == ForceNormalization::Yes)) {
        for (auto& percentage : percentages)
            percentage *= (100 / total);
    }

    // 5. If total is less than 100%, let leftover be (100% - total). Otherwise, let leftover be 0%.

    double leftover = total < 100 ? 100 - total : 0;

    // 6. Return items and leftover.

    return { WTF::move(percentages), leftover };
}

} // namespace CSS
} // namespace WebCore
