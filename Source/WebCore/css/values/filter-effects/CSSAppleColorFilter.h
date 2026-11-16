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
#include <wtf/StdLibExtras.h>

namespace WebCore {
namespace CSS {

// Non-standard types used for the `-apple-color-filter` property. It is similar to <'filter'>,
// but does not support `blur()`, `drop-shadow()` and reference filters, but adds support for the
// non-standard function `-apple-invert-lightness-filter()`.

// Any <apple-color-filter-function>.
// (Equivalent of https://drafts.fxtf.org/filter-effects/#typedef-filter-function)
using AppleColorFilterValueKind = Variant<
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
struct AppleColorFilterValue {
    AppleColorFilterValueKind value;

    template<typename T>
        requires std::constructible_from<AppleColorFilterValueKind, T>
    AppleColorFilterValue(T&& value)
        : value(std::forward<T>(value))
    {
    }

    FORWARD_VARIANT_FUNCTIONS(AppleColorFilterValue, value)

    bool operator==(const AppleColorFilterValue&) const = default;
};

// <apple-color-filter-value-list> = [ <apple-color-filter-function> ]+
// (Equivalent of https://drafts.fxtf.org/filter-effects/#typedef-filter-value-list)
using AppleColorFilterValueList = SpaceSeparatedVector<AppleColorFilterValue>;

// <'-apple-color-filter'> = none | <-apple-color-filter-value-list>
// (Equivalent of https://drafts.fxtf.org/filter-effects/#propdef-filter)
struct AppleColorFilter : ListOrNone<AppleColorFilterValueList> {
    using ListOrNone<AppleColorFilterValueList>::ListOrNone;
};

} // namespace CSS
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::CSS::AppleColorFilterValue)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::CSS::AppleColorFilter)
