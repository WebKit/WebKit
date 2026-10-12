/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/CertificateInfo.h>
#include <WebCore/DigitalCredentialPresentationProtocol.h>
#include <algorithm>
#include <optional>
#include <wtf/Variant.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

enum class OpenID4VPCredentialFormat : uint8_t {
    MsoMdoc,
    DcSdJwt,
    Unknown,
};

enum class OpenID4VPTrustedAuthorityType : uint8_t {
    AuthorityKeyIdentifier,
    ETSITrustedList,
    OpenIDFederation,
};

struct OpenID4VPTrustedAuthoritiesQuery {
    OpenID4VPTrustedAuthorityType type;
    Vector<String> values;
};

struct OpenID4VPAllArrayElements { };

using OpenID4VPClaimPathComponent = Variant<String, OpenID4VPAllArrayElements, uint64_t>;

using OpenID4VPClaimValue = Variant<String, int64_t, bool>;

struct OpenID4VPClaimsQuery {
    String identifier;
    Vector<OpenID4VPClaimPathComponent> path;
    Vector<OpenID4VPClaimValue> values;
    std::optional<bool> intentToRetain;
};

struct OpenID4VPCredentialQuery {
    String identifier;
    OpenID4VPCredentialFormat format;
    bool allowsMultiple { false };
    String documentType;
    Vector<String> verifiableCredentialTypes;
    Vector<OpenID4VPTrustedAuthoritiesQuery> trustedAuthorities;
    bool requiresCryptographicHolderBinding { true };
    Vector<OpenID4VPClaimsQuery> claims;
    Vector<Vector<String>> claimSets;
};

struct OpenID4VPCredentialSetQuery {
    Vector<Vector<String>> options;
    bool isRequired { true };
};

struct OpenID4VPVerifierIdentity {
    String clientIdentifierPrefix;
    String identifier;
    CertificateInfo certificateChain;
};

struct ValidatedOpenID4VPRequestObject {
    Vector<OpenID4VPCredentialQuery> credentials;
    Vector<OpenID4VPCredentialSetQuery> credentialSets;
    Vector<OpenID4VPVerifierIdentity> verifierIdentities;
};

struct ValidatedOpenID4VPRequest {
    DigitalCredentialPresentationProtocol protocol;
    ValidatedOpenID4VPRequestObject request;
};

constexpr size_t maxOpenID4VPRequestCount = 32;
constexpr size_t maxOpenID4VPRequestElementCount = 4096;

// The strings of a validated request come from its request data, so they share that data's length
// limit; the element count bounds every nested vector.
inline bool isWithinOpenID4VPRequestLimits(const ValidatedOpenID4VPRequestObject& request)
{
    size_t stringLength = 0;
    size_t elementCount = 0;
    auto addElement = [&] {
        return ++elementCount <= maxOpenID4VPRequestElementCount;
    };
    auto addString = [&](const String& string) {
        stringLength += string.length();
        return stringLength <= maxOpenID4VPRequestDataLength && addElement();
    };
    auto addStrings = [&](const Vector<String>& strings) {
        return addElement() && std::ranges::all_of(strings, addString);
    };

    for (auto& credential : request.credentials) {
        if (!addElement() || !addString(credential.identifier) || !addString(credential.documentType) || !addStrings(credential.verifiableCredentialTypes))
            return false;
        for (auto& trustedAuthority : credential.trustedAuthorities) {
            if (!addStrings(trustedAuthority.values))
                return false;
        }
        for (auto& claim : credential.claims) {
            if (!addElement() || !addString(claim.identifier))
                return false;
            for (auto& component : claim.path) {
                auto* key = std::get_if<String>(&component);
                if (!(key ? addString(*key) : addElement()))
                    return false;
            }
            for (auto& value : claim.values) {
                auto* string = std::get_if<String>(&value);
                if (!(string ? addString(*string) : addElement()))
                    return false;
            }
        }
        for (auto& claimSet : credential.claimSets) {
            if (!addStrings(claimSet))
                return false;
        }
    }

    for (auto& credentialSet : request.credentialSets) {
        if (!addElement())
            return false;
        for (auto& option : credentialSet.options) {
            if (!addStrings(option))
                return false;
        }
    }

    for (auto& verifierIdentity : request.verifierIdentities) {
        if (!addElement() || !addString(verifierIdentity.clientIdentifierPrefix) || !addString(verifierIdentity.identifier))
            return false;
    }

    return true;
}

} // namespace WebCore
