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

#include "CSSBlurFunction.h"
#include "CSSBrightnessFunction.h"
#include "CSSContrastFunction.h"
#include "CSSDropShadowFunction.h"
#include "CSSFilterReference.h"
#include "CSSGrayscaleFunction.h"
#include "CSSHueRotateFunction.h"
#include "CSSInvertFunction.h"
#include "CSSOpacityFunction.h"
#include "CSSSaturateFunction.h"
#include "CSSSepiaFunction.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {
namespace CSS {

// Any <filter-function> or a reference to filter via <url>.
// https://drafts.fxtf.org/filter-effects/#typedef-filter-function
using FilterValueKind = Variant<
    BlurFunction,
    BrightnessFunction,
    ContrastFunction,
    DropShadowFunction,
    GrayscaleFunction,
    HueRotateFunction,
    InvertFunction,
    OpacityFunction,
    SaturateFunction,
    SepiaFunction,
    FilterReference
>;
struct FilterValue {
    FilterValueKind value;

    template<typename T>
        requires std::constructible_from<FilterValueKind, T>
    FilterValue(T&& value)
        : value(std::forward<T>(value))
    {
    }

    FORWARD_VARIANT_FUNCTIONS(FilterValue, value)

    bool operator==(const FilterValue&) const = default;
};

// <filter-value-list> = [ <filter-function> | <url> ]+
// https://drafts.fxtf.org/filter-effects/#typedef-filter-value-list
using FilterValueList = SpaceSeparatedVector<FilterValue>;

// <'filter'> = none | <filter-value-list>
// https://drafts.fxtf.org/filter-effects/#propdef-filter
struct Filter : ListOrNone<FilterValueList> {
    using ListOrNone<FilterValueList>::ListOrNone;
};

} // namespace CSS
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::CSS::FilterValue)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::CSS::Filter)
