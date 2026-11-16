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

#include "CSSValueTypes.h"

namespace WebCore {
namespace CSS {

// <'mask-border-repeat'> = [ stretch | repeat | round | space ]{1,2}
// https://drafts.fxtf.org/css-masking-1/#propdef-mask-border-repeat
struct MaskBorderRepeat {
    using Value = Variant<Keyword::Stretch, Keyword::Repeat, Keyword::Round, Keyword::Space>;
    MinimallySerializingSpaceSeparatedSize<Value> values;

    constexpr Value horizontalRule() const { return values.width(); }
    constexpr Value verticalRule() const  { return values.height(); }

    constexpr bool operator==(const MaskBorderRepeat&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(MaskBorderRepeat, values);

// MARK: - Conversion

template<> struct CSSValueCreation<MaskBorderRepeat> { auto operator()(CSSValuePool&, const MaskBorderRepeat&) -> Ref<CSSValue>; };

} // namespace CSS
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::CSS::MaskBorderRepeat)
