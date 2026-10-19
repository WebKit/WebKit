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

#include "config.h"
#include "QuirkMatchPattern.h"

#include "PublicSuffixStore.h"
#include "RegistrableDomain.h"
#include <wtf/text/MakeString.h>

#if PLATFORM(IOS_FAMILY)
#include <pal/system/ios/UserInterfaceIdiom.h>
#endif

#if PLATFORM(COCOA)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

namespace WebCore {

const String& URLMatchContext::registrableDomain() const
{
    if (!m_registrableDomain)
        m_registrableDomain = RegistrableDomain { m_url }.string();
    return *m_registrableDomain;
}

const String& URLMatchContext::domainWithoutPublicSuffix() const
{
    if (!m_domainWithoutPublicSuffix)
        m_domainWithoutPublicSuffix = PublicSuffixStore::singleton().domainWithoutPublicSuffix(registrableDomain());
    return *m_domainWithoutPublicSuffix;
}

bool evaluateURLEnvironment(URLEnvironment environment)
{
    switch (environment) {
    case URLEnvironment::SmallScreen:
#if PLATFORM(IOS_FAMILY)
        return PAL::currentUserInterfaceIdiomIsSmallScreen();
#else
        return false;
#endif
    case URLEnvironment::TubularApp:
#if PLATFORM(IOS_FAMILY)
        return WTF::IOSApplication::isTubular();
#else
        return false;
#endif
    case URLEnvironment::LensApp:
#if PLATFORM(IOS_FAMILY)
        return WTF::IOSApplication::isLensApp();
#else
        return false;
#endif
    case URLEnvironment::SafariWebApp:
#if PLATFORM(MAC)
        return WTF::MacApplication::isSafariWebApp();
#elif PLATFORM(IOS_FAMILY)
        return WTF::IOSApplication::isAppleWebApp();
#else
        return false;
#endif
    }

    ASSERT_NOT_REACHED();
    return false;
}

QuirkMatchPattern::QuirkMatchPattern(String&& string, UserContentURLPattern&& pattern, bool matchesAnyPublicSuffix)
    : m_string(WTF::move(string))
    , m_pattern(WTF::move(pattern))
    , m_matchesAnyPublicSuffix(matchesAnyPublicSuffix)
{
}

QuirkMatchPattern::QuirkMatchPattern(String&& string)
    : m_string(WTF::move(string))
{
    if (auto parsed = parse(m_string)) {
        m_pattern = WTF::move(parsed->m_pattern);
        m_matchesAnyPublicSuffix = parsed->m_matchesAnyPublicSuffix;
    }
}

std::optional<QuirkMatchPattern> QuirkMatchPattern::parse(StringView string)
{
    static constexpr auto anyPublicSuffixAndPathStart = ".*/"_s;

    UserContentURLPattern pattern { string };

    bool matchesAnyPublicSuffix = false;
    if (pattern.error() == UserContentURLPattern::Error::InvalidHost) {
        auto suffixStart = string.find(anyPublicSuffixAndPathStart);
        if (suffixStart == notFound)
            return std::nullopt;
        pattern = UserContentURLPattern { makeString(string.left(suffixStart), string.substring(suffixStart + 2)) };
        matchesAnyPublicSuffix = true;
    }

    if (!pattern.isValid())
        return std::nullopt;

    auto& scheme = pattern.scheme();
    if (scheme != "*"_s && scheme != "http"_s && scheme != "https"_s)
        return std::nullopt;

    if (pattern.host().isEmpty() && !pattern.matchAllHosts())
        return std::nullopt;

    if (matchesAnyPublicSuffix && (pattern.host().isEmpty() || pattern.host().contains('.') || pattern.host().startsWith('[')))
        return std::nullopt;

    return QuirkMatchPattern { string.toString(), WTF::move(pattern), matchesAnyPublicSuffix };
}

static bool matchesPathAndQuery(const UserContentURLPattern& pattern, const URL& url)
{
    return matchesWildcardPattern(pattern.path(), url.viewWithoutFragmentIdentifier().substring(url.pathStart()).toStringWithoutCopying());
}

bool QuirkMatchPattern::matches(const URLMatchContext& context) const
{
    if (!isValid())
        return false;

    auto& url = context.url();
    if (!m_pattern.matchesScheme(url))
        return false;

    if (!m_matchesAnyPublicSuffix)
        return m_pattern.matchesHost(url) && matchesPathAndQuery(m_pattern, url);

    if (!matchesPathAndQuery(m_pattern, url))
        return false;

    if (!equalIgnoringASCIICase(context.domainWithoutPublicSuffix(), m_pattern.host()))
        return false;

    return m_pattern.matchSubdomains() || context.host() == context.registrableDomain();
}

} // namespace WebCore
