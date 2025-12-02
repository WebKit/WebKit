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

#include <WebCore/StyleValueTypes.h>
#include <wtf/text/AtomString.h>

namespace WebCore {
namespace Style {

// <'-webkit-locale'> = auto | <string>
// NOTE: There is no standard associated with this property.
struct WebkitLocale {
    WebkitLocale(CSS::Keyword::Auto) : m_platform { nullAtom() } { }
    WebkitLocale(const AtomString& value) : m_platform { value } { }
    WebkitLocale(AtomString&& value) : m_platform { WTFMove(value) } { }

    const AtomString& platform() const LIFETIME_BOUND { return m_platform; }
    AtomString takePlatform() { return WTFMove(m_platform); }

    bool isAuto() const { return m_platform.isNull(); }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (isAuto())
            return visitor(CSS::Keyword::Auto { });

        // FIXME: It seems wrong that we extract/serialize the value as a <custom-ident>, given it is parsed as a <string>, but this maintains existing behavior. See https://bugs.webkit.org/show_bug.cgi?id=302724.
        return visitor(CustomIdentifier { m_platform });
    }

    bool operator==(const WebkitLocale&) const = default;

private:
    AtomString m_platform;
};

// MARK: - Conversion

template<> struct CSSValueConversion<WebkitLocale> { auto operator()(BuilderState&, const CSSValue&) -> WebkitLocale; };

// MARK: - Platform

template<> struct ToPlatform<WebkitLocale> {
    auto operator()(const WebkitLocale& value LIFETIME_BOUND) -> const AtomString&
    {
        return value.platform();
    }
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::WebkitLocale)
