/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include <WebCore/StyleScopeOrdinal.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <'view-transition-name> = none | auto | match-element | <custom-ident excluding=none,auto,match-element>
// https://www.w3.org/TR/css-view-transitions-1/#propdef-view-transition-name
struct ViewTransitionName {
    ViewTransitionName(CSS::Keyword::None keyword)
        : m_value { keyword }
    {
    }

    ViewTransitionName(CSS::Keyword::Auto keyword, ScopeOrdinal ordinal)
        : m_value { keyword }
        , m_scopeOrdinal { ordinal }
    {
    }

    ViewTransitionName(CSS::Keyword::MatchElement keyword, ScopeOrdinal ordinal)
        : m_value { keyword }
        , m_scopeOrdinal { ordinal }
    {
    }

    ViewTransitionName(CustomIdentifier&& customIdentifier, ScopeOrdinal ordinal)
        : m_value { WTFMove(customIdentifier) }
        , m_scopeOrdinal { ordinal }
    {
    }

    bool isNone() const { return WTF::holdsAlternative<CSS::Keyword::None>(m_value); }
    bool isAuto() const { return WTF::holdsAlternative<CSS::Keyword::Auto>(m_value); }
    bool isMatchElement() const { return WTF::holdsAlternative<CSS::Keyword::MatchElement>(m_value); }
    bool isCustomIdentifier() const { return WTF::holdsAlternative<CustomIdentifier>(m_value); }

    ScopeOrdinal scopeOrdinal() const { ASSERT(!isNone()); return m_scopeOrdinal; }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    bool operator==(const ViewTransitionName&) const = default;

private:
    Variant<CSS::Keyword::None, CSS::Keyword::Auto, CSS::Keyword::MatchElement, CustomIdentifier> m_value;
    ScopeOrdinal m_scopeOrdinal { ScopeOrdinal::Element };
};

// MARK: - Conversion

template<> struct CSSValueConversion<ViewTransitionName> { auto operator()(BuilderState&, const CSSValue&) -> ViewTransitionName; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::ViewTransitionName)
