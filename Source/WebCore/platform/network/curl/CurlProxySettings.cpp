/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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
#include "CurlProxySettings.h"

#if USE(CURL)

#if PLATFORM(WIN)
#include <winsock2.h>
#endif

#include <curl/curl.h>

namespace WebCore {

static const uint16_t SocksProxyPort = 1080;

static Optional<uint16_t> getProxyPort(const URL&);
static Optional<String> createProxyUrl(const URL&);

CurlProxySettings::CurlProxySettings(URL&& proxyUrl, String&& ignoreHosts)
    : m_mode(Mode::Custom)
    , m_url(WTFMove(proxyUrl))
    , m_ignoreHosts(WTFMove(ignoreHosts))
{
    if (m_url.protocol().isEmpty())
        m_url.setProtocol("http");

    rebuildUrl();
}

void CurlProxySettings::rebuildUrl()
{
    if (auto url = createProxyUrl(m_url))
        m_urlSerializedWithPort = WTFMove(*url);
}

void CurlProxySettings::setUserPass(const String& user, const String& password)
{
    m_url.setUser(user);
    m_url.setPass(password);

    rebuildUrl();
}

static long determineAuthMethod(long authMethod)
{
    if (authMethod & CURLAUTH_NEGOTIATE)
        return CURLAUTH_NEGOTIATE;
    if (authMethod & CURLAUTH_DIGEST)
        return CURLAUTH_DIGEST;
    if (authMethod & CURLAUTH_NTLM)
        return CURLAUTH_NTLM;
    if (authMethod & CURLAUTH_BASIC)
        return CURLAUTH_BASIC;
    return CURLAUTH_NONE;
}

void CurlProxySettings::setAuthMethod(long authMethod)
{
    m_authMethod = determineAuthMethod(authMethod);
}

bool protocolIsInSocksFamily(const URL& url)
{
    return url.protocolIs("socks4") || url.protocolIs("socks4a") || url.protocolIs("socks5") || url.protocolIs("socks5h");
}

static Optional<uint16_t> getProxyPort(const URL& url)
{
    auto port = url.port();
    if (port)
        return port;

    // For Curl port, if port number is not specified for HTTP Proxy protocol, port 80 is used.
    // This differs for libcurl's default port number for HTTP proxy whose value (1080) is for socks.
    if (url.protocolIsInHTTPFamily())
        return defaultPortForProtocol(url.protocol());

    if (protocolIsInSocksFamily(url))
        return SocksProxyPort;

    return WTF::nullopt;
}

static Optional<String> createProxyUrl(const URL &url)
{
    if (url.isEmpty() || url.host().isEmpty())
        return WTF::nullopt;

    if (!url.protocolIsInHTTPFamily() && !protocolIsInSocksFamily(url))
        return WTF::nullopt;

    auto port = getProxyPort(url);
    if (!port)
        return WTF::nullopt;

    auto userpass = (url.hasUsername() || url.hasPassword()) ? makeString(url.user(), ":", url.pass(), "@") : String();
    return makeString(url.protocol(), "://", userpass, url.host(), ":", String::number(*port));
}

}

#endif
