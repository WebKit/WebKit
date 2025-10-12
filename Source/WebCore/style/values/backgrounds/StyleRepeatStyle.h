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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/RenderStyleConstants.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <repeat-style> = repeat-x@(alias=[repeat no-repeat]) | repeat-y@(alias=[no-repeat repeat]) | [repeat | space | round | no-repeat]{1,2}
// https://www.w3.org/TR/css-backgrounds-3/#typedef-repeat-style
struct RepeatStyle {
    MinimallySerializingSpaceSeparatedPoint<FillRepeat> values;

    FillRepeat x() const { return values.x(); }
    FillRepeat y() const { return values.y(); }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (x() == y())
            return visitor(x());
        if (x() == FillRepeat::Repeat && y() == FillRepeat::NoRepeat)
            return visitor(CSS::Keyword::RepeatX { });
        if (x() == FillRepeat::NoRepeat && y() == FillRepeat::Repeat)
            return visitor(CSS::Keyword::RepeatY { });
        return visitor(values);
    }

    bool operator==(const RepeatStyle&) const = default;
    bool operator==(FillRepeat other) const { return x() == other && y() == other; }
};

// MARK: - Conversion

template<> struct CSSValueConversion<RepeatStyle> { auto operator()(BuilderState&, const CSSValue&) -> RepeatStyle; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::RepeatStyle)
