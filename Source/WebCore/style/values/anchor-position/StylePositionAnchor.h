/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
 * Copyright (C) 2026 Suraj Thanugundla <contact@surajt.com>
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StyleAnchorName.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <'position-anchor'> = normal | none | auto | <anchor-name>
// https://drafts.csswg.org/css-anchor-position-1/#propdef-position-anchor
struct PositionAnchor {
    PositionAnchor(CSS::Keyword::Normal keyword)
        : m_value { keyword }
    {
    }

    PositionAnchor(CSS::Keyword::None keyword)
        : m_value { keyword }
    {
    }

    PositionAnchor(CSS::Keyword::Auto keyword)
        : m_value { keyword }
    {
    }

    PositionAnchor(AnchorName&& value)
        : m_value { WTF::move(value) }
    {
    }

    bool isNormal() const { return WTF::holdsAlternative<CSS::Keyword::Normal>(m_value); }
    bool isNone() const { return WTF::holdsAlternative<CSS::Keyword::None>(m_value); }
    bool isAuto() const { return WTF::holdsAlternative<CSS::Keyword::Auto>(m_value); }
    bool isName() const { return WTF::holdsAlternative<AnchorName>(m_value); }

    std::optional<AnchorName> tryName() const
    {
        if (auto* name = std::get_if<AnchorName>(&m_value))
            return *name;
        return { };
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    bool operator==(const PositionAnchor&) const = default;

private:
    Variant<CSS::Keyword::Normal, CSS::Keyword::None, CSS::Keyword::Auto, AnchorName> m_value;
};

// MARK: - Conversion

template<> struct CSSValueConversion<PositionAnchor> {
    auto operator()(BuilderState&, const CSSValue&) -> PositionAnchor;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::PositionAnchor)
