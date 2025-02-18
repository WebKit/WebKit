/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include "CSSBorderImageOutset.h"
#include "CSSBorderImageRepeat.h"
#include "CSSBorderImageSlice.h"
#include "CSSBorderImageSource.h"
#include "CSSBorderImageWidth.h"

namespace WebCore {
namespace CSS {

// <'border-image'> = <'border-image-source'>
//                 || <'border-image-slice'> [ / <'border-image-width'> | / <'border-image-width'>? / <'border-image-outset'> ]?
//                 || <'border-image-repeat'>
// https://drafts.csswg.org/css-backgrounds/#propdef-border-image
struct BorderImage {
    std::optional<BorderImageSource> source;
    std::optional<BorderImageSlice> slice;
    std::optional<BorderImageWidth> width;
    std::optional<BorderImageOutset> outset;
    std::optional<BorderImageRepeat> repeat;

    struct MarkableTraits {
        static bool isEmptyValue(const BorderImage& value) { return !value.source && !value.slice && !value.width && !value.outset && !value.repeat; }
        static BorderImage emptyValue() { return BorderImage { }; }
    };

    bool operator==(const BorderImage&) const = default;
};

template<size_t I> const auto& get(const BorderImage& value)
{
    if constexpr (!I)
        return value.source;
    else if constexpr (I == 1)
        return value.slice;
    else if constexpr (I == 2)
        return value.width;
    else if constexpr (I == 3)
        return value.outset;
    else if constexpr (I == 4)
        return value.repeat;
}

template<> struct Serialize<BorderImage> { void operator()(StringBuilder&, const SerializationContext&, const BorderImage&); };

} // namespace CSS
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::BorderImage, 5)
