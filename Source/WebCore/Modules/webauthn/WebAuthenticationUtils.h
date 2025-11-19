/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_AUTHN)

#include <WebCore/AttestationConveyancePreference.h>
#include <WebCore/AuthenticatorTransport.h>
#include <WebCore/BufferSource.h>
#include <WebCore/CBORValue.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/WebAuthenticationConstants.h>
#include <wtf/Forward.h>
#include <wtf/JSONValues.h>

namespace WebCore {

struct ParsedCOSEKey {
    int keyType; // 1=OKP, 2=EC2, 3=RSA, 4=Symmetric, 5=HSS-LMS
    std::optional<int> algorithm; // COSE alg identifier (e.g., -7=ES256, -257=RS256)
    std::optional<int> curve; // COSE curve identifier (e.g., 1=P-256, 6=Ed25519)
    std::optional<String> keyId; // base64
    std::optional<Vector<int>> keyOps; // array of key operation identifiers
    std::optional<String> baseIV; // base64

    // EC2/OKP keys (keyType 1, 2)
    std::optional<String> x; // base64 - x-coordinate
    std::optional<String> y; // base64 - y-coordinate (EC2 only)
    std::optional<String> d; // base64 - private key

    // RSA keys (keyType 3)
    std::optional<String> n; // base64 - modulus
    std::optional<String> e; // base64 - exponent
    std::optional<String> p; // base64 - first prime factor
    std::optional<String> q; // base64 - second prime factor
    std::optional<String> dP; // base64 - first factor CRT exponent
    std::optional<String> dQ; // base64 - second factor CRT exponent
    std::optional<String> qInv; // base64 - first CRT coefficient

    // Symmetric keys (keyType 4) and HSS-LMS (keyType 5)
    std::optional<String> keyValue; // base64
    std::optional<int> lmsType; // HSS-LMS type
    std::optional<int> lmotsType; // HSS-LMS OTS type

    String rawCBOR; // base64 - original CBOR bytes

    WEBCORE_EXPORT Ref<JSON::Object> toJSONObject() const;
};

struct ParsedAuthenticatorData {
    struct Flags {
        bool userPresent; // bit 0 (UP)
        bool rfu1; // bit 1 (reserved)
        bool userVerified; // bit 2 (UV)
        bool backupEligible; // bit 3 (BE)
        bool backupState; // bit 4 (BS)
        bool rfu2; // bit 5 (reserved)
        bool attestedCredentialDataIncluded; // bit 6 (AT)
        bool extensionDataIncluded; // bit 7 (ED)
    };

    String rpIdHash; // base64 (32 bytes)
    Flags flags;
    uint32_t signCount;

    // Attested credential data (if AT flag set)
    std::optional<String> aaguid; // base64 (16 bytes)
    std::optional<String> credentialId; // base64 (variable length)
    std::optional<ParsedCOSEKey> credentialPublicKey; // parsed COSE key + raw CBOR

    // Extensions (if ED flag set)
    std::optional<Ref<JSON::Value>> extensions; // parsed CBOR map as JSON
    std::optional<String> extensionsRaw; // base64 - raw CBOR bytes

    WEBCORE_EXPORT Ref<JSON::Object> toJSONObject() const;
};

// Parse authenticator data binary structure into structured format
WEBCORE_EXPORT std::optional<ParsedAuthenticatorData> parseAuthenticatorData(const Vector<uint8_t>& authData);

// Produce a SHA-256 hash of the given RP ID.
WEBCORE_EXPORT Vector<uint8_t> produceRpIdHash(const String& rpId);

WEBCORE_EXPORT Vector<uint8_t> encodeES256PublicKeyAsCBOR(Vector<uint8_t>&& x, Vector<uint8_t>&& y);

// https://www.w3.org/TR/webauthn/#attested-credential-data
WEBCORE_EXPORT Vector<uint8_t> buildAttestedCredentialData(const Vector<uint8_t>& aaguid, const Vector<uint8_t>& credentialId, const Vector<uint8_t>& coseKey);

// https://www.w3.org/TR/webauthn/#sec-authenticator-data
WEBCORE_EXPORT Vector<uint8_t> buildAuthData(const String& rpId, const uint8_t flags, const uint32_t counter, const Vector<uint8_t>& optionalAttestedCredentialData);

WEBCORE_EXPORT cbor::CBORValue::MapValue buildAttestationMap(Vector<uint8_t>&&, String&&, cbor::CBORValue::MapValue&&, const AttestationConveyancePreference&, ShouldZeroAAGUID = ShouldZeroAAGUID::No);

WEBCORE_EXPORT cbor::CBORValue::MapValue buildCredentialDescriptor(const Vector<uint8_t>& credentialId);

// https://www.w3.org/TR/webauthn/#attestation-object
WEBCORE_EXPORT Vector<uint8_t> buildAttestationObject(Vector<uint8_t>&& authData, String&& format, cbor::CBORValue::MapValue&& statementMap, const AttestationConveyancePreference&, ShouldZeroAAGUID = ShouldZeroAAGUID::No);

WEBCORE_EXPORT Ref<ArrayBuffer> buildClientDataJson(ClientDataType /*type*/, const BufferSource& challenge, const SecurityOrigin& /*origin*/, WebAuthn::Scope, const String& topOrigin = { });

WEBCORE_EXPORT Vector<uint8_t> buildClientDataJsonHash(const ArrayBuffer& clientDataJson);

WEBCORE_EXPORT cbor::CBORValue::MapValue buildUserEntityMap(const Vector<uint8_t>& userId, const String& name, const String& displayName);

// encodeRawPublicKey takes X & Y and returns them as a 0x04 || X || Y byte array.
WEBCORE_EXPORT Vector<uint8_t> encodeRawPublicKey(const Vector<uint8_t>& X, const Vector<uint8_t>& Y);

WEBCORE_EXPORT String toString(AuthenticatorTransport);

WEBCORE_EXPORT std::optional<AuthenticatorTransport> convertStringToAuthenticatorTransport(const String& transport);

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
