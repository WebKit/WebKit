/*
 * Copyright (C) 2026 Sam Weinig. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ColorMissingComponents.h"

namespace WebCore {

// The index of the component of `outputCategories` analogous to component `index` of `inputCategories`, or 3 for alpha.
static std::optional<unsigned> analogousComponentIndexForCategories(const ColorComponentCategories& outputCategories, const ColorComponentCategories& inputCategories, unsigned index)
{
    if (index == 3)
        return 3;
    if (!inputCategories[index])
        return std::nullopt;
    for (unsigned outputIndex = 0; outputIndex < 3; ++outputIndex) {
        if (inputCategories[index] == outputCategories[outputIndex])
            return outputIndex;
    }
    return std::nullopt;
}

void carryForwardMissingComponents(const ColorComponentCategories& inputCategories, const ColorComponents<float, 4>& input, const ColorComponentCategories& outputCategories, ColorComponents<float, 4>& output)
{
    // https://drafts.csswg.org/css-color-4/#interpolation-missing

    bool hasAnalogousSet = false;
    bool allAnalogousSetComponentsAreMissing = true;
    for (unsigned index = 0; index < 4; ++index) {
        if (analogousComponentIndexForCategories(outputCategories, inputCategories, index))
            continue;
        hasAnalogousSet = true;
        if (!std::isnan(input[index]))
            allAnalogousSetComponentsAreMissing = false;
    }

    if (hasAnalogousSet && allAnalogousSetComponentsAreMissing) {
        for (unsigned index = 0; index < 4; ++index) {
            if (!analogousComponentIndexForCategories(inputCategories, outputCategories, index))
                output[index] = std::numeric_limits<float>::quiet_NaN();
        }
    }

    for (unsigned index = 0; index < 4; ++index) {
        if (auto outputIndex = analogousComponentIndexForCategories(outputCategories, inputCategories, index); outputIndex && std::isnan(input[index]))
            output[*outputIndex] = std::numeric_limits<float>::quiet_NaN();
    }
}

} // namespace WebCore
