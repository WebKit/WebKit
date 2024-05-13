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
#include "Color.h"
#include "ColorTypes.h"

namespace WebCore {

// This file implements support for converting "typed colors" (e.g. `SRGBA<float>`)
// into the type erased `Color`, allowing for compaction and optional flags.

constexpr bool outsideSRGBGamut(HSLA<float> hsla)
{
    auto unresolved = hsla.unresolved();
    return unresolved.saturation > 100.0 || unresolved.lightness < 0.0 || unresolved.lightness > 100.0;
}

constexpr bool outsideSRGBGamut(HWBA<float> hwba)
{
    auto unresolved = hwba.unresolved();
    return unresolved.whiteness < 0.0 || unresolved.whiteness > 100.0 || unresolved.blackness < 0.0 || unresolved.blackness > 100.0;
}

constexpr bool outsideSRGBGamut(SRGBA<float>)
{
    return false;
}

template<typename Descriptor, CSSColorFunctionForm Form>
struct ConvertToColor;

template<typename Descriptor>
struct ConvertToColor<Descriptor, CSSColorFunctionForm::Absolute> {
    static Color convertToColor(GetColorType<Descriptor> color, unsigned nestingLevel)
    {
        if constexpr (Descriptor::allowConversionTo8BitSRGB) {
            if constexpr (Descriptor::syntax == CSSColorFunctionSyntax::Modern) {
                if (color.unresolved().anyComponentIsNone()) {
                    // If any component uses "none", we store the value as is to allow for storage of the special value as NaN.
                    return { color, Descriptor::flagsForAbsolute };
                }
            }

            if (outsideSRGBGamut(color)) {
                // If any component is outside the reference range, we store the value as is to allow for non-SRGB gamut values.
                return { color, Descriptor::flagsForAbsolute };
            }

            if (nestingLevel > 1) {
                // If the color is being consumed as part of a composition (relative color, color-mix, light-dark, etc.), we
                // store the value as is to allow for maximum precision.
                return { color, Descriptor::flagsForAbsolute };
            }

            // The explicit conversion to SRGBA<uint8_t> is an intentional performance optimization that allows storing the
            // color with no extra allocation for an extended color object. This is permissible in some case due to the
            // historical requirement that some syntaxes serialize using the legacy color syntax (rgb()/rgba()) and
            // historically have used the 8-bit rgba internal representation in engines.
            return { convertColor<SRGBA<uint8_t>>(color), Descriptor::flagsForAbsolute };
        } else
            return { color, Descriptor::flagsForAbsolute };
    }
};

template<typename Descriptor>
struct ConvertToColor<Descriptor, CSSColorFunctionForm::Relative> {
    static Color convertToColor(GetColorType<Descriptor> color)
    {
        return { color, Descriptor::flagsForRelative };
    }
};

template<typename Descriptor, CSSColorFunctionForm Form>
Color convertToColor(GetColorType<Descriptor> color, unsigned nestingLevel)
{
    static_assert(Form == CSSColorFunctionForm::Absolute);
    return ConvertToColor<Descriptor, Form>::convertToColor(color, nestingLevel);
}

template<typename Descriptor, CSSColorFunctionForm Form>
Color convertToColor(GetColorType<Descriptor> color)
{
    static_assert(Form == CSSColorFunctionForm::Relative);
    return ConvertToColor<Descriptor, Form>::convertToColor(color);
}

template<typename Descriptor, CSSColorFunctionForm Form>
Color convertToColor(std::optional<GetColorType<Descriptor>> color, unsigned nestingLevel)
{
    static_assert(Form == CSSColorFunctionForm::Absolute);

    if (!color)
        return { };
    return convertToColor<Descriptor, Form>(*color, nestingLevel);
}

template<typename Descriptor, CSSColorFunctionForm Form>
Color convertToColor(std::optional<GetColorType<Descriptor>> color)
{
    static_assert(Form == CSSColorFunctionForm::Relative);
    if (!color)
        return { };
    return convertToColor<Descriptor, Form>(*color);
}

} // namespace WebCore
