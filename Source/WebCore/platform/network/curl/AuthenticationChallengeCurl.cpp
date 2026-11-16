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
#include "AuthenticationChallenge.h"

#if USE(CURL)

#include "CurlContext.h"
#include "CurlResponse.h"
#include "ResourceError.h"

namespace WebCore {

AuthenticationChallenge::AuthenticationChallenge(const CurlResponse& curlResponse, unsigned previousFailureCount, const ResourceResponse& response)
    : AuthenticationChallengeBase(protectionSpaceForPasswordBased(curlResponse, response), Credential(), previousFailureCount, response, ResourceError())
{
}

AuthenticationChallenge::AuthenticationChallenge(const URL& url, const CertificateInfo& certificateInfo, const ResourceError& resourceError)
    : AuthenticationChallengeBase(protectionSpaceForServerTrust(url, certificateInfo), Credential(), 0, ResourceResponse(), resourceError)
{
}

ProtectionSpace::ServerType AuthenticationChallenge::protectionSpaceServerTypeFromURI(const URL& url, bool isForProxy)
{
    if (url.protocolIs("https"_s))
        return isForProxy ? ProtectionSpace::ServerType::ProxyHTTPS : ProtectionSpace::ServerType::HTTPS;
    if (url.protocolIs("http"_s))
        return isForProxy ? ProtectionSpace::ServerType::ProxyHTTP : ProtectionSpace::ServerType::HTTP;
    if (url.protocolIs("ftp"_s))
        return isForProxy ? ProtectionSpace::ServerType::ProxyFTP : ProtectionSpace::ServerType::FTP;
    return isForProxy ? ProtectionSpace::ServerType::ProxyHTTP : ProtectionSpace::ServerType::HTTP;
}

ProtectionSpace AuthenticationChallenge::protectionSpaceForPasswordBased(const CurlResponse& curlResponse, const ResourceResponse& response)
{
    if (!response.isUnauthorized() && !response.isProxyAuthenticationRequired())
        return ProtectionSpace();

    auto isProxyAuth = response.isProxyAuthenticationRequired();
    const auto& url = isProxyAuth ? curlResponse.proxyUrl : response.url();
    auto port = determineProxyPort(url);
    auto serverType = protectionSpaceServerTypeFromURI(url, isProxyAuth);
    auto authenticationScheme = authenticationSchemeFromCurlAuth(isProxyAuth ? curlResponse.availableProxyAuth : curlResponse.availableHttpAuth);

    return ProtectionSpace(url.host().toString(), static_cast<int>(port.value_or(0)), serverType, parseRealm(response), authenticationScheme);
}

ProtectionSpace AuthenticationChallenge::protectionSpaceForServerTrust(const URL& url, const CertificateInfo& certificateInfo)
{
    auto port = determineProxyPort(url);
    auto serverType = protectionSpaceServerTypeFromURI(url, false);
    auto authenticationScheme = ProtectionSpace::AuthenticationScheme::ServerTrustEvaluationRequested;

    return ProtectionSpace(url.host().toString(), static_cast<int>(port.value_or(0)), serverType, String(), authenticationScheme, certificateInfo);
}

std::optional<uint16_t> AuthenticationChallenge::determineProxyPort(const URL& url)
{
    static const uint16_t socksPort = 1080;

    if (auto port = url.port())
        return *port;

    if (auto port = defaultPortForProtocol(url.protocol()))
        return *port;

    if (protocolIsInSocksFamily(url))
        return socksPort;

    return std::nullopt;
}

ProtectionSpace::AuthenticationScheme AuthenticationChallenge::authenticationSchemeFromCurlAuth(long curlAuth)
{
    if (curlAuth & CURLAUTH_NTLM)
        return ProtectionSpace::AuthenticationScheme::NTLM;
    if (curlAuth & CURLAUTH_NEGOTIATE)
        return ProtectionSpace::AuthenticationScheme::Negotiate;
    if (curlAuth & CURLAUTH_DIGEST)
        return ProtectionSpace::AuthenticationScheme::HTTPDigest;
    if (curlAuth & CURLAUTH_BASIC)
        return ProtectionSpace::AuthenticationScheme::HTTPBasic;
    return ProtectionSpace::AuthenticationScheme::Unknown;
}

String AuthenticationChallenge::parseRealm(const ResourceResponse& response)
{
    static constexpr auto wwwAuthenticate = "www-authenticate"_s;
    static constexpr auto proxyAuthenticate = "proxy-authenticate"_s;
    static NeverDestroyed<String> realmString(MAKE_STATIC_STRING_IMPL("realm="));

    String realm;
    auto authHeader = response.httpHeaderField(StringView { response.isUnauthorized() ? wwwAuthenticate : proxyAuthenticate });
    auto realmPos = authHeader.findIgnoringASCIICase(realmString.get());
    if (realmPos != notFound) {
        realm = authHeader.substring(realmPos + realmString.get().length());
        realm = realm.left(realm.find(','));
        removeLeadingAndTrailingQuotes(realm);
    }
    return realm;
}

void AuthenticationChallenge::removeLeadingAndTrailingQuotes(String& value)
{
    auto length = value.length();
    if (value.startsWith('"') && value.endsWith('"') && length > 1)
        value = value.substring(1, length - 2);
}

} // namespace WebCore

#endif
