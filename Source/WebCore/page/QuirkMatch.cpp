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
#include "QuirkMatch.h"

#include "PublicSuffixStore.h"
#include "RegistrableDomain.h"

namespace WebCore {

const String& QuirkMatchContext::topRegistrableDomain() const
{
    if (!m_topRegistrableDomain)
        m_topRegistrableDomain = RegistrableDomain { m_topURL }.string();
    return *m_topRegistrableDomain;
}

const String& QuirkMatchContext::topDomainWithoutPublicSuffix() const
{
    if (!m_topDomainWithoutPublicSuffix)
        m_topDomainWithoutPublicSuffix = PublicSuffixStore::singleton().domainWithoutPublicSuffix(topRegistrableDomain());
    return *m_topDomainWithoutPublicSuffix;
}

const String& QuirkMatchContext::documentRegistrableDomain() const
{
    if (!m_documentRegistrableDomain)
        m_documentRegistrableDomain = RegistrableDomain { m_documentURL }.string();
    return *m_documentRegistrableDomain;
}

bool QuirkMatch::RefinementSet::matchesPathPattern(const URL& topURL) const
{
    switch (pathComparison) {
    case PathComparison::PathContains:
        return topURL.path().contains(pathPattern);
    case PathComparison::PathStartsWith:
        return startsWithLettersIgnoringASCIICase(topURL.path(), pathPattern);
    case PathComparison::PathOrFragmentContains:
        return topURL.path().contains(pathPattern) || topURL.fragmentIdentifier().contains(pathPattern);
    }

    ASSERT_NOT_REACHED();
    return false;
}

bool QuirkMatch::RefinementSet::matches(const QuirkMatchContext& context) const
{
    if (!pathPattern.isNull() && !matchesPathPattern(context.topURL()))
        return false;

    if (environment && !evaluateQuirkEnvironment(*environment))
        return false;

    if (requiresEmbeddedDocument && context.isTopDocument())
        return false;

    if (!documentDomains.isEmpty() && !documentDomains.contains(context.documentRegistrableDomain()))
        return false;

    if (!hosts.isEmpty() && !hosts.contains(context.topHost()))
        return false;

    return true;
}

bool QuirkMatch::matchesSite(const QuirkMatchContext& context) const
{
    switch (m_kind) {
    case Kind::Domain:
        return m_patterns.contains(context.topRegistrableDomain());
    case Kind::Host:
        return m_patterns.contains(context.topHost());
    case Kind::HostOrSubdomainOf:
        return m_patterns.containsMatching([&](ASCIILiteral pattern) { return context.topURL().isMatchingDomain(pattern); });
    case Kind::AnyTopLevelDomain:
        return m_patterns.contains(context.topDomainWithoutPublicSuffix());
    case Kind::AnySite:
        // about:, data:, and other URLs without a host are not sites.
        return !context.topHost().isEmpty();
    }

    ASSERT_NOT_REACHED();
    return false;
}

bool QuirkMatch::matches(const QuirkMatchContext& context) const
{
    if (!matchesSite(context)) [[likely]]
        return false;

    if (!m_refinements.matches(context))
        return false;

    if (m_exception && m_exception->matches(context))
        return false;

    return true;
}

} // namespace WebCore
