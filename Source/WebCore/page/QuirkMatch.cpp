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

const String& QuirkMatchContext::registrableDomain() const
{
    if (!m_registrableDomain)
        m_registrableDomain = RegistrableDomain { m_url }.string();
    return *m_registrableDomain;
}

const String& QuirkMatchContext::domainWithoutPublicSuffix() const
{
    if (!m_domainWithoutPublicSuffix)
        m_domainWithoutPublicSuffix = PublicSuffixStore::singleton().domainWithoutPublicSuffix(registrableDomain());
    return *m_domainWithoutPublicSuffix;
}

bool QuirkMatch::RefinementSet::matchesPathPattern(const URL& url) const
{
    switch (pathComparison) {
    case PathComparison::PathContains:
        return url.path().contains(pathPattern);
    case PathComparison::PathStartsWith:
        return startsWithLettersIgnoringASCIICase(url.path(), pathPattern);
    case PathComparison::PathOrFragmentContains:
        return url.path().contains(pathPattern) || url.fragmentIdentifier().contains(pathPattern);
    case PathComparison::LastPathComponentIs:
        return url.lastPathComponent() == pathPattern;
    case PathComparison::LastPathComponentStartsWith:
        return url.lastPathComponent().startsWith(pathPattern);
    case PathComparison::LastPathComponentEndsWith:
        return url.lastPathComponent().endsWith(pathPattern);
    }

    ASSERT_NOT_REACHED();
    return false;
}

bool QuirkMatch::RefinementSet::matches(const QuirkMatchContext& context) const
{
    if (!pathPattern.isNull() && !matchesPathPattern(context.url()))
        return false;

    if (!hosts.isEmpty() && !hosts.contains(context.host()))
        return false;

    if (environment && !evaluateQuirkEnvironment(*environment))
        return false;

    return true;
}

bool QuirkMatch::matchesIdentity(const QuirkMatchContext& context) const
{
    switch (m_kind) {
    case Kind::Domain:
        return m_patterns.contains(context.registrableDomain());
    case Kind::Host:
        return m_patterns.contains(context.host());
    case Kind::HostOrSubdomainOf:
        return m_patterns.containsMatching([&](ASCIILiteral pattern) { return context.url().isMatchingDomain(pattern); });
    case Kind::AnyTopLevelDomain:
        return m_patterns.contains(context.domainWithoutPublicSuffix());
    case Kind::Any:
        // about:, data:, and other URLs without a host are not sites.
        return !context.host().isEmpty();
    }

    ASSERT_NOT_REACHED();
    return false;
}

bool QuirkMatch::matches(const QuirkMatchContext& context) const
{
    if (!matchesIdentity(context)) [[likely]]
        return false;

    if (!m_refinements.matches(context))
        return false;

    if (m_exception && m_exception->matches(context))
        return false;

    return true;
}

} // namespace WebCore
