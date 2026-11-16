/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/BoxSides.h>
#include <WebCore/CSSFlexWrap.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

using CSS::FlexWrapType;

// FIXME: Style::FlexWrap and CSS::FlexWrap are identical and ideally would be the same
// type using TreatAsNonConverting<>, but currently TreatAsNonConverting<> cannot be used
// on types that are TreatAsVariantLike<>, which CSS::FlexWrap is.

// <'flex-wrap'> = nowrap | [ wrap | wrap-reverse ] || balance
// https://drafts.csswg.org/css-flexbox-2/#propdef-flex-wrap
struct FlexWrap : CSS::FlexWrap {
    using CSS::FlexWrap::FlexWrap;

    FlexWrap(CSS::FlexWrap base)
        : CSS::FlexWrap { base }
    {
    }
};

inline AxisDirection toAxisDirection(const FlexWrap& flexWrap)
{
    return flexWrap.isReverse() ? AxisDirection::Reverse : AxisDirection::Normal;
}

// MARK: - Conversion

template<> struct CSSValueConversion<FlexWrap> {
    auto operator()(BuilderState&, const CSSValue&) -> FlexWrap;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::FlexWrap)
