// Copyright 2019 The Chromium Authors. All rights reserved.
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
#include "Pin.h"

#if ENABLE(WEB_AUTHN)

#include "CBORReader.h"
#include "CBORWriter.h"
#include "CryptoAlgorithmAESCBC.h"
#include "CryptoAlgorithmAesCbcCfbParams.h"
#include "CryptoAlgorithmECDH.h"
#include "CryptoAlgorithmHMAC.h"
#include "CryptoKeyAES.h"
#include "CryptoKeyEC.h"
#include "CryptoKeyHMAC.h"
#include "DeviceResponseConverter.h"
#include "WebAuthenticationConstants.h"
#include "WebAuthenticationUtils.h"
#include <pal/crypto/CryptoDigest.h>

namespace fido {
using namespace WebCore;
using CBOR = cbor::CBORValue;

namespace pin {
using namespace cbor;

// hasAtLeastFourCodepoints returns true if |pin| contains
// four or more code points. This reflects the "4 Unicode characters"
// requirement in CTAP2.
static bool hasAtLeastFourCodepoints(const String& pin)
{
    return pin.length() >= 4;
}

// makePinAuth returns `LEFT(HMAC-SHA-256(secret, data), 16)`.
static Vector<uint8_t> makePinAuth(const CryptoKeyHMAC& key, const Vector<uint8_t>& data)
{
    auto result = CryptoAlgorithmHMAC::platformSign(key, data);
    ASSERT(!result.hasException());
    auto pinAuth = result.releaseReturnValue();
    pinAuth.shrink(16);
    return pinAuth;
}

std::optional<CString> validateAndConvertToUTF8(const String& pin)
{
    if (!hasAtLeastFourCodepoints(pin))
        return std::nullopt;
    auto result = pin.utf8();
    if (result.length() < kMinBytes || result.length() > kMaxBytes)
        return std::nullopt;
    return result;
}

// encodePINCommand returns a CTAP2 PIN command for the operation |subcommand|.
// Additional elements of the top-level CBOR map can be added with the optional
// |addAdditional| callback.
static Vector<uint8_t> encodePinCommand(Subcommand subcommand, Function<void(CBORValue::MapValue*)> addAdditional = nullptr)
{
    CBORValue::MapValue map;
    map.emplace(static_cast<int64_t>(RequestKey::kProtocol), kProtocolVersion);
    map.emplace(static_cast<int64_t>(RequestKey::kSubcommand), static_cast<int64_t>(subcommand));

    if (addAdditional)
        addAdditional(&map);

    auto serializedParam = CBORWriter::write(CBORValue(WTFMove(map)));
    ASSERT(serializedParam);

    Vector<uint8_t> cborRequest({ static_cast<uint8_t>(CtapRequestCommand::kAuthenticatorClientPin) });
    cborRequest.appendVector(*serializedParam);
    return cborRequest;
}

RetriesResponse::RetriesResponse() = default;

std::optional<RetriesResponse> RetriesResponse::parse(const Vector<uint8_t>& inBuffer)
{
    auto decodedMap = decodeResponseMap(inBuffer);
    if (!decodedMap)
        return std::nullopt;
    const auto& responseMap = decodedMap->getMap();

    auto it = responseMap.find(CBORValue(static_cast<int64_t>(ResponseKey::kRetries)));
    if (it == responseMap.end() || !it->second.isUnsigned())
        return std::nullopt;

    RetriesResponse ret;
    ret.retries = static_cast<uint64_t>(it->second.getUnsigned());
    return ret;
}

KeyAgreementResponse::KeyAgreementResponse(Ref<CryptoKeyEC>&& peerKey)
    : peerKey(WTFMove(peerKey))
{
}

std::optional<KeyAgreementResponse> KeyAgreementResponse::parse(const Vector<uint8_t>& inBuffer)
{
    auto decodedMap = decodeResponseMap(inBuffer);
    if (!decodedMap)
        return std::nullopt;
    const auto& responseMap = decodedMap->getMap();

    // The ephemeral key is encoded as a COSE structure.
    auto it = responseMap.find(CBORValue(static_cast<int64_t>(ResponseKey::kKeyAgreement)));
    if (it == responseMap.end() || !it->second.isMap())
        return std::nullopt;
    const auto& coseKey = it->second.getMap();

    return parseFromCOSE(coseKey);
}

std::optional<KeyAgreementResponse> KeyAgreementResponse::parseFromCOSE(const CBORValue::MapValue& coseKey)
{
    // The COSE key must be a P-256 point. See
    // https://tools.ietf.org/html/rfc8152#section-7.1
    for (const auto& pair : Vector<std::pair<int64_t, int64_t>>({
        { static_cast<int64_t>(COSE::kty), static_cast<int64_t>(COSE::EC2) },
        { static_cast<int64_t>(COSE::alg), static_cast<int64_t>(COSE::ECDH256) },
        { static_cast<int64_t>(COSE::crv), static_cast<int64_t>(COSE::P_256) },
    })) {
        auto it = coseKey.find(CBORValue(pair.first));
        if (it == coseKey.end() || !it->second.isInteger() || it->second.getInteger() != pair.second)
            return std::nullopt;
    }

    // See https://tools.ietf.org/html/rfc8152#section-13.1.1
    const auto& xIt = coseKey.find(CBORValue(static_cast<int64_t>(COSE::x)));
    const auto& yIt = coseKey.find(CBORValue(static_cast<int64_t>(COSE::y)));
    if (xIt == coseKey.end() || yIt == coseKey.end() || !xIt->second.isByteString() || !yIt->second.isByteString())
        return std::nullopt;

    const auto& x = xIt->second.getByteString();
    const auto& y = yIt->second.getByteString();
    auto peerKey = CryptoKeyEC::importRaw(CryptoAlgorithmIdentifier::ECDH, "P-256"_s, encodeRawPublicKey(x, y), true, CryptoKeyUsageDeriveBits);
    if (!peerKey)
        return std::nullopt;

    return KeyAgreementResponse(peerKey.releaseNonNull());
}

cbor::CBORValue::MapValue encodeCOSEPublicKey(const Vector<uint8_t>& rawPublicKey)
{
    ASSERT(rawPublicKey.size() == 65);
    Vector<uint8_t> x { rawPublicKey.data() + 1, ES256FieldElementLength };
    Vector<uint8_t> y { rawPublicKey.data() + 1 + ES256FieldElementLength, ES256FieldElementLength };

    cbor::CBORValue::MapValue publicKeyMap;
    publicKeyMap[cbor::CBORValue(COSE::kty)] = cbor::CBORValue(COSE::EC2);
    publicKeyMap[cbor::CBORValue(COSE::alg)] = cbor::CBORValue(COSE::ECDH256);
    publicKeyMap[cbor::CBORValue(COSE::crv)] = cbor::CBORValue(COSE::P_256);
    publicKeyMap[cbor::CBORValue(COSE::x)] = cbor::CBORValue(WTFMove(x));
    publicKeyMap[cbor::CBORValue(COSE::y)] = cbor::CBORValue(WTFMove(y));

    return publicKeyMap;
}

TokenResponse::TokenResponse(Ref<WebCore::CryptoKeyHMAC>&& token)
    : m_token(WTFMove(token))
{
}

std::optional<TokenResponse> TokenResponse::parse(const WebCore::CryptoKeyAES& sharedKey, const Vector<uint8_t>& inBuffer)
{
    auto decodedMap = decodeResponseMap(inBuffer);
    if (!decodedMap)
        return std::nullopt;
    const auto& responseMap = decodedMap->getMap();

    auto it = responseMap.find(CBORValue(static_cast<int64_t>(ResponseKey::kPinToken)));
    if (it == responseMap.end() || !it->second.isByteString())
        return std::nullopt;
    const auto& encryptedToken = it->second.getByteString();

    auto tokenResult = CryptoAlgorithmAESCBC::platformDecrypt({ }, sharedKey, encryptedToken, CryptoAlgorithmAESCBC::Padding::No);
    if (tokenResult.hasException())
        return std::nullopt;
    auto token = tokenResult.releaseReturnValue();

    auto tokenKey = CryptoKeyHMAC::importRaw(token.size() * 8, CryptoAlgorithmIdentifier::SHA_256, WTFMove(token), true, CryptoKeyUsageSign);
    ASSERT(tokenKey);

    return TokenResponse(tokenKey.releaseNonNull());
}

Vector<uint8_t> TokenResponse::pinAuth(const Vector<uint8_t>& clientDataHash) const
{
    return makePinAuth(m_token, clientDataHash);
}

const Vector<uint8_t>& TokenResponse::token() const
{
    return m_token->key();
}

Vector<uint8_t> encodeAsCBOR(const RetriesRequest&)
{
    return encodePinCommand(Subcommand::kGetRetries);
}

Vector<uint8_t> encodeAsCBOR(const KeyAgreementRequest&)
{
    return encodePinCommand(Subcommand::kGetKeyAgreement);
}

std::optional<TokenRequest> TokenRequest::tryCreate(const CString& pin, const CryptoKeyEC& peerKey)
{
    // The following implements Section 5.5.4 Getting sharedSecret from Authenticator.
    // https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#gettingSharedSecret
    // 1. Generate a P256 key pair.
    auto keyPairResult = CryptoKeyEC::generatePair(CryptoAlgorithmIdentifier::ECDH, "P-256"_s, true, CryptoKeyUsageDeriveBits);
    ASSERT(!keyPairResult.hasException());
    auto keyPair = keyPairResult.releaseReturnValue();

    // 2. Use ECDH and SHA-256 to compute the shared AES-CBC key.
    auto sharedKeyResult = CryptoAlgorithmECDH::platformDeriveBits(downcast<CryptoKeyEC>(*keyPair.privateKey), peerKey);
    if (!sharedKeyResult)
        return std::nullopt;

    auto crypto = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);
    crypto->addBytes(sharedKeyResult->data(), sharedKeyResult->size());
    auto sharedKeyHash = crypto->computeHash();

