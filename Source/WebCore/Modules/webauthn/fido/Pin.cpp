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
#include "CryptoAlgorithmHKDF.h"
#include "CryptoAlgorithmHMAC.h"
#include "CryptoAlgorithmHkdfParams.h"
#include "CryptoKeyAES.h"
#include "CryptoKeyEC.h"
#include "CryptoKeyHMAC.h"
#include "CryptoKeyRaw.h"
#include "DeviceResponseConverter.h"
#include "ExceptionOr.h"
#include "WebAuthenticationConstants.h"
#include "WebAuthenticationUtils.h"
#include <pal/crypto/CryptoDigest.h>
#include <wtf/CryptographicallyRandomNumber.h>

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

static Vector<uint8_t> decryptForProtocol(PINUVAuthProtocol protocol, const CryptoKeyAES& key, const Vector<uint8_t>& ciphertext)
{
    if (protocol == PINUVAuthProtocol::kPinProtocol2) {
        // CTAP 2.1 spec 6.5.7: Protocol 2 decrypt
        // Split ciphertext into IV (first 16 bytes) and ct (remaining bytes)
        if (ciphertext.size() < 16)
            return { };

        Vector<uint8_t> iv(ciphertext.subspan(0, 16));
        Vector<uint8_t> ct(ciphertext.subspan(16));

        CryptoAlgorithmAesCbcCfbParams params;
        params.iv = BufferSource(iv);

        auto result = CryptoAlgorithmAESCBC::platformDecrypt(params, key, ct, CryptoAlgorithmAESCBC::Padding::No);
        if (result.hasException())
            return { };
        return result.releaseReturnValue();
    }

    // CTAP 2.1 spec 6.5.6: Protocol 1 decrypt with zero IV
    auto result = CryptoAlgorithmAESCBC::platformDecrypt({ }, key, ciphertext, CryptoAlgorithmAESCBC::Padding::No);
    if (result.hasException())
        return { };
    return result.releaseReturnValue();
}
static Vector<uint8_t> authenticateForProtocol(PINUVAuthProtocol protocol, const CryptoKeyHMAC& key, const Vector<uint8_t>& message)
{
    auto result = CryptoAlgorithmHMAC::platformSign(key, message);
    ASSERT(!result.hasException());
    auto signature = result.releaseReturnValue();

    // https://fidoalliance.org/specs/fido-v2.1-ps-20210615/fido-client-to-authenticator-protocol-v2.1-ps-20210615.html#pinProto1
    // Pin Protocol 1 should trim to 16 bytes, Pin Protocol 2 uses full 32.
    if (protocol == PINUVAuthProtocol::kPinProtocol1)
        signature.shrink(16);

    return signature;
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
static Vector<uint8_t> encodePinCommand(Subcommand subcommand, PINUVAuthProtocol protocol, Function<void(CBORValue::MapValue*)> addAdditional = nullptr)
{
    CBORValue::MapValue map;
    map.emplace(static_cast<int64_t>(RequestKey::kProtocol), static_cast<int64_t>(protocol));
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

KeyAgreementResponse::~KeyAgreementResponse() = default;
KeyAgreementResponse::KeyAgreementResponse(KeyAgreementResponse&&) = default;
KeyAgreementResponse& KeyAgreementResponse::operator=(KeyAgreementResponse&&) = default;

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
    auto x = rawPublicKey.subvector(1, ES256FieldElementLength);
    auto y = rawPublicKey.subvector(1 + ES256FieldElementLength, ES256FieldElementLength);

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

std::optional<TokenResponse> TokenResponse::parse(PINUVAuthProtocol protocol, const WebCore::CryptoKeyAES& sharedKey, const Vector<uint8_t>& inBuffer)
{
    auto decodedMap = decodeResponseMap(inBuffer);
    if (!decodedMap)
        return std::nullopt;
    const auto& responseMap = decodedMap->getMap();

    auto it = responseMap.find(CBORValue(static_cast<int64_t>(ResponseKey::kPinToken)));
    if (it == responseMap.end() || !it->second.isByteString())
        return std::nullopt;
    const auto& encryptedToken = it->second.getByteString();

    auto token = decryptForProtocol(protocol, sharedKey, encryptedToken);
    if (token.isEmpty())
        return std::nullopt;

    auto tokenKey = CryptoKeyHMAC::importRaw(token.size() * 8, CryptoAlgorithmIdentifier::SHA_256, WTFMove(token), true, CryptoKeyUsageSign);
    if (!tokenKey)
        return std::nullopt;

    return TokenResponse(tokenKey.releaseNonNull());
}

Vector<uint8_t> TokenResponse::pinAuth(PINUVAuthProtocol protocol, const Vector<uint8_t>& clientDataHash) const
{
    return authenticateForProtocol(protocol, m_token, clientDataHash);
}

const Vector<uint8_t>& TokenResponse::token() const
{
    return m_token->key();
}

Vector<uint8_t> encodeAsCBOR(const RetriesRequest& request)
{
    return encodePinCommand(Subcommand::kGetRetries, request.protocol);
}

Vector<uint8_t> encodeAsCBOR(const KeyAgreementRequest& request)
{
    return encodePinCommand(Subcommand::kGetKeyAgreement, request.protocol);
}

static Vector<uint8_t> deriveProtocolSharedSecret(PINUVAuthProtocol protocol, Vector<uint8_t>&& ecdhResult)
{
    // CTAP spec 6.5.6 (Protocol 1) and 6.5.7 (Protocol 2).
    Vector<uint8_t> sharedSecret;
    if (protocol == PINUVAuthProtocol::kPinProtocol1) {
        auto crypto = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);
        crypto->addBytes(ecdhResult.span());
        sharedSecret = crypto->computeHash();
    } else if (protocol == PINUVAuthProtocol::kPinProtocol2) {
        sharedSecret.reserveInitialCapacity(64);
        auto hkdfKey = CryptoKeyRaw::create(CryptoAlgorithmIdentifier::HKDF, WTFMove(ecdhResult), CryptoKeyUsageDeriveBits);

        CryptoAlgorithmHkdfParams hmacHkdfParams;
        hmacHkdfParams.hashIdentifier = CryptoAlgorithmIdentifier::SHA_256;
        Vector<uint8_t> hkdfSalt(32, 0);
        hmacHkdfParams.salt = toBufferSource(hkdfSalt.span());
        hmacHkdfParams.info = toBufferSource(std::span { kHKDFInfoHMACKey });

        auto hmacKeyMaterial = CryptoAlgorithmHKDF::deriveBits(hmacHkdfParams, hkdfKey.get(), 32 * 8);
        if (hmacKeyMaterial.hasException())
            return { };
        sharedSecret.appendVector(hmacKeyMaterial.releaseReturnValue());

        CryptoAlgorithmHkdfParams aesHkdfParams;
        aesHkdfParams.hashIdentifier = CryptoAlgorithmIdentifier::SHA_256;
        aesHkdfParams.salt = toBufferSource(hkdfSalt.span());
        aesHkdfParams.info = toBufferSource(std::span { kHKDFInfoAESKey });

        auto aesKeyMaterial = CryptoAlgorithmHKDF::deriveBits(aesHkdfParams, hkdfKey.get(), 32 * 8);
        if (aesKeyMaterial.hasException())
            return { };
        sharedSecret.appendVector(aesKeyMaterial.releaseReturnValue());
    } else {
        ASSERT_NOT_REACHED();
        return { };
    }
    return sharedSecret;
}

static Vector<uint8_t> encryptForProtocol(PINUVAuthProtocol protocol, const CryptoKeyAES& key, const Vector<uint8_t>& plaintext)
{
    if (protocol == PINUVAuthProtocol::kPinProtocol2) {
        Vector<uint8_t> iv(16);
        cryptographicallyRandomValues(iv.mutableSpan());

        CryptoAlgorithmAesCbcCfbParams params;
        params.iv = BufferSource(iv);

        auto result = CryptoAlgorithmAESCBC::platformEncrypt(params, key, plaintext, CryptoAlgorithmAESCBC::Padding::No);
        ASSERT(!result.hasException());

        Vector<uint8_t> output;
        output.reserveInitialCapacity(iv.size() + result.returnValue().size());
        output.appendVector(iv);
        output.appendVector(result.releaseReturnValue());
        return output;
    }

    auto result = CryptoAlgorithmAESCBC::platformEncrypt({ }, key, plaintext, CryptoAlgorithmAESCBC::Padding::No);
    ASSERT(!result.hasException());
    return result.releaseReturnValue();
}

std::optional<TokenRequest> TokenRequest::tryCreate(PINUVAuthProtocol protocol, const CString& pin, const CryptoKeyEC& peerKey)
{
    // The following implements Section 5.5.4 Getting sharedSecret from Authenticator.
    // https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#gettingSharedSecret
    // 1. Generate a P256 key pair.
    auto keyPairResult = CryptoKeyEC::generatePair(CryptoAlgorithmIdentifier::ECDH, "P-256"_s, true, CryptoKeyUsageDeriveBits);
    ASSERT(!keyPairResult.hasException());
    auto keyPair = keyPairResult.releaseReturnValue();

    // 2. Use ECDH to compute the shared secret, then apply protocol-specific KDF.
    auto sharedKeyResult = CryptoAlgorithmECDH::platformDeriveBits(downcast<CryptoKeyEC>(*keyPair.privateKey), peerKey);
    if (!sharedKeyResult)
        return std::nullopt;

    auto sharedSecret = deriveProtocolSharedSecret(protocol, WTFMove(*sharedKeyResult));
    if (sharedSecret.isEmpty())
        return std::nullopt;

    Vector<uint8_t> aesKeyMaterial;
    if (protocol == PINUVAuthProtocol::kPinProtocol2) {
        ASSERT(sharedSecret.size() == 64);
        aesKeyMaterial = Vector<uint8_t>(sharedSecret.span().last(32));
    } else
        aesKeyMaterial = sharedSecret;

    auto sharedKey = CryptoKeyAES::importRaw(CryptoAlgorithmIdentifier::AES_CBC, WTFMove(aesKeyMaterial), true, CryptoKeyUsageEncrypt | CryptoKeyUsageDecrypt);
    ASSERT(sharedKey);

    // The following encodes the public key of the above key pair into COSE format.
    auto rawPublicKeyResult = downcast<CryptoKeyEC>(*keyPair.publicKey).exportRaw();
    ASSERT(!rawPublicKeyResult.hasException());
    auto coseKey = encodeCOSEPublicKey(rawPublicKeyResult.returnValue());

    // The following calculates a SHA-256 digest of the PIN, and shrink to the left 16 bytes.
    auto crypto = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);
    crypto->addBytes(byteCast<uint8_t>(pin.span()));
    auto pinHash = crypto->computeHash();
    pinHash.shrink(16);

    return TokenRequest(sharedKey.releaseNonNull(), WTFMove(coseKey), WTFMove(pinHash), protocol);
}

