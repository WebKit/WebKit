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

#include <WebCore/UserContentURLPattern.h>
#include <algorithm>
#include <optional>
#include <span>
#include <wtf/URL.h>
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>

// QuirkMatchPattern is a match pattern style syntax, parsed at runtime, that
// identifies the URLs a served quirk applies to:
//
//     *://www.example.com/*        exactly www.example.com, any path
//     *://*.example.com/*          example.com or any subdomain of it
//     *://example.com/store/*      example.com, paths under /store/
//     *://*/*                      every HTTP-family URL
//     *://*.google.*/search        /search only
//     *://*.google.*/search?*      /search with any query
//     *://amazon.*/*               amazon.com, amazon.co.uk, but not www.amazon.com
//     *://*.amazon.*/*             any of the above, plus their subdomains

namespace WebCore {

enum class URLEnvironment : uint8_t {
    SmallScreen,
    TubularApp,
    LensApp,
    SafariWebApp,
    HomeView,
};

WEBCORE_EXPORT bool evaluateURLEnvironment(URLEnvironment);

enum class IsTopDocument : bool { No, Yes };

class URLMatchContext {
public:
    URLMatchContext(URL url)
        : m_url(WTF::move(url))
    {
    }

    const URL& url() const LIFETIME_BOUND { return m_url; }

    StringView host() const LIFETIME_BOUND { return m_url.host(); }

    WEBCORE_EXPORT const String& domainWithoutPublicSuffix() const LIFETIME_BOUND;

    WEBCORE_EXPORT const String& registrableDomain() const LIFETIME_BOUND;

private:
    const URL m_url;
    mutable std::optional<String> m_registrableDomain;
    mutable std::optional<String> m_domainWithoutPublicSuffix;
};

class QuirkMatchPattern {
public:
    WEBCORE_EXPORT static std::optional<QuirkMatchPattern> parse(StringView);

    WEBCORE_EXPORT explicit QuirkMatchPattern(String&&);

    bool isValid() const { return m_pattern.isValid(); }

    WEBCORE_EXPORT bool matches(const URLMatchContext&) const;

    const String& string() const LIFETIME_BOUND { return m_string; }

    friend bool operator==(const QuirkMatchPattern& a, const QuirkMatchPattern& b) { return a.m_string == b.m_string; }

private:
    QuirkMatchPattern(String&&, UserContentURLPattern&&, bool matchesAnyPublicSuffix);

    String m_string;

    UserContentURLPattern m_pattern;
    bool m_matchesAnyPublicSuffix { false };
};

inline bool anyPatternMatches(std::span<const QuirkMatchPattern> patterns, const URLMatchContext& context)
{
    return std::ranges::any_of(patterns, [&](auto& pattern) {
        return pattern.matches(context);
    });
}

} // namespace WebCore