    auto sharedKey = CryptoKeyAES::importRaw(CryptoAlgorithmIdentifier::AES_CBC, WTFMove(sharedKeyHash), true, CryptoKeyUsageEncrypt | CryptoKeyUsageDecrypt);
    ASSERT(sharedKey);

    // The following encodes the public key of the above key pair into COSE format.
    auto rawPublicKeyResult = downcast<CryptoKeyEC>(*keyPair.publicKey).exportRaw();
    ASSERT(!rawPublicKeyResult.hasException());
    auto coseKey = encodeCOSEPublicKey(rawPublicKeyResult.returnValue());

    // The following calculates a SHA-256 digest of the PIN, and shrink to the left 16 bytes.
    crypto = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);
    crypto->addBytes(pin.data(), pin.length());
    auto pinHash = crypto->computeHash();
    pinHash.shrink(16);

    return TokenRequest(sharedKey.releaseNonNull(), WTFMove(coseKey), WTFMove(pinHash));
}

TokenRequest::TokenRequest(Ref<WebCore::CryptoKeyAES>&& sharedKey, cbor::CBORValue::MapValue&& coseKey, Vector<uint8_t>&& pinHash)
    : m_sharedKey(WTFMove(sharedKey))
    , m_coseKey(WTFMove(coseKey))
    , m_pinHash(WTFMove(pinHash))
{
}

const CryptoKeyAES& TokenRequest::sharedKey() const
{
    return m_sharedKey;
}

Vector<uint8_t> encodeAsCBOR(const TokenRequest& request)
{
    auto result = CryptoAlgorithmAESCBC::platformEncrypt({ }, request.sharedKey(), request.m_pinHash, CryptoAlgorithmAESCBC::Padding::No);
    ASSERT(!result.hasException());

    return encodePinCommand(Subcommand::kGetPinToken, [coseKey = WTFMove(request.m_coseKey), encryptedPin = result.releaseReturnValue()] (CBORValue::MapValue* map) mutable {
        map->emplace(static_cast<int64_t>(RequestKey::kKeyAgreement), WTFMove(coseKey));
        map->emplace(static_cast<int64_t>(RequestKey::kPinHashEnc), WTFMove(encryptedPin));
    });
}

} // namespace pin
} // namespace fido

#endif // ENABLE(WEB_AUTHN)
