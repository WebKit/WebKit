/*
 * Copyright (C) 2007-2020 Apple Inc.  All rights reserved.
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

#pragma once

#include <wtf/EnumTraits.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ProtectionSpace;

enum ProtectionSpaceServerType {
    ProtectionSpaceServerHTTP = 1,
    ProtectionSpaceServerHTTPS = 2,
    ProtectionSpaceServerFTP = 3,
    ProtectionSpaceServerFTPS = 4,
    ProtectionSpaceProxyHTTP = 5,
    ProtectionSpaceProxyHTTPS = 6,
    ProtectionSpaceProxyFTP = 7,
    ProtectionSpaceProxySOCKS = 8
};

enum ProtectionSpaceAuthenticationScheme {
    ProtectionSpaceAuthenticationSchemeDefault = 1,
    ProtectionSpaceAuthenticationSchemeHTTPBasic = 2,
    ProtectionSpaceAuthenticationSchemeHTTPDigest = 3,
    ProtectionSpaceAuthenticationSchemeHTMLForm = 4,
    ProtectionSpaceAuthenticationSchemeNTLM = 5,
    ProtectionSpaceAuthenticationSchemeNegotiate = 6,
    ProtectionSpaceAuthenticationSchemeClientCertificateRequested = 7,
    ProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested = 8,
    ProtectionSpaceAuthenticationSchemeOAuth = 9,
#if USE(GLIB)
    ProtectionSpaceAuthenticationSchemeClientCertificatePINRequested = 10,
#endif
    ProtectionSpaceAuthenticationSchemeUnknown = 100
};
  
class ProtectionSpaceBase {

public:
    bool isHashTableDeletedValue() const { return m_isHashTableDeletedValue; }
    
    WEBCORE_EXPORT const String& host() const;
    WEBCORE_EXPORT int port() const;
    WEBCORE_EXPORT ProtectionSpaceServerType serverType() const;
    WEBCORE_EXPORT bool isProxy() const;
    WEBCORE_EXPORT const String& realm() const;
    WEBCORE_EXPORT ProtectionSpaceAuthenticationScheme authenticationScheme() const;
    
    WEBCORE_EXPORT bool receivesCredentialSecurely() const;
    WEBCORE_EXPORT bool isPasswordBased() const;

    bool encodingRequiresPlatformData() const { return false; }

    WEBCORE_EXPORT static bool compare(const ProtectionSpace&, const ProtectionSpace&);

protected:
    WEBCORE_EXPORT ProtectionSpaceBase();
    WEBCORE_EXPORT ProtectionSpaceBase(const String& host, int port, ProtectionSpaceServerType, const String& realm, ProtectionSpaceAuthenticationScheme);

    // Hash table deleted values, which are only constructed and never copied or destroyed.
    ProtectionSpaceBase(WTF::HashTableDeletedValueType) : m_isHashTableDeletedValue(true) { }

    static bool platformCompare(const ProtectionSpace&, const ProtectionSpace&) { return true; }

private:
    String m_host;
    int m_port;
    ProtectionSpaceServerType m_serverType;
    String m_realm;
    ProtectionSpaceAuthenticationScheme m_authenticationScheme;
    bool m_isHashTableDeletedValue;
};

inline bool operator==(const ProtectionSpace& a, const ProtectionSpace& b) { return ProtectionSpaceBase::compare(a, b); }
inline bool operator!=(const ProtectionSpace& a, const ProtectionSpace& b) { return !(a == b); }
    
} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::ProtectionSpaceAuthenticationScheme> {
    using values = EnumValues<
        WebCore::ProtectionSpaceAuthenticationScheme,
        WebCore::ProtectionSpaceAuthenticationScheme::ProtectionSpaceAuthenticationSchemeDefault,
        WebCore::ProtectionSpaceAuthenticationScheme::ProtectionSpaceAuthenticationSchemeHTTPBasic,
        WebCore::ProtectionSpaceAuthenticationScheme::ProtectionSpaceAuthenticationSchemeHTTPDigest,
        WebCore::ProtectionSpaceAuthenticationScheme::ProtectionSpaceAuthenticationSchemeHTMLForm,
        WebCore::ProtectionSpaceAuthenticationScheme::ProtectionSpaceAuthenticationSchemeNTLM,
        WebCore::ProtectionSpaceAuthenticationScheme::ProtectionSpaceAuthenticationSchemeNegotiate,
        WebCore::ProtectionSpaceAuthenticationScheme::ProtectionSpaceAuthenticationSchemeClientCertificateRequested,
        WebCore::ProtectionSpaceAuthenticationScheme::ProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested,
        WebCore::ProtectionSpaceAuthenticationScheme::ProtectionSpaceAuthenticationSchemeOAuth,
        WebCore::ProtectionSpaceAuthenticationScheme::ProtectionSpaceAuthenticationSchemeUnknown
    >;
};

template<> struct EnumTraits<WebCore::ProtectionSpaceServerType> {
    using values = EnumValues<
        WebCore::ProtectionSpaceServerType,
        WebCore::ProtectionSpaceServerType::ProtectionSpaceServerHTTP,
        WebCore::ProtectionSpaceServerType::ProtectionSpaceServerHTTPS,
        WebCore::ProtectionSpaceServerType::ProtectionSpaceServerFTP,
        WebCore::ProtectionSpaceServerType::ProtectionSpaceServerFTPS,
        WebCore::ProtectionSpaceServerType::ProtectionSpaceProxyHTTP,
        WebCore::ProtectionSpaceServerType::ProtectionSpaceProxyHTTPS,
        WebCore::ProtectionSpaceServerType::ProtectionSpaceProxyFTP,
        WebCore::ProtectionSpaceServerType::ProtectionSpaceProxySOCKS
    >;
};

} // namespace WTF
