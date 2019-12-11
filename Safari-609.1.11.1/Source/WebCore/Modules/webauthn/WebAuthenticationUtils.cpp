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

#include "config.h"
#include "WebAuthenticationUtils.h"

#if ENABLE(WEB_AUTHN)

#include "CBORWriter.h"
#include "WebAuthenticationConstants.h"
#include <pal/crypto/CryptoDigest.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

Vector<uint8_t> convertBytesToVector(const uint8_t byteArray[], const size_t length)
{
    Vector<uint8_t> result;
    result.append(byteArray, length);
    return result;
}

Vector<uint8_t> produceRpIdHash(const String& rpId)
{
    auto crypto = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);
    auto rpIdUtf8 = rpId.utf8();
    crypto->addBytes(rpIdUtf8.data(), rpIdUtf8.length());
    return crypto->computeHash();
}

Vector<uint8_t> encodeES256PublicKeyAsCBOR(Vector<uint8_t>&& x, Vector<uint8_t>&& y)
{
    cbor::CBORValue::MapValue publicKeyMap;
    publicKeyMap[cbor::CBORValue(COSE::kty)] = cbor::CBORValue(COSE::EC2);
    publicKeyMap[cbor::CBORValue(COSE::alg)] = cbor::CBORValue(COSE::ES256);
    publicKeyMap[cbor::CBORValue(COSE::crv)] = cbor::CBORValue(COSE::P_256);
    publicKeyMap[cbor::CBORValue(COSE::x)] = cbor::CBORValue(WTFMove(x));
    publicKeyMap[cbor::CBORValue(COSE::y)] = cbor::CBORValue(WTFMove(y));

    auto cosePublicKey = cbor::CBORWriter::write(cbor::CBORValue(WTFMove(publicKeyMap)));
    ASSERT(cosePublicKey);
    return *cosePublicKey;
}

Vector<uint8_t> buildAttestedCredentialData(const Vector<uint8_t>& aaguid, const Vector<uint8_t>& credentialId, const Vector<uint8_t>& coseKey)
{
    Vector<uint8_t> attestedCredentialData;
    attestedCredentialData.reserveInitialCapacity(aaguidLength + credentialIdLengthLength + credentialId.size() + coseKey.size());

    // aaguid
    ASSERT(aaguid.size() == aaguidLength);
    attestedCredentialData.appendVector(aaguid);

    // credentialIdLength
    ASSERT(credentialId.size() <= std::numeric_limits<uint16_t>::max());
    attestedCredentialData.append(credentialId.size() >> 8 & 0xff);
    attestedCredentialData.append(credentialId.size() & 0xff);

    // credentialId
    attestedCredentialData.appendVector(credentialId);

    // credentialPublicKey
    attestedCredentialData.appendVector(coseKey);

    return attestedCredentialData;
}

Vector<uint8_t> buildAuthData(const String& rpId, const uint8_t flags, const uint32_t counter, const Vector<uint8_t>& optionalAttestedCredentialData)
{
    Vector<uint8_t> authData;
    authData.reserveInitialCapacity(rpIdHashLength + flagsLength + signCounterLength + optionalAttestedCredentialData.size());

    // RP ID hash
    authData.appendVector(produceRpIdHash(rpId));

    // FLAGS
    authData.append(flags);

    // COUNTER
    authData.append(counter >> 24 & 0xff);
    authData.append(counter >> 16 & 0xff);
    authData.append(counter >> 8 & 0xff);
    authData.append(counter & 0xff);

    // ATTESTED CRED. DATA
    authData.appendVector(optionalAttestedCredentialData);

    return authData;
}

Vector<uint8_t> buildAttestationObject(Vector<uint8_t>&& authData, String&& format, cbor::CBORValue::MapValue&& statementMap, const AttestationConveyancePreference& attestation)
{
    cbor::CBORValue::MapValue attestationObjectMap;
    // The following implements Step 20 with regard to AttestationConveyancePreference
    // of https://www.w3.org/TR/webauthn/#createCredential as of 4 March 2019.
    // None attestation is always returned if it is requested to keep consistency, and therefore skip the
    // step to return self attestation.
    if (attestation == AttestationConveyancePreference::None) {
        const size_t aaguidOffset = rpIdHashLength + flagsLength + signCounterLength;
        if (authData.size() >= aaguidOffset + aaguidLength)
            memset(authData.data() + aaguidOffset, 0, aaguidLength);
        format = noneAttestationValue;
        statementMap.clear();
    }
    attestationObjectMap[cbor::CBORValue("authData")] = cbor::CBORValue(WTFMove(authData));
    attestationObjectMap[cbor::CBORValue("fmt")] = cbor::CBORValue(WTFMove(format));
    attestationObjectMap[cbor::CBORValue("attStmt")] = cbor::CBORValue(WTFMove(statementMap));

    auto attestationObject = cbor::CBORWriter::write(cbor::CBORValue(WTFMove(attestationObjectMap)));
    ASSERT(attestationObject);
    return *attestationObject;
}


} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