TokenRequest::TokenRequest(Ref<WebCore::CryptoKeyAES>&& sharedKey, cbor::CBORValue::MapValue&& coseKey, Vector<uint8_t>&& pinHash, PINUVAuthProtocol protocol)
    : m_sharedKey(WTFMove(sharedKey))
    , m_coseKey(WTFMove(coseKey))
    , m_pinHash(WTFMove(pinHash))
    , m_protocol(protocol)
{
}

SetPinRequest::SetPinRequest(Ref<WebCore::CryptoKeyAES>&& sharedKey, cbor::CBORValue::MapValue&& coseKey, Vector<uint8_t>&& newPinEnc, Vector<uint8_t>&& pinUvAuthParam, PINUVAuthProtocol protocol)
    : m_sharedKey(WTFMove(sharedKey))
    , m_coseKey(WTFMove(coseKey))
    , m_newPinEnc(WTFMove(newPinEnc))
    , m_pinUvAuthParam(WTFMove(pinUvAuthParam))
    , m_protocol(protocol)
{
}

const Vector<uint8_t>& SetPinRequest::pinAuth() const
{
    return m_pinUvAuthParam;
}

std::optional<SetPinRequest> SetPinRequest::tryCreate(PINUVAuthProtocol protocol, const String& inputPin, const WebCore::CryptoKeyEC& peerKey)
{
    std::optional<CString> newPin = validateAndConvertToUTF8(inputPin);
    if (!newPin)
        return std::nullopt;

    // The following implements Section 5.5.4 Getting sharedSecret from Authenticator.
    // https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#gettingSharedSecret
    // 1. Generate a P256 key pair.
    auto keyPairResult = CryptoKeyEC::generatePair(CryptoAlgorithmIdentifier::ECDH, "P-256"_s, true, CryptoKeyUsageDeriveBits);
    ASSERT(!keyPairResult.hasException());
    auto keyPair = keyPairResult.releaseReturnValue();

    auto sharedKeyResult = CryptoAlgorithmECDH::platformDeriveBits(downcast<CryptoKeyEC>(*keyPair.privateKey), peerKey);
    if (!sharedKeyResult)
        return std::nullopt;

    auto sharedSecret = deriveProtocolSharedSecret(protocol, WTFMove(*sharedKeyResult));
    if (sharedSecret.isEmpty())
        return std::nullopt;

    Vector<uint8_t> hmacKeyMaterial, aesKeyMaterial;
    if (protocol == PINUVAuthProtocol::kPinProtocol2) {
        ASSERT(sharedSecret.size() == 64);
        hmacKeyMaterial = Vector<uint8_t>(sharedSecret.span().first(32));
        aesKeyMaterial = Vector<uint8_t>(sharedSecret.span().last(32));
    } else {
        hmacKeyMaterial = sharedSecret;
        aesKeyMaterial = sharedSecret;
    }

    auto sharedKey = CryptoKeyAES::importRaw(CryptoAlgorithmIdentifier::AES_CBC, WTFMove(aesKeyMaterial), true, CryptoKeyUsageEncrypt | CryptoKeyUsageDecrypt);
    ASSERT(sharedKey);

    // The following encodes the public key of the above key pair into COSE format.
    auto rawPublicKeyResult = downcast<CryptoKeyEC>(*keyPair.publicKey).exportRaw();
    ASSERT(!rawPublicKeyResult.hasException());
    auto coseKey = encodeCOSEPublicKey(rawPublicKeyResult.returnValue());

    const size_t minPaddedPinLength = 64;
    Vector<uint8_t> paddedPin;
    paddedPin.reserveInitialCapacity(minPaddedPinLength);
    paddedPin.append(inputPin.utf8().span());
    for (int i = paddedPin.size(); i < 64; i++)
        paddedPin.append('\0');

    auto hmacKey = CryptoKeyHMAC::importRaw(hmacKeyMaterial.size() * 8 /* lengthInBits */, CryptoAlgorithmIdentifier::SHA_256, WTFMove(hmacKeyMaterial), true, CryptoKeyUsageSign);

    auto newPinEnc = encryptForProtocol(protocol, *sharedKey, paddedPin);

    auto pinUvAuthParam = authenticateForProtocol(protocol, *hmacKey, newPinEnc);

    return SetPinRequest(sharedKey.releaseNonNull(), WTFMove(coseKey), WTFMove(newPinEnc), WTFMove(pinUvAuthParam), protocol);
}

