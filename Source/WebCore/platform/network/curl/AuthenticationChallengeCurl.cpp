/*
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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

AuthenticationChallenge::AuthenticationChallenge(const CurlResponse& curlResponse, unsigned previousFailureCount, const ResourceResponse& response, AuthenticationClient* client)
    : AuthenticationChallengeBase(protectionSpaceFromHandle(curlResponse, response), Credential(), previousFailureCount, response, ResourceError())
    , m_authenticationClient(client)
{
}

ProtectionSpaceServerType AuthenticationChallenge::protectionSpaceServerTypeFromURI(const URL& url)
{
    if (url.protocolIs("https"))
        return ProtectionSpaceServerHTTPS;
    if (url.protocolIs("http"))
        return ProtectionSpaceServerHTTP;
    if (url.protocolIs("ftp"))
        return ProtectionSpaceServerFTP;
    return ProtectionSpaceServerHTTP;
}

ProtectionSpace AuthenticationChallenge::protectionSpaceFromHandle(const CurlResponse& curlResponse, const ResourceResponse& response)
{
    auto port = curlResponse.connectPort;
    auto availableHttpAuth = curlResponse.availableHttpAuth;

    ProtectionSpaceAuthenticationScheme scheme = ProtectionSpaceAuthenticationSchemeUnknown;
    if (availableHttpAuth & CURLAUTH_BASIC)
        scheme = ProtectionSpaceAuthenticationSchemeHTTPBasic;
    if (availableHttpAuth & CURLAUTH_DIGEST)
        scheme = ProtectionSpaceAuthenticationSchemeHTTPDigest;
    if (availableHttpAuth & CURLAUTH_NEGOTIATE)
        scheme = ProtectionSpaceAuthenticationSchemeNegotiate;
    if (availableHttpAuth & CURLAUTH_NTLM)
        scheme = ProtectionSpaceAuthenticationSchemeNTLM;

    String realm;
    const String realmString("realm=");
    auto authHeader = response.httpHeaderField(String("www-authenticate"));
    auto realmPos = authHeader.findIgnoringASCIICase(realmString);
    if (realmPos != notFound) {
        realm = authHeader.substring(realmPos + realmString.length());
        realm = realm.left(realm.find(','));
        removeLeadingAndTrailingQuotes(realm);
    }

    return ProtectionSpace(response.url().host(), static_cast<int>(port), protectionSpaceServerTypeFromURI(response.url()), realm, scheme);
}

void AuthenticationChallenge::removeLeadingAndTrailingQuotes(String& value)
{
    auto length = value.length();
    if (value.startsWith('"') && value.endsWith('"') && length > 1)
        value = value.substring(1, length - 2);
}

} // namespace WebCore

#endif
