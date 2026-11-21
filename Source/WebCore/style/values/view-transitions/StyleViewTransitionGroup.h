/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <WebCore/StyleCustomIdent.h>
#include <WebCore/StyleScopeOrdinal.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <'view-transition-group'> = normal | contain | nearest | <custom-ident excluding=normal,contain,nearest,none>
// https://drafts.csswg.org/css-view-transitions-2/#view-transition-group-prop
struct ViewTransitionGroup {
    ViewTransitionGroup(CSS::Keyword::Normal keyword)
        : m_value { keyword }
    {
    }

    ViewTransitionGroup(CSS::Keyword::Contain keyword, ScopeOrdinal ordinal)
        : m_value { keyword }
        , m_scopeOrdinal { ordinal }
    {
    }

    ViewTransitionGroup(CSS::Keyword::Nearest keyword, ScopeOrdinal ordinal)
        : m_value { keyword }
        , m_scopeOrdinal { ordinal }
    {
    }

    ViewTransitionGroup(CustomIdent&& customIdent, ScopeOrdinal ordinal)
        : m_value { WTF::move(customIdent) }
        , m_scopeOrdinal { ordinal }
    {
    }

    bool isNormal() const { return WTF::holdsAlternative<CSS::Keyword::Normal>(m_value); }
    bool isContain() const { return WTF::holdsAlternative<CSS::Keyword::Contain>(m_value); }
    bool isNearest() const { return WTF::holdsAlternative<CSS::Keyword::Nearest>(m_value); }
    bool isCustomIdent() const { return WTF::holdsAlternative<CustomIdent>(m_value); }

    const CustomIdent& customIdent() const { ASSERT(isCustomIdent()); return std::get<CustomIdent>(m_value); }

    ScopeOrdinal scopeOrdinal() const { ASSERT(!isNormal()); return m_scopeOrdinal; }

    template<typename... F> decltype(auto) switchOn(NOESCAPE F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    bool operator==(const ViewTransitionGroup&) const = default;

private:
    Variant<CSS::Keyword::Normal, CSS::Keyword::Contain, CSS::Keyword::Nearest, CustomIdent> m_value;
    ScopeOrdinal m_scopeOrdinal { ScopeOrdinal::Element };
};

// MARK: - Conversion

template<> struct CSSValueConversion<ViewTransitionGroup> {
    auto operator()(BuilderState&, const CSSValue&) -> ViewTransitionGroup;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::ViewTransitionGroup)