Vector<uint8_t> encodeAsCBOR(const TokenRequest& request)
{
    auto encryptedPin = encryptForProtocol(request.m_protocol, request.sharedKey(), request.m_pinHash);

    return encodePinCommand(Subcommand::kGetPinToken, request.m_protocol, [coseKey = WTFMove(request.m_coseKey), encryptedPin = WTFMove(encryptedPin)] (CBORValue::MapValue* map) mutable {
        map->emplace(static_cast<int64_t>(RequestKey::kKeyAgreement), WTFMove(coseKey));
        map->emplace(static_cast<int64_t>(RequestKey::kPinHashEnc), WTFMove(encryptedPin));
    });
}

Vector<uint8_t> encodeAsCBOR(const SetPinRequest& request)
{
    return encodePinCommand(Subcommand::kSetPin, request.m_protocol, [coseKey = WTFMove(request.m_coseKey), encryptedPin = request.m_newPinEnc, pinUvAuthParam = request.m_pinUvAuthParam] (CBORValue::MapValue* map) mutable {
        map->emplace(static_cast<int64_t>(RequestKey::kKeyAgreement), WTFMove(coseKey));
        map->emplace(static_cast<int64_t>(RequestKey::kNewPinEnc), WTFMove(encryptedPin));
        map->emplace(static_cast<int64_t>(RequestKey::kPinAuth), WTFMove(pinUvAuthParam));
    });
}

