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

#include "CSSPrimitiveNumericTypes.h"

namespace WebCore {
namespace CSS {

// <'border-image-width'> = [ <length-percentage [0,∞]> | <number [0,∞]> | auto ]{1,4}
// https://drafts.csswg.org/css-backgrounds/#propdef-border-image-width
struct BorderImageWidth {
    using Width = std::variant<LengthPercentage<Nonnegative>, Number<Nonnegative>, CSS::Keyword::Auto>;
    using Widths = MinimallySerializingSpaceSeparatedRectEdges<Width>;

    Widths widths;
    bool overridesBorderWidths { false };

    bool operator==(const BorderImageWidth&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(BorderImageWidth, widths)

template<> struct Serialize<BorderImageWidth> { void operator()(StringBuilder&, const SerializationContext&, const BorderImageWidth&); };

bool hasLength(const CSS::BorderImageWidth::Widths&);

} // namespace CSS
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::BorderImageWidth, 1)
