/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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
#include "ProtectionSpaceBase.h"

#include "ProtectionSpace.h"

#if USE(CFURLCONNECTION)
#include "AuthenticationCF.h"
#include <CFNetwork/CFURLProtectionSpacePriv.h>
#endif

namespace WebCore {
 
// Need to enforce empty, non-null strings due to the pickiness of the String == String operator
// combined with the semantics of the String(NSString*) constructor
ProtectionSpaceBase::ProtectionSpaceBase(const String& host, int port, ServerType serverType, const String& realm, AuthenticationScheme authenticationScheme)
    : m_host(host.length() ? host : emptyString())
    , m_realm(realm.length() ? realm : emptyString())
    , m_port(port)
    , m_serverType(serverType)
    , m_authenticationScheme(authenticationScheme)
{    
}

bool ProtectionSpaceBase::isProxy() const
{
    return m_serverType == ServerType::ProxyHTTP
        || m_serverType == ServerType::ProxyHTTPS
        || m_serverType == ServerType::ProxyFTP
        || m_serverType == ServerType::ProxySOCKS;
}

bool ProtectionSpaceBase::receivesCredentialSecurely() const
{
    return m_serverType == ServerType::HTTPS
        || m_serverType == ServerType::FTPS
        || m_serverType == ServerType::ProxyHTTPS
        || m_authenticationScheme == AuthenticationScheme::HTTPDigest;
}

bool ProtectionSpaceBase::isPasswordBased() const
{
    switch (m_authenticationScheme) {
    case AuthenticationScheme::Default:
    case AuthenticationScheme::HTTPBasic:
    case AuthenticationScheme::HTTPDigest:
    case AuthenticationScheme::HTMLForm:
    case AuthenticationScheme::NTLM:
    case AuthenticationScheme::Negotiate:
    case AuthenticationScheme::OAuth:
#if USE(GLIB)
    case AuthenticationScheme::ClientCertificatePINRequested:
#endif
        return true;
    case AuthenticationScheme::ClientCertificateRequested:
    case AuthenticationScheme::ServerTrustEvaluationRequested:
    case AuthenticationScheme::Unknown:
        return false;
    }

    return true;
}

bool ProtectionSpaceBase::compare(const ProtectionSpace& a, const ProtectionSpace& b)
{
    if (a.host() != b.host())
        return false;
    if (a.port() != b.port())
        return false;
    if (a.serverType() != b.serverType())
        return false;
    // Ignore realm for proxies
    if (!a.isProxy() && a.realm() != b.realm())
        return false;
    if (a.authenticationScheme() != b.authenticationScheme())
        return false;

    return ProtectionSpace::platformCompare(a, b);
}

}
