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

#pragma once

#include <WebCore/ColorConversion.h>
#include <array>
#include <optional>

namespace WebCore {

// Conversion function for typed colors that carries forward missing component values
// from analogous components in the input type.
template<UnpremultipliedColorType Output, GamutMappingAlgorithm GamutMapping = ClipGamutMapping, UnpremultipliedColorType Input> Output convertColorCarryingForwardMissing(const Input& color);

// Conversion function for raw color components with an associated color space that carries forward
// missing component values from analogous components in the input color space.
template<typename Output, GamutMappingAlgorithm GamutMapping = ClipGamutMapping> Output convertColorComponentsCarryingForwardMissing(ColorSpaceName inputColorSpace, const ColorComponents<float, 4>& input);

// MARK: Color types known at compile time.

// Looks for a an analogous component in `Output` of `Input[IndexInInput]`.
// For example, take the following:
//
//   auto result = analogousComponentIndex<LCHA<float>, HSLA<float>, 0>;
//
// This returns "2", because the input component specified (the hue in HSLA)
// is analogous to component "2" in the output (the hue in LCHA).
//
// If there is no analogous component, `std::nullopt` is returned.
template<typename Output, typename Input, unsigned IndexInInput>
consteval std::optional<unsigned> analogousComponentIndex()
{
    if constexpr (IndexInInput == 3)
        return 3; // Special case alpha, it always should carry forward.
    else if constexpr (constexpr auto inputCategory = Input::Model::componentInfo[IndexInInput].category) {
        if constexpr (*inputCategory == Output::Model::componentInfo[0].category)
            return 0;
        else if constexpr (*inputCategory == Output::Model::componentInfo[1].category)
            return 1;
        else if constexpr (*inputCategory == Output::Model::componentInfo[2].category)
            return 2;
        else
            return std::nullopt;
    } else
        return std::nullopt;
}

// Determines if an analogous set exists between Output and Input. An analogous set
// is comprised of any components that don't have analogous components. So, for there
// to be an analogous set, at least one component must not have an analogous component.
template<typename Output, typename Input>
consteval bool hasAnalogousSet()
{
    return !analogousComponentIndex<Output, Input, 0>()
        || !analogousComponentIndex<Output, Input, 1>()
        || !analogousComponentIndex<Output, Input, 2>()
        || !analogousComponentIndex<Output, Input, 3>();
}

// Utility to check whether if all analogous set components are missing. When this is
// true, the analogous set components will all be forwarded as missing.
template<typename Output, typename Input>
    requires (hasAnalogousSet<Output, Input>())
constexpr bool areAllAnalogousSetComponentsMissing(ColorComponents<float, 4>& input)
{
    if constexpr (!analogousComponentIndex<Output, Input, 0>()) {
        if (!std::isnan(input[0]))
            return false;
    }
    if constexpr (!analogousComponentIndex<Output, Input, 1>()) {
        if (!std::isnan(input[1]))
            return false;
    }
    if constexpr (!analogousComponentIndex<Output, Input, 2>()) {
        if (!std::isnan(input[2]))
            return false;
    }
    if constexpr (!analogousComponentIndex<Output, Input, 3>()) {
        if (!std::isnan(input[3]))
            return false;
    }
    return true;
}


// Utility to update appropriate component in `output` if an analogous component
// in `input` is missing (which is encoded as NaN in color types).
template<typename Output, typename Input, unsigned IndexInInput>
constexpr void tryToCarryForwardMissingForAnalogousComponent(const ColorComponents<float, 4>& input, ColorComponents<float, 4>& output)
{
    if constexpr (constexpr auto analogousComponentIndexInOutput = analogousComponentIndex<Output, Input, IndexInInput>()) {
        if (std::isnan(input[IndexInInput]))
            output[*analogousComponentIndexInOutput] = std::numeric_limits<float>::quiet_NaN();
    }
}

// Utility to update appropriate component in `output` if all analogous set components
// in `input` are missing  (which is encoded as NaN in color types) and this component
// in `output` is part of the analogous set.
template<typename Output, typename Input, unsigned IndexInOutput>
constexpr void tryToCarryForwardMissingForAnalogousSetComponent(ColorComponents<float, 4>& output)
{
    if constexpr (!analogousComponentIndex<Input, Output, IndexInOutput>())
        output[IndexInOutput] = std::numeric_limits<float>::quiet_NaN();
}

// Performs color space conversion followed by carrying forward missing components
// from `Input` for analogous components in `Output` as described by CSS Color 4
// § 12. Color Interpolation, https://drafts.csswg.org/css-color-4/#interpolation.
template<UnpremultipliedColorType Output, GamutMappingAlgorithm GamutMapping, UnpremultipliedColorType Input> Output convertColorCarryingForwardMissing(const Input& color)
{
    if constexpr (std::is_same_v<Input, Output>)
        return convertColor<Output, GamutMapping>(color);
    else if constexpr (std::is_same_v<typename Input::ComponentType, uint8_t> || std::is_same_v<typename Output::ComponentType, uint8_t>)
        return convertColor<Output, GamutMapping>(color);
    else {
        auto input = asColorComponents(color.unresolved());
        auto output = asColorComponents(convertColor<Output, GamutMapping>(color).unresolved());

        if constexpr (hasAnalogousSet<Output, Input>()) {
            if (areAllAnalogousSetComponentsMissing<Output, Input>(input)) {
                tryToCarryForwardMissingForAnalogousSetComponent<Output, Input, 0>(output);
                tryToCarryForwardMissingForAnalogousSetComponent<Output, Input, 1>(output);
                tryToCarryForwardMissingForAnalogousSetComponent<Output, Input, 2>(output);
                tryToCarryForwardMissingForAnalogousSetComponent<Output, Input, 3>(output);
            }
        }

        tryToCarryForwardMissingForAnalogousComponent<Output, Input, 0>(input, output);
        tryToCarryForwardMissingForAnalogousComponent<Output, Input, 1>(input, output);
        tryToCarryForwardMissingForAnalogousComponent<Output, Input, 2>(input, output);
        tryToCarryForwardMissingForAnalogousComponent<Output, Input, 3>(input, output);

        return makeFromComponents<Output>(output);
    }
}

// MARK: Color types known only at runtime.

// The category of each component, used to find analogous components at runtime, as
// analogousComponentIndex() does for color types known at compile time.
using ColorComponentCategories = std::array<std::optional<ColorComponentCategory>, 3>;

template<typename ColorType> consteval ColorComponentCategories colorComponentCategories()
{
    return { ColorType::Model::componentInfo[0].category, ColorType::Model::componentInfo[1].category, ColorType::Model::componentInfo[2].category };
}

// Does what convertColorCarryingForwardMissing() does after converting, for color types known only at runtime.
WEBCORE_EXPORT void carryForwardMissingComponents(const ColorComponentCategories& inputCategories, const ColorComponents<float, 4>& input, const ColorComponentCategories& outputCategories, ColorComponents<float, 4>& output);

// Not inlined, so that the dispatch in convertColorComponentsCarryingForwardMissing() calls one copy per output type, rather than inlining one for every input type.
template<typename Output> NEVER_INLINE Output withMissingComponentsCarriedForward(const ColorComponentCategories& inputCategories, const ColorComponents<float, 4>& input, const Output& output)
{
    auto outputComponents = asColorComponents(output.unresolved());
    carryForwardMissingComponents(inputCategories, input, colorComponentCategories<Output>(), outputComponents);
    return makeFromComponents<Output>(outputComponents);
}

template<typename Output, GamutMappingAlgorithm GamutMapping> Output convertColorComponentsCarryingForwardMissing(ColorSpaceName inputColorSpace, const ColorComponents<float, 4>& input)
{
    if constexpr (MapsToGamut<Output, GamutMapping>) {
        auto output = convertColorComponents<Output, GamutMapping>(inputColorSpace, input);
        if constexpr (!std::same_as<typename Output::ComponentType, uint8_t>) {
            if (unresolvedColor(input).anyComponentIsNone()) {
                auto inputCategories = callWithColorType<float>(inputColorSpace, []<typename Input> {
                    return colorComponentCategories<Input>();
                });
                return withMissingComponentsCarriedForward<Output>(inputCategories, input, output);
            }
        }
        return output;
    } else {
        return callWithColorType<float>(inputColorSpace, [&]<typename Input> -> Output {
            if constexpr (ConvertsWithoutXYZ<Input, Output>)
                return convertColorCarryingForwardMissing<Output>(makeFromComponents<Input>(input));
            else {
                auto output = convertColorComponentsSplitAtXYZ<Input, Output>(input);
                if constexpr (!std::same_as<typename Output::ComponentType, uint8_t>) {
                    if (unresolvedColor(input).anyComponentIsNone())
                        return withMissingComponentsCarriedForward<Output>(colorComponentCategories<Input>(), input, output);
                }
                return output;
            }
        });
    }
}

} // namespace WebCore
