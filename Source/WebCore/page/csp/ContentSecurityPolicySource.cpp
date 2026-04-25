/*
 * Copyright (C) 2011 Google, Inc. All rights reserved.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ContentSecurityPolicySource.h"

#include "ContentSecurityPolicy.h"
#include "SecurityOriginData.h"
#include <pal/text/TextEncoding.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URL.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ContentSecurityPolicySource);

ContentSecurityPolicySource::ContentSecurityPolicySource(const ContentSecurityPolicy& policy, const String& scheme, const String& host, std::optional<uint16_t> port, const String& path, bool hostHasWildcard, bool portHasWildcard, IsSelfSource isSelfSource)
    : m_policy(policy)
    , m_scheme(scheme)
    , m_host(host)
    , m_path(path)
    , m_port(port)
    , m_hostHasWildcard(hostHasWildcard)
    , m_portHasWildcard(portHasWildcard)
    , m_isSelfSource(isSelfSource == IsSelfSource::Yes)
{
}

bool ContentSecurityPolicySource::matches(const URL& url, bool didReceiveRedirectResponse) const
{
    // https://www.w3.org/TR/CSP3/#match-url-to-source-expression.
    auto schemeMatch = schemeMatches(url);
    if (schemeMatch == SchemeMatchResult::NoMatch)
        return false;
    if (isSchemeOnly())
        return true;
    auto shouldUpgradePorts = (schemeMatch == SchemeMatchResult::InsecureUpgradeMatch) ? ShouldUpgradePorts::Yes : ShouldUpgradePorts::No;
    return hostMatches(url) && portMatches(url, shouldUpgradePorts) && (didReceiveRedirectResponse || pathMatches(url));
}

SchemeMatchResult ContentSecurityPolicySource::schemeMatches(const URL& url) const
{
    // https://www.w3.org/TR/CSP3/#match-schemes.
    const auto& scheme = m_scheme.isEmpty() ? m_policy->selfProtocol() : m_scheme;
    auto urlScheme = url.protocol();

    // Step 1.1: A matches B.
    if (scheme == urlScheme)
        return SchemeMatchResult::Match;

    // Step 1.2: A is "http" and B is "https".
    if (scheme == "http"_s && urlScheme == "https"_s)
        return SchemeMatchResult::InsecureUpgradeMatch;

    // Step 1.3: A is "ws" and B is "wss", "http", or "https".
    if (scheme == "ws"_s) {
        if (urlScheme == "wss"_s || urlScheme == "https"_s)
            return SchemeMatchResult::InsecureUpgradeMatch;
        if (urlScheme == "http"_s)
            return SchemeMatchResult::Match;
    }

    // Step 1.4: A is "wss" and B is "https".
    if (scheme == "wss"_s && urlScheme == "https"_s)
        return SchemeMatchResult::Match;

    // self-sources can always upgrade to secure protocols and side-grade insecure protocols. (§6.7.2.8 step 4 NOTE)
    if (m_isSelfSource) {
        if (urlScheme == "https"_s || urlScheme == "wss"_s) {
            if (scheme == "http"_s || scheme == "ws"_s)
                return SchemeMatchResult::InsecureUpgradeMatch;
            return SchemeMatchResult::Match;
        }
        if (scheme == "http"_s && urlScheme == "ws"_s)
            return SchemeMatchResult::Match;
    }

    // Step 2: return "Does Not Match".
    return SchemeMatchResult::NoMatch;
}

static bool NODELETE wildcardMatches(StringView host, const String& hostWithWildcard)
{
    auto hostLength = host.length();
    auto hostWithWildcardLength = hostWithWildcard.length();
    return host.endsWithIgnoringASCIICase(hostWithWildcard)
        && hostLength > hostWithWildcardLength
        && host[hostLength - hostWithWildcardLength - 1] == '.';
}

bool ContentSecurityPolicySource::hostMatches(const URL& url) const
{
    auto host = url.host();
    if (m_hostHasWildcard) {
        if (m_host.isEmpty())
            return true;
        return wildcardMatches(host, m_host);
    }
    return equalIgnoringASCIICase(host, m_host);
}

bool ContentSecurityPolicySource::pathMatches(const URL& url) const
{
    if (m_path.isEmpty())
        return true;

    auto path = PAL::decodeURLEscapeSequences(url.path());

    if (m_path.endsWith('/'))
        return path.startsWith(m_path);

    return path == m_path;
}

bool ContentSecurityPolicySource::portMatches(const URL& url, ShouldUpgradePorts shouldUpgradePorts) const
{
    // https://www.w3.org/TR/CSP3/#match-ports
    // input is the source expression's port-part (m_port).
    // url is the URL being checked against the policy.

    // Step 1: Assert input is null, "*", or a sequence of one or more ASCII digits.
    ASSERT(!(m_portHasWildcard && m_port));

    // Step 2: wildcard port matches any URL.
    if (m_portHasWildcard)
        return true;

    std::optional<uint16_t> urlPort = url.port();

    // Steps 3-4: if normalizedInput equals url's port (including null), return "Matches".
    if (urlPort == m_port)
        return true;

    // When upgrading from an insecure to secure scheme, treat default
    // ports as equivalent since they differ across schemes (80 vs 443).
    if (shouldUpgradePorts == ShouldUpgradePorts::Yes) {
        auto defaultSecurePort = WTF::defaultPortForProtocol("https"_s).value_or(443);
        auto defaultInsecurePort = WTF::defaultPortForProtocol("http"_s).value_or(80);
        bool urlOnSecureDefaultPort = (urlPort == defaultSecurePort) || (!urlPort && (url.protocol() == "https"_s || url.protocol() == "wss"_s));
        bool sourcePortIsUpgradable = !m_port || m_port == defaultInsecurePort || m_port == defaultSecurePort;
        if (urlOnSecureDefaultPort && sourcePortIsUpgradable)
            return true;
    }

    // Step 5: if url's port is null, check if source port is the default for url's scheme.
    if (!urlPort)
        return WTF::isDefaultPortForProtocol(m_port.value(), url.protocol());

    if (!m_port)
        return WTF::isDefaultPortForProtocol(urlPort.value(), url.protocol());

    // Step 6: return "Does Not Match".
    return false;
}

bool ContentSecurityPolicySource::isSchemeOnly() const
{
    return m_host.isEmpty() && !m_hostHasWildcard;
}

ContentSecurityPolicySource::operator SecurityOriginData() const
{
    return { m_scheme, m_host, m_port };
}

} // namespace WebCore