// HmacSecretRequest implementation
HmacSecretRequest::HmacSecretRequest(Ref<CryptoKeyAES>&& sharedKey, CBORValue::MapValue&& coseKey, Vector<uint8_t>&& saltEnc, Vector<uint8_t>&& saltAuth, PINUVAuthProtocol protocol)
    : m_sharedKey(WTFMove(sharedKey))
    , m_coseKey(WTFMove(coseKey))
    , m_saltEnc(WTFMove(saltEnc))
    , m_saltAuth(WTFMove(saltAuth))
    , m_protocol(protocol)
{
}

std::optional<HmacSecretRequest> HmacSecretRequest::create(PINUVAuthProtocol protocol, const Vector<uint8_t>& salt1, const std::optional<Vector<uint8_t>>& salt2, const CryptoKeyEC& peerKey)
{
    if (salt1.size() != 32)
        return std::nullopt;
    if (salt2 && salt2->size() != 32)
        return std::nullopt;

    // The following implements Section 5.5.4 Getting sharedSecret from Authenticator
    auto keyPairResult = CryptoKeyEC::generatePair(CryptoAlgorithmIdentifier::ECDH, "P-256"_s, true, CryptoKeyUsageDeriveBits);
    if (keyPairResult.hasException())
        return std::nullopt;
    auto keyPair = keyPairResult.releaseReturnValue();

    auto sharedKeyResult = CryptoAlgorithmECDH::platformDeriveBits(downcast<CryptoKeyEC>(*keyPair.privateKey), peerKey);
    if (!sharedKeyResult)
        return std::nullopt;

    auto sharedSecret = deriveProtocolSharedSecret(protocol, WTFMove(*sharedKeyResult));
    if (sharedSecret.isEmpty())
        return std::nullopt;

    Vector<uint8_t> hmacKeyMaterial, aesKeyMaterial;
    if (protocol == PINUVAuthProtocol::kPinProtocol2) {
        ASSERT(sharedSecret.size() == 64);
        hmacKeyMaterial = Vector<uint8_t>(sharedSecret.span().first(32));
        aesKeyMaterial = Vector<uint8_t>(sharedSecret.span().last(32));
    } else {
        hmacKeyMaterial = sharedSecret;
        aesKeyMaterial = sharedSecret;
    }

    auto sharedKey = CryptoKeyAES::importRaw(CryptoAlgorithmIdentifier::AES_CBC, WTFMove(aesKeyMaterial), true, CryptoKeyUsageEncrypt | CryptoKeyUsageDecrypt);
    if (!sharedKey)
        return std::nullopt;

    auto rawPublicKeyResult = downcast<CryptoKeyEC>(*keyPair.publicKey).exportRaw();
    if (rawPublicKeyResult.hasException())
        return std::nullopt;
    auto coseKey = encodeCOSEPublicKey(rawPublicKeyResult.returnValue());

    Vector<uint8_t> saltsBuffer = salt1;
    if (salt2)
        saltsBuffer.appendVector(*salt2);

    auto saltEnc = encryptForProtocol(protocol, *sharedKey, saltsBuffer);

    auto hmacKey = CryptoKeyHMAC::importRaw(hmacKeyMaterial.size() * 8, CryptoAlgorithmIdentifier::SHA_256, WTFMove(hmacKeyMaterial), true, CryptoKeyUsageSign);
    if (!hmacKey)
        return std::nullopt;

    auto saltAuth = authenticateForProtocol(protocol, *hmacKey, saltEnc);

    return HmacSecretRequest(sharedKey.releaseNonNull(), WTFMove(coseKey), WTFMove(saltEnc), WTFMove(saltAuth), protocol);
}

// HmacSecretResponse implementation
HmacSecretResponse::HmacSecretResponse(Vector<uint8_t>&& decryptedOutput)
    : m_output(WTFMove(decryptedOutput))
{
}

std::optional<HmacSecretResponse> HmacSecretResponse::parse(PINUVAuthProtocol protocol, const CryptoKeyAES& sharedKey, const Vector<uint8_t>& encryptedOutput)
{
    auto output = decryptForProtocol(protocol, sharedKey, encryptedOutput);
    if (output.isEmpty())
        return std::nullopt;

    if (output.size() != 32 && output.size() != 64)
        return std::nullopt;

    return HmacSecretResponse(WTFMove(output));
}

const Vector<uint8_t>& HmacSecretResponse::output() const
{
    return m_output;
}

} // namespace pin
} // namespace fido

#endif // ENABLE(WEB_AUTHN)
