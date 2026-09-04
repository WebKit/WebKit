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

#if ENABLE(SPATIAL_PORTAL)
#include <WebCore/StylePinnedAnchorName.h>
#endif

namespace WebCore {
namespace Style {

// <'position-anchor'> = normal | none | auto | <anchor-name>
// https://drafts.csswg.org/css-anchor-position-1/#propdef-position-anchor
//
// With the spatial portal feature enabled, <anchor-name> may carry a '#'-delimited attachment
// point: normal | none | auto | <anchor-name> [ '#' <ident> ]?
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

#if ENABLE(SPATIAL_PORTAL)
    PositionAnchor(PinnedAnchorName&& value)
        : m_value { WTF::move(value) }
    {
    }
#endif

    bool isNormal() const { return WTF::holdsAlternative<CSS::Keyword::Normal>(m_value); }
    bool isNone() const { return WTF::holdsAlternative<CSS::Keyword::None>(m_value); }
    bool isAuto() const { return WTF::holdsAlternative<CSS::Keyword::Auto>(m_value); }

    bool isName() const
    {
#if ENABLE(SPATIAL_PORTAL)
        if (WTF::holdsAlternative<PinnedAnchorName>(m_value))
            return true;
#endif
        return WTF::holdsAlternative<AnchorName>(m_value);
    }

    std::optional<AnchorName> tryName() const
    {
        if (auto* name = std::get_if<AnchorName>(&m_value))
            return *name;
#if ENABLE(SPATIAL_PORTAL)
        if (auto* pinned = std::get_if<PinnedAnchorName>(&m_value))
            return pinned->name;
#endif
        return { };
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    bool operator==(const PositionAnchor&) const = default;

private:
#if ENABLE(SPATIAL_PORTAL)
    using Value = Variant<CSS::Keyword::Normal, CSS::Keyword::None, CSS::Keyword::Auto, AnchorName, PinnedAnchorName>;
#else
    using Value = Variant<CSS::Keyword::Normal, CSS::Keyword::None, CSS::Keyword::Auto, AnchorName>;
#endif

    Value m_value;
};

// MARK: - Conversion

template<> struct CSSValueConversion<PositionAnchor> {
    auto operator()(BuilderState&, const CSSValue&) -> PositionAnchor;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::PositionAnchor)
