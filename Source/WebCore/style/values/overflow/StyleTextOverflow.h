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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StyleString.h>
#include <WebCore/StyleValueTypes.h>
#include <wtf/text/AtomString.h>

namespace WebCore {
namespace Style {

// <'text-overflow'> = clip | ellipsis | <string>
// https://drafts.csswg.org/css-overflow-3/#propdef-text-overflow
struct TextOverflow {
    TextOverflow(CSS::Keyword::Clip)
    {
    }

    TextOverflow(CSS::Keyword::Ellipsis)
        : m_type { Type::Ellipsis }
    {
    }

    TextOverflow(String&& string)
        : m_type { Type::String }
        , m_string { WTF::move(string.value) }
    {
    }

    bool isClip() const { return m_type == Type::Clip; }
    bool isEllipsis() const { return m_type == Type::Ellipsis; }
    bool isString() const { return m_type == Type::String; }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        switch (m_type) {
        case Type::Clip:
            return visitor(CSS::Keyword::Clip { });
        case Type::Ellipsis:
            return visitor(CSS::Keyword::Ellipsis { });
        case Type::String:
            return visitor(String { m_string });
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    bool operator==(const TextOverflow&) const = default;

private:
    enum class Type : uint8_t { Clip, Ellipsis, String };

    Type m_type { Type::Clip };
    AtomString m_string { nullAtom() };
};

// MARK: - Conversion

template<> struct CSSValueConversion<TextOverflow> {
    auto operator()(BuilderState&, const CSSValue&) -> TextOverflow;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::TextOverflow)
