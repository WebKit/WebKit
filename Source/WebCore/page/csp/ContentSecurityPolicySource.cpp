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
#include "URL.h"

namespace WebCore {

ContentSecurityPolicySource::ContentSecurityPolicySource(const ContentSecurityPolicy& policy, const String& scheme, const String& host, int port, const String& path, bool hostHasWildcard, bool portHasWildcard)
    : m_policy(policy)
    , m_scheme(scheme)
    , m_host(host)
    , m_port(port)
    , m_path(path)
    , m_hostHasWildcard(hostHasWildcard)
    , m_portHasWildcard(portHasWildcard)
{
}

bool ContentSecurityPolicySource::matches(const URL& url, bool didReceiveRedirectResponse) const
{
    if (!schemeMatches(url))
        return false;
    if (isSchemeOnly())
        return true;
    return hostMatches(url) && portMatches(url) && (didReceiveRedirectResponse || pathMatches(url));
}

bool ContentSecurityPolicySource::schemeMatches(const URL& url) const
{
    if (m_scheme.isEmpty())
        return m_policy.protocolMatchesSelf(url);
    return equalIgnoringASCIICase(url.protocol(), m_scheme);
}

bool ContentSecurityPolicySource::hostMatches(const URL& url) const
{
    const String& host = url.host();
    if (equalIgnoringASCIICase(host, m_host))
        return true;
    return m_hostHasWildcard && host.endsWith("." + m_host, false);

}

bool ContentSecurityPolicySource::pathMatches(const URL& url) const
{
    if (m_path.isEmpty())
        return true;

    String path = decodeURLEscapeSequences(url.path());

    if (m_path.endsWith("/"))
        return path.startsWith(m_path);

    return path == m_path;
}

bool ContentSecurityPolicySource::portMatches(const URL& url) const
{
    if (m_portHasWildcard)
        return true;

    int port = url.port();

    if (port == m_port)
        return true;

    if (!port)
        return isDefaultPortForProtocol(m_port, url.protocol());

    if (!m_port)
        return isDefaultPortForProtocol(port, url.protocol());

    return false;
}

bool ContentSecurityPolicySource::isSchemeOnly() const
{
    return m_host.isEmpty();
}

} // namespace WebCore
