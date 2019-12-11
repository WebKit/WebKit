// Copyright 2018 The Chromium Authors. All rights reserved.
// Copyright (C) 2019 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"
#include "U2fResponseConverter.h"

#if ENABLE(WEB_AUTHN)

#include "CommonCryptoDERUtilities.h"
#include "FidoConstants.h"
#include "WebAuthenticationConstants.h"
#include "WebAuthenticationUtils.h"

namespace fido {
using namespace WebCore;

namespace {

// In a U2F registration response, the key is in X9.62 format:
// - a constant 0x04 prefix to indicate an uncompressed key
// - the 32-byte x coordinate
// - the 32-byte y coordinate.
const uint8_t uncompressedKey = 0x04;
// https://www.w3.org/TR/webauthn/#flags
const uint8_t makeCredentialFlags = 0b01000001; // UP and AT are set.
// https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-raw-message-formats-v1.2-ps-20170411.html#registration-response-message-success
const uint8_t minSignatureLength = 71;
const uint8_t maxSignatureLength = 73;
// https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-raw-message-formats-v1.2-ps-20170411.html#authentication-response-message-success
const size_t flagIndex = 0;
const size_t counterIndex = 1;
const size_t signatureIndex = 5;

static Vector<uint8_t> extractECPublicKeyFromU2fRegistrationResponse(const Vector<uint8_t>& u2fData)
{
    size_t pos = kReservedLength;
    if (u2fData.size() <= pos || u2fData[pos] != uncompressedKey)
        return { };
    pos++;

    if (u2fData.size() < pos + 2 * ES256FieldElementLength)
        return { };

    Vector<uint8_t> x;
    x.append(u2fData.data() + pos, ES256FieldElementLength);
    pos += ES256FieldElementLength;

    Vector<uint8_t> y;
    y.append(u2fData.data() + pos, ES256FieldElementLength);

    return encodeES256PublicKeyAsCBOR(WTFMove(x), WTFMove(y));
}

static Vector<uint8_t> extractCredentialIdFromU2fRegistrationResponse(const Vector<uint8_t>& u2fData)
{
    size_t pos = kU2fKeyHandleLengthOffset;
    if (u2fData.size() <= pos)
        return { };
    size_t credentialIdLength = u2fData[pos];
    pos++;

    if (u2fData.size() < pos + credentialIdLength)
        return { };
    Vector<uint8_t> credentialId;
    credentialId.append(u2fData.data() + pos, credentialIdLength);
    return credentialId;
}

static Vector<uint8_t> createAttestedCredentialDataFromU2fRegisterResponse(const Vector<uint8_t>& u2fData, const Vector<uint8_t>& publicKey)
{
    auto credentialId = extractCredentialIdFromU2fRegistrationResponse(u2fData);
    if (credentialId.isEmpty())
        return { };

    return buildAttestedCredentialData(Vector<uint8_t>(aaguidLength, 0), credentialId, publicKey);
}

static size_t parseX509Length(const Vector<uint8_t>& u2fData, size_t offset)
{
    if (u2fData.size() <= offset || u2fData[offset] != SequenceMark)
        return 0;
    offset++;

    if (u2fData.size() <= offset)
        return 0;
    const auto sequenceLengthLength = bytesUsedToEncodedLength(u2fData[offset]);

    if (sequenceLengthLength > sizeof(size_t) || (u2fData.size() < offset + sequenceLengthLength))
        return 0;
    size_t sequenceLength = sequenceLengthLength == 1 ? u2fData[offset] : 0;
    offset++;
    for (auto i = sequenceLengthLength - 1; i; i--, offset++)
        sequenceLength += u2fData[offset] << (i - 1) * 8;

    return sequenceLength + sequenceLengthLength + sizeof(SequenceMark);
}

static cbor::CBORValue::MapValue createFidoAttestationStatementFromU2fRegisterResponse(const Vector<uint8_t>& u2fData, size_t offset)
{
    auto x509Length = parseX509Length(u2fData, offset);
    if (!x509Length || u2fData.size() < offset + x509Length)
        return { };

    Vector<uint8_t> x509;
    x509.append(u2fData.data() + offset, x509Length);
    offset += x509Length;

    Vector<uint8_t> signature;
    signature.append(u2fData.data() + offset, u2fData.size() - offset);
    if (signature.size() < minSignatureLength || signature.size() > maxSignatureLength)
        return { };

    cbor::CBORValue::MapValue attestationStatementMap;
    attestationStatementMap[cbor::CBORValue("sig")] = cbor::CBORValue(WTFMove(signature));
    Vector<cbor::CBORValue> cborArray;
    cborArray.append(cbor::CBORValue(WTFMove(x509)));
    attestationStatementMap[cbor::CBORValue("x5c")] = cbor::CBORValue(WTFMove(cborArray));

    return attestationStatementMap;
}

} // namespace

Optional<PublicKeyCredentialData> readU2fRegisterResponse(const String& rpId, const Vector<uint8_t>& u2fData, const AttestationConveyancePreference& attestation)
{
    auto publicKey = extractECPublicKeyFromU2fRegistrationResponse(u2fData);
    if (publicKey.isEmpty())
        return WTF::nullopt;

    auto attestedCredentialData = createAttestedCredentialDataFromU2fRegisterResponse(u2fData, publicKey);
    if (attestedCredentialData.isEmpty())
        return WTF::nullopt;

    // Extract the credentialId for packing into the response data.
    auto credentialId = extractCredentialIdFromU2fRegistrationResponse(u2fData);
    ASSERT(!credentialId.isEmpty());

    // The counter is zeroed out for Register requests.
    auto authData = buildAuthData(rpId, makeCredentialFlags, 0, attestedCredentialData);

    auto fidoAttestationStatement = createFidoAttestationStatementFromU2fRegisterResponse(u2fData, kU2fKeyHandleOffset + credentialId.size());
    if (fidoAttestationStatement.empty())
        return WTF::nullopt;

    auto attestationObject = buildAttestationObject(WTFMove(authData), "fido-u2f", WTFMove(fidoAttestationStatement), attestation);

    return PublicKeyCredentialData { ArrayBuffer::create(credentialId.data(), credentialId.size()), true, nullptr, ArrayBuffer::create(attestationObject.data(), attestationObject.size()), nullptr, nullptr, nullptr, WTF::nullopt };
}

Optional<PublicKeyCredentialData> readU2fSignResponse(const String& rpId, const Vector<uint8_t>& keyHandle, const Vector<uint8_t>& u2fData)
{
    if (keyHandle.isEmpty() || u2fData.size() <= signatureIndex)
        return WTF::nullopt;

    // 1 byte flags, 4 bytes counter
    auto flags = u2fData[flagIndex];
    uint32_t counter = u2fData[counterIndex] << 24;
    counter += u2fData[counterIndex + 1] << 16;
    counter += u2fData[counterIndex + 2] << 8;
    counter += u2fData[counterIndex + 3];
    auto authData = buildAuthData(rpId, flags, counter, { });

    return PublicKeyCredentialData { ArrayBuffer::create(keyHandle.data(), keyHandle.size()), false, nullptr, nullptr, ArrayBuffer::create(authData.data(), authData.size()), ArrayBuffer::create(u2fData.data() + signatureIndex, u2fData.size() - signatureIndex), nullptr, WTF::nullopt };
}

} // namespace fido

#endif // ENABLE(WEB_AUTHN)
