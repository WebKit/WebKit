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
ProtectionSpaceBase::ProtectionSpaceBase()
    : m_host(emptyString())
    , m_port(0)
    , m_serverType(ProtectionSpaceServerHTTP)
    , m_realm(emptyString())
    , m_authenticationScheme(ProtectionSpaceAuthenticationSchemeDefault)
    , m_isHashTableDeletedValue(false)
{
}
 
// Need to enforce empty, non-null strings due to the pickiness of the String == String operator
// combined with the semantics of the String(NSString*) constructor
ProtectionSpaceBase::ProtectionSpaceBase(const String& host, int port, ProtectionSpaceServerType serverType, const String& realm, ProtectionSpaceAuthenticationScheme authenticationScheme)
    : m_host(host.length() ? host : emptyString())
    , m_port(port)
    , m_serverType(serverType)
    , m_realm(realm.length() ? realm : emptyString())
    , m_authenticationScheme(authenticationScheme)
    , m_isHashTableDeletedValue(false)
{    
}
    
const String& ProtectionSpaceBase::host() const
{
    return m_host; 
}

int ProtectionSpaceBase::port() const
{
    return m_port; 
}

ProtectionSpaceServerType ProtectionSpaceBase::serverType() const
{
    return m_serverType;
}

bool ProtectionSpaceBase::isProxy() const
{
    return (m_serverType == ProtectionSpaceProxyHTTP ||
            m_serverType == ProtectionSpaceProxyHTTPS ||
            m_serverType == ProtectionSpaceProxyFTP ||
            m_serverType == ProtectionSpaceProxySOCKS);
}

const String& ProtectionSpaceBase::realm() const
{ 
    return m_realm; 
}

ProtectionSpaceAuthenticationScheme ProtectionSpaceBase::authenticationScheme() const
{ 
    return m_authenticationScheme; 
}

bool ProtectionSpaceBase::receivesCredentialSecurely() const
{
    return (m_serverType == ProtectionSpaceServerHTTPS ||
            m_serverType == ProtectionSpaceServerFTPS || 
            m_serverType == ProtectionSpaceProxyHTTPS || 
            m_authenticationScheme == ProtectionSpaceAuthenticationSchemeHTTPDigest); 
}

bool ProtectionSpaceBase::isPasswordBased() const
{
    switch (m_authenticationScheme) {
    case ProtectionSpaceAuthenticationSchemeDefault:
    case ProtectionSpaceAuthenticationSchemeHTTPBasic:
    case ProtectionSpaceAuthenticationSchemeHTTPDigest:
    case ProtectionSpaceAuthenticationSchemeHTMLForm:
    case ProtectionSpaceAuthenticationSchemeNTLM:
    case ProtectionSpaceAuthenticationSchemeNegotiate:
    case ProtectionSpaceAuthenticationSchemeOAuth:
        return true;
    case ProtectionSpaceAuthenticationSchemeClientCertificateRequested:
    case ProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested:
    case ProtectionSpaceAuthenticationSchemeUnknown:
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
