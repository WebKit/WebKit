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

#if ENABLE(SPATIAL_PORTAL)

#include <WebCore/StyleString.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <'environment-map'> = auto | none | <string>
// https://webkit.github.io/explainers/css-spatial/Overview.html#propdef-environment-map
//
// Accepts a single name, simplified from the spec's 'auto | none | <string>#'; summing multiple
// maps is not currently supported.
struct EnvironmentMap {
    EnvironmentMap(CSS::Keyword::Auto keyword)
        : m_value { keyword }
    {
    }

    EnvironmentMap(CSS::Keyword::None keyword)
        : m_value { keyword }
    {
    }

    EnvironmentMap(String&& name)
        : m_value { WTF::move(name) }
    {
    }

    bool isAuto() const { return WTF::holdsAlternative<CSS::Keyword::Auto>(m_value); }
    bool isNone() const { return WTF::holdsAlternative<CSS::Keyword::None>(m_value); }

    const String* tryName() const LIFETIME_BOUND { return std::get_if<String>(&m_value); }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    bool operator==(const EnvironmentMap&) const = default;

private:
    Variant<CSS::Keyword::Auto, CSS::Keyword::None, String> m_value;
};

// MARK: - Conversion

template<> struct CSSValueConversion<EnvironmentMap> {
    auto operator()(BuilderState&, const CSSValue&) -> EnvironmentMap;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::EnvironmentMap)

#endif // ENABLE(SPATIAL_PORTAL)
