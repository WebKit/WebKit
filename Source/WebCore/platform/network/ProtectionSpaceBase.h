/*
 * Copyright (C) 2007-2021 Apple Inc.  All rights reserved.
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
  
class ProtectionSpaceBase {
public:
    enum class ServerType : uint8_t {
        HTTP = 1,
        HTTPS = 2,
        FTP = 3,
        FTPS = 4,
        ProxyHTTP = 5,
        ProxyHTTPS = 6,
        ProxyFTP = 7,
        ProxySOCKS = 8
    };

    enum class AuthenticationScheme : uint8_t {
        Default = 1,
        HTTPBasic = 2,
        HTTPDigest = 3,
        HTMLForm = 4,
        NTLM = 5,
        Negotiate = 6,
        ClientCertificateRequested = 7,
        ServerTrustEvaluationRequested = 8,
        OAuth = 9,
#if USE(GLIB)
        ClientCertificatePINRequested = 10,
#endif
        Unknown = 100
    };

    bool isHashTableDeletedValue() const { return m_isHashTableDeletedValue; }
    
    const String& host() const { return m_host; }
    int port() const { return m_port; }
    ServerType serverType() const { return m_serverType; }
    WEBCORE_EXPORT bool isProxy() const;
    const String& realm() const { return m_realm; }
    AuthenticationScheme authenticationScheme() const { return m_authenticationScheme; }
    
    WEBCORE_EXPORT bool receivesCredentialSecurely() const;
    WEBCORE_EXPORT bool isPasswordBased() const;

    bool encodingRequiresPlatformData() const { return false; }

    WEBCORE_EXPORT static bool compare(const ProtectionSpace&, const ProtectionSpace&);

protected:
    ProtectionSpaceBase() = default;
    WEBCORE_EXPORT ProtectionSpaceBase(const String& host, int port, ServerType, const String& realm, AuthenticationScheme);

    // Hash table deleted values, which are only constructed and never copied or destroyed.
    ProtectionSpaceBase(WTF::HashTableDeletedValueType) : m_isHashTableDeletedValue(true) { }

    static bool platformCompare(const ProtectionSpace&, const ProtectionSpace&) { return true; }

private:
    // Need to enforce empty, non-null strings due to the pickiness of the String == String operator
    // combined with the semantics of the String(NSString*) constructor
    String m_host { emptyString() };
    String m_realm { emptyString() };

    int m_port { 0 };
    ServerType m_serverType { ServerType::HTTP };
    AuthenticationScheme m_authenticationScheme { AuthenticationScheme::Default };
    bool m_isHashTableDeletedValue { false };
};

inline bool operator==(const ProtectionSpace& a, const ProtectionSpace& b) { return ProtectionSpaceBase::compare(a, b); }
    
} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::ProtectionSpaceBase::AuthenticationScheme> {
    using values = EnumValues<
        WebCore::ProtectionSpaceBase::AuthenticationScheme,
        WebCore::ProtectionSpaceBase::AuthenticationScheme::Default,
        WebCore::ProtectionSpaceBase::AuthenticationScheme::HTTPBasic,
        WebCore::ProtectionSpaceBase::AuthenticationScheme::HTTPDigest,
        WebCore::ProtectionSpaceBase::AuthenticationScheme::HTMLForm,
        WebCore::ProtectionSpaceBase::AuthenticationScheme::NTLM,
        WebCore::ProtectionSpaceBase::AuthenticationScheme::Negotiate,
        WebCore::ProtectionSpaceBase::AuthenticationScheme::ClientCertificateRequested,
        WebCore::ProtectionSpaceBase::AuthenticationScheme::ServerTrustEvaluationRequested,
        WebCore::ProtectionSpaceBase::AuthenticationScheme::OAuth,
        WebCore::ProtectionSpaceBase::AuthenticationScheme::Unknown
    >;
};

template<> struct EnumTraits<WebCore::ProtectionSpaceBase::ServerType> {
    using values = EnumValues<
        WebCore::ProtectionSpaceBase::ServerType,
        WebCore::ProtectionSpaceBase::ServerType::HTTP,
        WebCore::ProtectionSpaceBase::ServerType::HTTPS,
        WebCore::ProtectionSpaceBase::ServerType::FTP,
        WebCore::ProtectionSpaceBase::ServerType::FTPS,
        WebCore::ProtectionSpaceBase::ServerType::ProxyHTTP,
        WebCore::ProtectionSpaceBase::ServerType::ProxyHTTPS,
        WebCore::ProtectionSpaceBase::ServerType::ProxyFTP,
        WebCore::ProtectionSpaceBase::ServerType::ProxySOCKS
    >;
};

} // namespace WTF
