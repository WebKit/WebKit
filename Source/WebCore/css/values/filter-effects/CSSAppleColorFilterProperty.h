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

#include "CSSAppleInvertLightnessFunction.h"
#include "CSSBrightnessFunction.h"
#include "CSSContrastFunction.h"
#include "CSSGrayscaleFunction.h"
#include "CSSHueRotateFunction.h"
#include "CSSInvertFunction.h"
#include "CSSOpacityFunction.h"
#include "CSSSaturateFunction.h"
#include "CSSSepiaFunction.h"

namespace WebCore {
namespace CSS {

using AppleColorFilter = std::variant<
    AppleInvertLightnessFunction,
    BrightnessFunction,
    ContrastFunction,
    GrayscaleFunction,
    HueRotateFunction,
    InvertFunction,
    OpacityFunction,
    SaturateFunction,
    SepiaFunction
>;
using AppleColorFilterValueList = SpaceSeparatedVector<AppleColorFilter>;

// Non-standard type used for the `-apple-color-filter` property. It is similar to CSS::FilterProperty,
// but does not support `blur()`, `drop-shadow()` and reference filters, but adds support for the
// non-standard function `-apple-invert-lightness-filter()`.
// <'-apple-color-filter'> = none | <-apple-color-filter-value-list>
// NOTE: Subclassing, rather than aliasing, is being used to allow easy forward declarations.
struct AppleColorFilterProperty : ListOrNone<AppleColorFilterValueList> { using ListOrNone<AppleColorFilterValueList>::ListOrNone; };

} // namespace CSS
} // namespace WebCore

template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::AppleColorFilterProperty> = true;
