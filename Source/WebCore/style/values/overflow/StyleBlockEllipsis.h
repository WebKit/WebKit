/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "StyleValueTypes.h"
#include <wtf/text/AtomString.h>

namespace WebCore {
namespace Style {

// <'block-ellipse'> = none | auto | <string>
// https://www.w3.org/TR/css-overflow-4/#propdef-block-ellipsis
struct BlockEllipsis {
    BlockEllipsis(CSS::Keyword::None)
    {
    }

    BlockEllipsis(CSS::Keyword::Auto)
        : m_type { Type::Auto }
    {
    }

    BlockEllipsis(AtomString&& string)
        : m_type { Type::String }
        , m_string { WTFMove(string) }
    {
    }

    bool isNone() const { return m_type == Type::None; }
    bool isAuto() const { return m_type == Type::Auto; }
    bool isString() const { return m_type == Type::String; }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        switch (m_type) {
        case Type::None:
            return visitor(CSS::Keyword::None { });
        case Type::Auto:
            return visitor(CSS::Keyword::Auto { });
        case Type::String:
            return visitor(m_string);
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    bool operator==(const BlockEllipsis&) const = default;

private:
    enum class Type : uint8_t { None, Auto, String };

    Type m_type { Type::None };
    AtomString m_string { nullAtom() };
};

// MARK: - Conversion

template<> struct CSSValueConversion<BlockEllipsis> { auto operator()(BuilderState&, const CSSValue&) -> BlockEllipsis; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::BlockEllipsis)
