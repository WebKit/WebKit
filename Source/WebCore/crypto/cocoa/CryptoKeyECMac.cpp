/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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
#include "CryptoKeyEC.h"

#include "CommonCryptoDERUtilities.h"
#include "JsonWebKey.h"
#if HAVE(SWIFT_CPP_INTEROP)
#include <pal/PALSwift.h>
#endif
#include <wtf/text/Base64.h>

namespace WebCore {

static const unsigned char InitialOctetEC = 0x04; // Per Section 2.3.3 of http://www.secg.org/sec1-v2.pdf
// OID id-ecPublicKey 1.2.840.10045.2.1.
static const unsigned char IdEcPublicKey[] = {0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01};
// OID secp256r1 1.2.840.10045.3.1.7.
static constexpr unsigned char Secp256r1[] = {0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07};
// OID secp384r1 1.3.132.0.34
static constexpr unsigned char Secp384r1[] = {0x06, 0x05, 0x2b, 0x81, 0x04, 0x00, 0x22};
// OID secp521r1 1.3.132.0.35
static constexpr unsigned char Secp521r1[] = {0x06, 0x05, 0x2b, 0x81, 0x04, 0x00, 0x23};

// Version 1. Per https://tools.ietf.org/html/rfc5915#section-3
static const unsigned char PrivateKeyVersion[] = {0x02, 0x01, 0x01};
// Tagged type [1]
static const unsigned char TaggedType1 = 0xa1;

static constexpr size_t sizeCeil(float f)
{
    auto s = static_cast<size_t>(f);
    return f > s ? s + 1 : s;
}

static constexpr size_t keySizeInBitsFromNamedCurve(CryptoKeyEC::NamedCurve curve)
{
    switch (curve) {
    case CryptoKeyEC::NamedCurve::P256:
        return 256;
    case CryptoKeyEC::NamedCurve::P384:
        return 384;
    case CryptoKeyEC::NamedCurve::P521:
        return 521;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

static constexpr size_t keySizeInBytesFromNamedCurve(CryptoKeyEC::NamedCurve curve)
{
    return sizeCeil(keySizeInBitsFromNamedCurve(curve) / 8.);
}

// Per Section 2.3.4 of http://www.secg.org/sec1-v2.pdf
// We only support uncompressed point format.
static constexpr bool doesUncompressedPointMatchNamedCurve(CryptoKeyEC::NamedCurve curve, size_t size)
{
    return (keySizeInBytesFromNamedCurve(curve) * 2 + 1) == size;
}

// Per Section 2.3.5 of http://www.secg.org/sec1-v2.pdf
static constexpr bool doesFieldElementMatchNamedCurve(CryptoKeyEC::NamedCurve curve, size_t size)
{
    return keySizeInBytesFromNamedCurve(curve) == size;
}

size_t CryptoKeyEC::keySizeInBits() const
{
    return keySizeInBitsFromNamedCurve(m_curve);
}

bool CryptoKeyEC::platformSupportedCurve(NamedCurve curve)
{
    return curve == NamedCurve::P256 || curve == NamedCurve::P384 || curve == NamedCurve::P521;
}

#if HAVE(SWIFT_CPP_INTEROP)
static PAL::ECCurve namedCurveToCryptoKitCurve(CryptoKeyEC::NamedCurve curve)
{
    switch (curve) {
    case CryptoKeyEC::NamedCurve::P256:
        return PAL::ECCurve::p256();
    case CryptoKeyEC::NamedCurve::P384:
        return PAL::ECCurve::p384();
    case CryptoKeyEC::NamedCurve::P521:
        return PAL::ECCurve::p521();
    }

    ASSERT_NOT_REACHED();
    return PAL::ECCurve::p256();
}
static PlatformECKeyContainer toPlatformKey(PAL::ECKey key)
{
    return makeUniqueRefWithoutFastMallocCheck<PAL::ECKey>(key);
}
#endif

std::optional<CryptoKeyPair> CryptoKeyEC::platformGeneratePair(CryptoAlgorithmIdentifier identifier, NamedCurve curve, bool extractable, CryptoKeyUsageBitmap usages)
{
#if HAVE(SWIFT_CPP_INTEROP)
    auto privateKey = CryptoKeyEC::create(identifier, curve, CryptoKeyType::Private, toPlatformKey(PAL::ECKey::init(namedCurveToCryptoKitCurve(curve))), extractable, usages);
    auto publicKey = CryptoKeyEC::create(identifier, curve, CryptoKeyType::Public, toPlatformKey(privateKey->platformKey()->toPub()), true, usages);
#else
    size_t size = keySizeInBitsFromNamedCurve(curve);
    CCECCryptorRef ccPublicKey = nullptr;
    CCECCryptorRef ccPrivateKey = nullptr;
    if (CCECCryptorGeneratePair(size, &ccPublicKey, &ccPrivateKey))
        return std::nullopt;
    auto publicKey = CryptoKeyEC::create(identifier, curve, CryptoKeyType::Public, PlatformECKeyContainer(ccPublicKey), true, usages);
    auto privateKey = CryptoKeyEC::create(identifier, curve, CryptoKeyType::Private, PlatformECKeyContainer(ccPrivateKey), extractable, usages);
#endif
    return CryptoKeyPair { WTFMove(publicKey), WTFMove(privateKey) };
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportRaw(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    if (!doesUncompressedPointMatchNamedCurve(curve, keyData.size()))
        return nullptr;
#if HAVE(SWIFT_CPP_INTEROP)
    auto rv = PAL::ECKey::importX963Pub(keyData.span(), namedCurveToCryptoKitCurve(curve));
    if (!rv.getErrorCode().isSuccess() || !rv.getKey())
        return nullptr;
    return create(identifier, curve, CryptoKeyType::Public, toPlatformKey(rv.getKey().get()), extractable, usages);
#else
    CCECCryptorRef ccPublicKey = nullptr;
    if (CCECCryptorImportKey(kCCImportKeyBinary, keyData.data(), keyData.size(), ccECKeyPublic, &ccPublicKey))
        return nullptr;
    return create(identifier, curve, CryptoKeyType::Public, PlatformECKeyContainer(ccPublicKey), extractable, usages);
#endif
}

Vector<uint8_t> CryptoKeyEC::platformExportRaw() const
{
    size_t expectedSize = 2 * keySizeInBytes() + 1; // Per Section 2.3.4 of http://www.secg.org/sec1-v2.pdf
#if HAVE(SWIFT_CPP_INTEROP)
    auto rv = platformKey()->exportX963Pub();
    if (rv.errorCode != Cpp::ErrorCodes::Success)
        return { };
    if (rv.result.size() != expectedSize)
        return { };
    return WTFMove(rv.result);
#else
    Vector<uint8_t> result(expectedSize);
    size_t size = result.size();
    if (UNLIKELY(CCECCryptorExportKey(kCCImportKeyBinary, result.data(), &size, ccECKeyPublic, platformKey().get()) || size != expectedSize))
        return { };
    return result;
#endif
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportJWKPublic(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& x, Vector<uint8_t>&& y, bool extractable, CryptoKeyUsageBitmap usages)
{
    if (!doesFieldElementMatchNamedCurve(curve, x.size()) || !doesFieldElementMatchNamedCurve(curve, y.size()))
        return nullptr;
    Vector<uint8_t> combined { InitialOctetEC };
    combined.appendVector(x);
    combined.appendVector(y);
    return platformImportRaw(identifier, curve, WTFMove(combined), extractable, usages);
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportJWKPrivate(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& x, Vector<uint8_t>&& y, Vector<uint8_t>&& d, bool extractable, CryptoKeyUsageBitmap usages)
{
    if (!doesFieldElementMatchNamedCurve(curve, x.size()) || !doesFieldElementMatchNamedCurve(curve, y.size()) || !doesFieldElementMatchNamedCurve(curve, d.size()))
        return nullptr;

    // A hack to CommonCrypto since it doesn't provide API for creating private keys directly from x, y, d.
    // BinaryInput = InitialOctetEC + X + Y + D
    Vector<uint8_t> binaryInput;
    binaryInput.append(InitialOctetEC);
    binaryInput.appendVector(x);
    binaryInput.appendVector(y);
    binaryInput.appendVector(d);
#if HAVE(SWIFT_CPP_INTEROP)
    auto rv = PAL::ECKey::importX963Private(binaryInput.span(), namedCurveToCryptoKitCurve(curve));
    if (!rv.getErrorCode().isSuccess() || !rv.getKey())
        return nullptr;
    return create(identifier, curve, CryptoKeyType::Private, toPlatformKey(rv.getKey().get()), extractable, usages);
#else
    CCECCryptorRef ccPrivateKey = nullptr;
    if (CCECCryptorImportKey(kCCImportKeyBinary, binaryInput.data(), binaryInput.size(), ccECKeyPrivate, &ccPrivateKey))
        return nullptr;
    return create(identifier, curve, CryptoKeyType::Private, PlatformECKeyContainer(ccPrivateKey), extractable, usages);
#endif
}

bool CryptoKeyEC::platformAddFieldElements(JsonWebKey& jwk) const
{
    size_t keySizeInBytes = this->keySizeInBytes();
    size_t publicKeySize = keySizeInBytes * 2 + 1; // 04 + X + Y per Section 2.3.4 of http://www.secg.org/sec1-v2.pdf
    size_t privateKeySize = keySizeInBytes * 3 + 1; // 04 + X + Y + D

    Vector<uint8_t> result(privateKeySize);
#if HAVE(SWIFT_CPP_INTEROP)
    switch (type()) {
    case CryptoKeyType::Public: {
        auto rv = platformKey()->exportX963Pub();
        if (rv.errorCode != Cpp::ErrorCodes::Success)
            return false;
        result = WTFMove(rv.result);
        break;
    }
    case CryptoKeyType::Private: {
        auto rv = platformKey()->exportX963Private();
        if (rv.errorCode != Cpp::ErrorCodes::Success)
            return false;
        result = WTFMove(rv.result);
        break;
    }
    case CryptoKeyType::Secret:
        ASSERT_NOT_REACHED();
        return false;
    }
    if (UNLIKELY((result.size() != publicKeySize) && (result.size() != privateKeySize)))
        return false;
    jwk.x = base64URLEncodeToString(result.subspan(1, keySizeInBytes));
    jwk.y = base64URLEncodeToString(result.subspan(keySizeInBytes + 1, keySizeInBytes));
    if (result.size() > publicKeySize)
        jwk.d = base64URLEncodeToString(result.subspan(publicKeySize, keySizeInBytes));
    return true;
#else
    size_t size = result.size();
    switch (type()) {
    case CryptoKeyType::Public:
        if (UNLIKELY(CCECCryptorExportKey(kCCImportKeyBinary, result.data(), &size, ccECKeyPublic, platformKey().get())))
            return false;
        break;
    case CryptoKeyType::Private:
        if (UNLIKELY(CCECCryptorExportKey(kCCImportKeyBinary, result.data(), &size, ccECKeyPrivate, platformKey().get())))
            return false;
        break;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }

    if (UNLIKELY((size != publicKeySize) && (size != privateKeySize)))
        return false;
    jwk.x = base64URLEncodeToString(result.subspan(1, keySizeInBytes));
    jwk.y = base64URLEncodeToString(result.subspan(keySizeInBytes + 1, keySizeInBytes));
    if (size > publicKeySize)
        jwk.d = base64URLEncodeToString(result.subspan(publicKeySize, keySizeInBytes));
    return true;
#endif
}

static size_t getOID(CryptoKeyEC::NamedCurve curve, const uint8_t*& oid)
{
    size_t oidSize;
    switch (curve) {
    case CryptoKeyEC::NamedCurve::P256:
        oid = Secp256r1;
        oidSize = sizeof(Secp256r1);
        break;
    case CryptoKeyEC::NamedCurve::P384:
        oid = Secp384r1;
        oidSize = sizeof(Secp384r1);
        break;
    case CryptoKeyEC::NamedCurve::P521:
        oid = Secp521r1;
        oidSize = sizeof(Secp521r1);
        break;
    }
    return oidSize;
}

// Per https://www.ietf.org/rfc/rfc5280.txt
// SubjectPublicKeyInfo ::= SEQUENCE { algorithm AlgorithmIdentifier, subjectPublicKey BIT STRING }
// AlgorithmIdentifier  ::= SEQUENCE { algorithm OBJECT IDENTIFIER, parameters ANY DEFINED BY algorithm OPTIONAL }
// Per https://www.ietf.org/rfc/rfc5480.txt
// id-ecPublicKey OBJECT IDENTIFIER ::= { iso(1) member-body(2) us(840) ansi-X9-62(10045) keyType(2) 1 }
// secp256r1 OBJECT IDENTIFIER      ::= { iso(1) member-body(2) us(840) ansi-X9-62(10045) curves(3) prime(1) 7 }
// secp384r1 OBJECT IDENTIFIER      ::= { iso(1) identified-organization(3) certicom(132) curve(0) 34 }
// secp521r1 OBJECT IDENTIFIER      ::= { iso(1) identified-organization(3) certicom(132) curve(0) 35 }
RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportSpki(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    // The following is a loose check on the provided SPKI key, it aims to extract AlgorithmIdentifier, ECParameters, and Key.
    // Once the underlying crypto library is updated to accept SPKI EC Key, we should remove this hack.
    // <rdar://problem/30987628>
    size_t index = 1; // Read SEQUENCE
    if (keyData.size() < index + 1)
        return nullptr;
    index += bytesUsedToEncodedLength(keyData[index]) + 1; // Read length, SEQUENCE
    if (keyData.size() < index + 1)
        return nullptr;
    index += bytesUsedToEncodedLength(keyData[index]); // Read length
    if (keyData.size() < index + sizeof(IdEcPublicKey))
        return nullptr;
    if (memcmp(keyData.data() + index, IdEcPublicKey, sizeof(IdEcPublicKey)))
        return nullptr;
    index += sizeof(IdEcPublicKey); // Read id-ecPublicKey
    const uint8_t* oid;
    size_t oidSize = getOID(curve, oid);
    if (keyData.size() < index + oidSize)
        return nullptr;
    if (memcmp(keyData.data() + index, oid, oidSize))
        return nullptr;
    index += oidSize + 1; // Read named curve OID, BIT STRING
    if (keyData.size() < index + 1)
        return nullptr;
    index += bytesUsedToEncodedLength(keyData[index]) + 1; // Read length
    if (doesUncompressedPointMatchNamedCurve(curve, keyData.size() - index))
        return platformImportRaw(identifier, curve, Vector<uint8_t>(keyData.subspan(index, keyData.size() - index)), extractable, usages);
#if HAVE(SWIFT_CPP_INTEROP)
    // CryptoKit can read pure compressed so no need for index++ here.
    auto rv = PAL::ECKey::importCompressedPub(keyData.subspan(index, keyData.size() - index), namedCurveToCryptoKitCurve(curve));
    if (!rv.getErrorCode().isSuccess() || !rv.getKey())
        return nullptr;
    return create(identifier, curve, CryptoKeyType::Public, toPlatformKey(rv.getKey().get()), extractable, usages);
#else
    ++index;
    CCECCryptorRef ccPublicKey = nullptr;
    if (CCECCryptorImportKey(kCCImportKeyCompact, keyData.data() + index, keyData.size() - index, ccECKeyPublic, &ccPublicKey))
        return nullptr;
    return create(identifier, curve, CryptoKeyType::Public, PlatformECKeyContainer(ccPublicKey), extractable, usages);
#endif
}

Vector<uint8_t> CryptoKeyEC::platformExportSpki() const
{
    size_t expectedKeySize = 2 * keySizeInBytes() + 1; // Per Section 2.3.4 of http://www.secg.org/sec1-v2.pdf
    Vector<uint8_t> keyBytes(expectedKeySize);
    size_t keySize = keyBytes.size();
#if HAVE(SWIFT_CPP_INTEROP)
    auto rv = platformKey()->exportX963Pub();
    if (rv.errorCode != Cpp::ErrorCodes::Success)
        return { };
    if (rv.result.size() != expectedKeySize)
        return { };
    keyBytes = WTFMove(rv.result);
    keySize = expectedKeySize;
#else
    if (UNLIKELY(CCECCryptorExportKey(kCCImportKeyBinary, keyBytes.data(), &keySize, ccECKeyPublic, platformKey().get()) || keySize != expectedKeySize))
        return { };
#endif
    // The following adds SPKI header to a raw EC public key.
    // Once the underlying crypto library is updated to output SPKI EC Key, we should remove this hack.
    // <rdar://problem/30987628>
    const uint8_t* oid;
    size_t oidSize = getOID(namedCurve(), oid);

    // SEQUENCE + length(1) + OID id-ecPublicKey + OID secp256r1/OID secp384r1/OID secp521r1 + BIT STRING + length(?) + InitialOctet + Key size
    size_t totalSize = sizeof(IdEcPublicKey) + oidSize + bytesNeededForEncodedLength(keySize + 1) + keySize + 4;

    Vector<uint8_t> result;
    result.reserveInitialCapacity(totalSize + bytesNeededForEncodedLength(totalSize) + 1);
    result.append(SequenceMark);
    addEncodedASN1Length(result, totalSize);
    result.append(SequenceMark);
    addEncodedASN1Length(result, sizeof(IdEcPublicKey) + oidSize);
    result.append(std::span { IdEcPublicKey });
    result.append(std::span { oid, oidSize });
    result.append(BitStringMark);
    addEncodedASN1Length(result, keySize + 1);
    result.append(InitialOctet);
    result.appendVector(keyBytes);

    return result;
}

// Per https://www.ietf.org/rfc/rfc5208.txt
// PrivateKeyInfo ::= SEQUENCE { version INTEGER, privateKeyAlgorithm AlgorithmIdentifier, privateKey OCTET STRING { ECPrivateKey } }
// Per https://www.ietf.org/rfc/rfc5915.txt
// ECPrivateKey ::= SEQUENCE { version INTEGER { ecPrivkeyVer1(1) }, privateKey OCTET STRING, parameters CustomECParameters, publicKey BIT STRING }
// OpenSSL uses custom ECParameters. We follow OpenSSL as a compatibility concern.
RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportPkcs8(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    // The following is a loose check on the provided PKCS8 key, it aims to extract AlgorithmIdentifier, ECParameters, and Key.
    // Once the underlying crypto library is updated to accept PKCS8 EC Key, we should remove this hack.
    // <rdar://problem/30987628>
    size_t index = 1; // Read SEQUENCE
    if (keyData.size() < index + 1)
        return nullptr;
    index += bytesUsedToEncodedLength(keyData[index]) + 4; // Read length, version, SEQUENCE
    if (keyData.size() < index + 1)
        return nullptr;
    index += bytesUsedToEncodedLength(keyData[index]); // Read length
    if (keyData.size() < index + sizeof(IdEcPublicKey))
        return nullptr;
    if (memcmp(keyData.data() + index, IdEcPublicKey, sizeof(IdEcPublicKey)))
        return nullptr;
    index += sizeof(IdEcPublicKey); // Read id-ecPublicKey
    const uint8_t* oid;
    size_t oidSize = getOID(curve, oid);
    if (keyData.size() < index + oidSize)
        return nullptr;
    if (memcmp(keyData.data() + index, oid, oidSize))
        return nullptr;
    index += oidSize + 1; // Read named curve OID, OCTET STRING
    if (keyData.size() < index + 1)
        return nullptr;
    index += bytesUsedToEncodedLength(keyData[index]) + 1; // Read length, SEQUENCE
    if (keyData.size() < index + 1)
        return nullptr;
    index += bytesUsedToEncodedLength(keyData[index]) + 4; // Read length, version, OCTET STRING
    if (keyData.size() < index + 1)
        return nullptr;
    index += bytesUsedToEncodedLength(keyData[index]); // Read length

    size_t privateKeySize = keySizeInBytesFromNamedCurve(curve);
    if (keyData.size() < index + privateKeySize)
        return nullptr;
    size_t privateKeyPos = index;
    index += privateKeySize + 1; // Read privateKey, TaggedType1
    if (keyData.size() < index + 1)
        return nullptr;
    index += bytesUsedToEncodedLength(keyData[index]) + 1; // Read length, BIT STRING
    if (keyData.size() < index + 1)
        return nullptr;
    index += bytesUsedToEncodedLength(keyData[index]) + 1; // Read length, InitialOctet

    // KeyBinary = uncompressed point + private key
    auto keyBinary = keyData.subvector(index);
    if (!doesUncompressedPointMatchNamedCurve(curve, keyBinary.size()))
        return nullptr;
    keyBinary.append(keyData.subspan(privateKeyPos, privateKeySize));
#if HAVE(SWIFT_CPP_INTEROP)
    auto rv = PAL::ECKey::importX963Private(keyBinary.span(), namedCurveToCryptoKitCurve(curve));
    if (!rv.getErrorCode().isSuccess() || !rv.getKey())
        return nullptr;
    return create(identifier, curve, CryptoKeyType::Private, toPlatformKey(rv.getKey().get()), extractable, usages);
#else
    CCECCryptorRef ccPrivateKey = nullptr;
    if (CCECCryptorImportKey(kCCImportKeyBinary, keyBinary.data(), keyBinary.size(), ccECKeyPrivate, &ccPrivateKey))
        return nullptr;
    return create(identifier, curve, CryptoKeyType::Private, PlatformECKeyContainer(ccPrivateKey), extractable, usages);
#endif
}

Vector<uint8_t> CryptoKeyEC::platformExportPkcs8() const
{
    size_t keySizeInBytes = this->keySizeInBytes();
    size_t expectedKeySize = keySizeInBytes * 3 + 1; // 04 + X + Y + D
    Vector<uint8_t> keyBytes(expectedKeySize);
#if HAVE(SWIFT_CPP_INTEROP)
    auto rv = platformKey()->exportX963Private();
    if (rv.errorCode != Cpp::ErrorCodes::Success)
        return { };
    if (rv.result.size() != expectedKeySize)
        return { };
    keyBytes = WTFMove(rv.result);
#else
    size_t keySize = keyBytes.size();
    if (UNLIKELY(CCECCryptorExportKey(kCCImportKeyBinary, keyBytes.data(), &keySize, ccECKeyPrivate, platformKey().get()) || keySize != expectedKeySize))
        return { };
#endif
    // The following addes PKCS8 header to a raw EC private key.
    // Once the underlying crypto library is updated to output PKCS8 EC Key, we should remove this hack.
    // <rdar://problem/30987628>
    const uint8_t* oid;
    size_t oidSize = getOID(namedCurve(), oid);

    // InitialOctet + 04 + X + Y
    size_t publicKeySize = keySizeInBytes * 2 + 2;
    // BIT STRING + length(?) + publicKeySize
    size_t taggedTypeSize = bytesNeededForEncodedLength(publicKeySize) + publicKeySize + 1;
    // VERSION + OCTET STRING + length(1) + private key + TaggedType1(1) + length(?) + BIT STRING + length(?) + publicKeySize
    size_t ecPrivateKeySize = sizeof(Version) + keySizeInBytes + bytesNeededForEncodedLength(taggedTypeSize) + bytesNeededForEncodedLength(publicKeySize) + publicKeySize + 4;
    // SEQUENCE + length(?) + ecPrivateKeySize
    size_t privateKeySize = bytesNeededForEncodedLength(ecPrivateKeySize) + ecPrivateKeySize + 1;
    // VERSION + SEQUENCE + length(1) + OID id-ecPublicKey + OID secp256r1/OID secp384r1/OID secp521r1 + OCTET STRING + length(?) + privateKeySize
    size_t totalSize = sizeof(Version) + sizeof(IdEcPublicKey) + oidSize + bytesNeededForEncodedLength(privateKeySize) + privateKeySize + 3;

    Vector<uint8_t> result;
    result.reserveInitialCapacity(totalSize + bytesNeededForEncodedLength(totalSize) + 1);
    result.append(SequenceMark);
    addEncodedASN1Length(result, totalSize);
    result.append(std::span { Version });
    result.append(SequenceMark);
    addEncodedASN1Length(result, sizeof(IdEcPublicKey) + oidSize);
    result.append(std::span { IdEcPublicKey });
    result.append(std::span { oid, oidSize });
    result.append(OctetStringMark);
    addEncodedASN1Length(result, privateKeySize);
    result.append(SequenceMark);
    addEncodedASN1Length(result, ecPrivateKeySize);
    result.append(std::span { PrivateKeyVersion });
    result.append(OctetStringMark);
    addEncodedASN1Length(result, keySizeInBytes);
    result.append(keyBytes.subspan(publicKeySize - 1, keySizeInBytes));
    result.append(TaggedType1);
    addEncodedASN1Length(result, taggedTypeSize);
    result.append(BitStringMark);
    addEncodedASN1Length(result, publicKeySize);
    result.append(InitialOctet);
    result.append(keyBytes.subspan(0, publicKeySize - 1));

    return result;
}

} // namespace WebCore
