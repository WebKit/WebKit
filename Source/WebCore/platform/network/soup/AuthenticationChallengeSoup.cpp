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

static ProtectionSpace::ServerType protectionSpaceServerTypeFromURL(const URL& url, bool isForProxy)
{
    if (url.protocolIs("https"_s) || url.protocolIs("wss"_s))
        return isForProxy ? ProtectionSpace::ServerType::ProxyHTTPS : ProtectionSpace::ServerType::HTTPS;
    if (url.protocolIs("http"_s) || url.protocolIs("ws"_s))
        return isForProxy ? ProtectionSpace::ServerType::ProxyHTTP : ProtectionSpace::ServerType::HTTP;
    if (url.protocolIs("ftp"_s))
        return isForProxy ? ProtectionSpace::ServerType::ProxyFTP : ProtectionSpace::ServerType::FTP;
    return isForProxy ? ProtectionSpace::ServerType::ProxyHTTP : ProtectionSpace::ServerType::HTTP;
}

static ProtectionSpace protectionSpaceFromSoupAuthAndURL(SoupAuth* soupAuth, const URL& url)
{
    const char* schemeName = soup_auth_get_scheme_name(soupAuth);
    ProtectionSpace::AuthenticationScheme scheme;
    if (!g_ascii_strcasecmp(schemeName, "basic"))
        scheme = ProtectionSpace::AuthenticationScheme::HTTPBasic;
    else if (!g_ascii_strcasecmp(schemeName, "digest"))
        scheme = ProtectionSpace::AuthenticationScheme::HTTPDigest;
    else if (!g_ascii_strcasecmp(schemeName, "ntlm"))
        scheme = ProtectionSpace::AuthenticationScheme::NTLM;
    else if (!g_ascii_strcasecmp(schemeName, "negotiate"))
        scheme = ProtectionSpace::AuthenticationScheme::Negotiate;
    else
        scheme = ProtectionSpace::AuthenticationScheme::Unknown;

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

    return ProtectionSpace(host.toString(), static_cast<int>(port.value_or(0)),
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

ProtectionSpace AuthenticationChallenge::protectionSpaceForClientCertificate(const URL& url)
{
    auto port = url.port();
    if (!port)
        port = defaultPortForProtocol(url.protocol());
    return ProtectionSpace(url.host().toString(), static_cast<int>(port.value_or(0)), protectionSpaceServerTypeFromURL(url, false), { },
        ProtectionSpace::AuthenticationScheme::ClientCertificateRequested);
}

AuthenticationChallenge::AuthenticationChallenge(SoupMessage* soupMessage, GTlsClientConnection*)
    : AuthenticationChallengeBase(protectionSpaceForClientCertificate(soupURIToURL(soup_message_get_uri(soupMessage)))
        , Credential() // proposedCredentials
        , 0 // previousFailureCount
        , soupMessage // failureResponse
        , ResourceError::authenticationError(soupMessage))
{
}

ProtectionSpace AuthenticationChallenge::protectionSpaceForClientCertificatePassword(const URL& url, GTlsPassword* tlsPassword)
{
    auto port = url.port();
    if (!port)
        port = defaultPortForProtocol(url.protocol());
    return ProtectionSpace(url.host().toString(), static_cast<int>(port.value_or(0)), protectionSpaceServerTypeFromURL(url, false),
        String::fromUTF8(g_tls_password_get_description(tlsPassword)), ProtectionSpace::AuthenticationScheme::ClientCertificatePINRequested);
}

AuthenticationChallenge::AuthenticationChallenge(SoupMessage* soupMessage, GTlsPassword* tlsPassword)
    : AuthenticationChallengeBase(protectionSpaceForClientCertificatePassword(soupURIToURL(soup_message_get_uri(soupMessage)), tlsPassword)
        , Credential() // proposedCredentials
        , g_tls_password_get_flags(tlsPassword) & G_TLS_PASSWORD_RETRY ? 1 : 0 // previousFailureCount
        , soupMessage // failureResponse
        , ResourceError::authenticationError(soupMessage))
    , m_tlsPassword(tlsPassword)
    , m_tlsPasswordFlags(tlsPassword ? g_tls_password_get_flags(tlsPassword) : G_TLS_PASSWORD_NONE)
{
}

bool AuthenticationChallenge::platformCompare(const AuthenticationChallenge& a, const AuthenticationChallenge& b)
{
    if (a.soupAuth() != b.soupAuth())
        return false;

    if (a.tlsPassword() != b.tlsPassword())
        return false;

    if (a.tlsPasswordFlags() != b.tlsPasswordFlags())
        return false;

#if USE(SOUP2)
    return a.soupMessage() == b.soupMessage();
#endif

    return true;
}

} // namespace WebCore

#endif
