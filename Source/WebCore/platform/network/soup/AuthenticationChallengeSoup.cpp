/*
 * Copyright (C) 2012 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY IGALIA S.L. ``AS IS'' AND ANY
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

#if USE(SOUP)

#include "AuthenticationChallenge.h"

#include "ResourceError.h"
#include "URLSoup.h"
#include <libsoup/soup.h>

namespace WebCore {

static ProtectionSpaceServerType protectionSpaceServerTypeFromURL(const URL& url, bool isForProxy)
{
    if (url.protocolIs("https"))
        return isForProxy ? ProtectionSpaceProxyHTTPS : ProtectionSpaceServerHTTPS;
    if (url.protocolIs("http"))
        return isForProxy ? ProtectionSpaceProxyHTTP : ProtectionSpaceServerHTTP;
    if (url.protocolIs("ftp"))
        return isForProxy ? ProtectionSpaceProxyFTP : ProtectionSpaceServerFTP;
    return isForProxy ? ProtectionSpaceProxyHTTP : ProtectionSpaceServerHTTP;
}

static ProtectionSpace protectionSpaceFromSoupAuthAndURL(SoupAuth* soupAuth, const URL& url)
{
    const char* schemeName = soup_auth_get_scheme_name(soupAuth);
    ProtectionSpaceAuthenticationScheme scheme;
    if (!g_ascii_strcasecmp(schemeName, "basic"))
        scheme = ProtectionSpaceAuthenticationSchemeHTTPBasic;
    else if (!g_ascii_strcasecmp(schemeName, "digest"))
        scheme = ProtectionSpaceAuthenticationSchemeHTTPDigest;
    else if (!g_ascii_strcasecmp(schemeName, "ntlm"))
        scheme = ProtectionSpaceAuthenticationSchemeNTLM;
    else if (!g_ascii_strcasecmp(schemeName, "negotiate"))
        scheme = ProtectionSpaceAuthenticationSchemeNegotiate;
    else
        scheme = ProtectionSpaceAuthenticationSchemeUnknown;

#if USE(SOUP2)
    auto host = url.host();
    auto port = url.port();
    if (!port)
        port = defaultPortForProtocol(url.protocol());
#else
    URL authURL({ }, makeString("http://", soup_auth_get_authority(soupAuth)));
    auto host = authURL.host();
    auto port = authURL.port();
#endif

    return ProtectionSpace(host.toString(), static_cast<int>(port.valueOr(0)),
        protectionSpaceServerTypeFromURL(url, soup_auth_is_for_proxy(soupAuth)),
        String::fromUTF8(soup_auth_get_realm(soupAuth)), scheme);
}

AuthenticationChallenge::AuthenticationChallenge(SoupMessage* soupMessage, SoupAuth* soupAuth, bool retrying)
    : AuthenticationChallengeBase(protectionSpaceFromSoupAuthAndURL(soupAuth, soupURIToURL(soup_message_get_uri(soupMessage)))
        , Credential() // proposedCredentials
        , retrying ? 1 : 0 // previousFailureCount
        , soupMessage // failureResponse
        , ResourceError::authenticationError(soupMessage))
#if USE(SOUP2)
    , m_soupMessage(soupMessage)
#endif
    , m_soupAuth(soupAuth)
{
}

bool AuthenticationChallenge::platformCompare(const AuthenticationChallenge& a, const AuthenticationChallenge& b)
{
    if (a.soupAuth() != b.soupAuth())
        return false;

#if USE(SOUP2)
    return a.soupMessage() == b.soupMessage();
#endif

    return true;
}

} // namespace WebCore

#endif
