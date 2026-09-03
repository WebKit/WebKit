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
#include "URLMatch.h"

#include "PublicSuffixStore.h"
#include "RegistrableDomain.h"

#if PLATFORM(IOS_FAMILY)
#include <pal/system/ios/UserInterfaceIdiom.h>
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
    }

    ASSERT_NOT_REACHED();
    return false;
}

bool URLMatch::RefinementSet::matchesPathPattern(const URL& url) const
{
    switch (pathComparison) {
    case PathComparison::PathContains:
        return url.path().contains(pathPattern);
    case PathComparison::PathStartsWith:
        return startsWithLettersIgnoringASCIICase(url.path(), pathPattern);
    case PathComparison::PathIs:
        return url.path() == pathPattern;
    case PathComparison::PathOrFragmentContains:
        return url.path().contains(pathPattern) || url.fragmentIdentifier().contains(pathPattern);
    }

    ASSERT_NOT_REACHED();
    return false;
}

bool URLMatch::RefinementSet::matches(const URLMatchContext& context) const
{
    if (!pathPattern.isNull() && !matchesPathPattern(context.url()))
        return false;

    if (!queryPattern.isNull() && !context.url().query().contains(queryPattern))
        return false;

    if (environment && !evaluateURLEnvironment(*environment))
        return false;

    if (!hosts.isEmpty() && !hosts.contains(context.host()))
        return false;

    return true;
}

bool URLMatch::matchesURL(const URLMatchContext& context) const
{
    switch (m_kind) {
    case Kind::Domain:
        return m_patterns.contains(context.registrableDomain());
    case Kind::Host:
        return m_patterns.contains(context.host());
    case Kind::HostOrSubdomainOf:
        return m_patterns.containsMatching([&](ASCIILiteral pattern) {
            return context.url().isMatchingDomain(pattern);
        });
    case Kind::AnyTopLevelDomain:
        return m_patterns.contains(context.domainWithoutPublicSuffix());
    case Kind::Any:
        // about:, data:, and other URLs without a host are never matched.
        return !context.host().isEmpty();
    }

    ASSERT_NOT_REACHED();
    return false;
}

bool URLMatch::matches(const URLMatchContext& context) const
{
    if (!matchesURL(context)) [[likely]]
        return false;

    if (!m_refinements.matches(context))
        return false;

    if (m_exception && m_exception->matches(context))
        return false;

    return true;
}

} // namespace WebCore
