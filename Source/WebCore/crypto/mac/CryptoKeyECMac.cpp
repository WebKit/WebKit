/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_CRYPTO)

#include "CommonCryptoDERUtilities.h"
#include "CommonCryptoUtilities.h"
#include "JsonWebKey.h"
#include <wtf/text/Base64.h>

namespace WebCore {

static const unsigned char InitialOctetEC = 0x04; // Per Section 2.3.3 of http://www.secg.org/sec1-v2.pdf
// OID id-ecPublicKey 1.2.840.10045.2.1.
static const unsigned char IdEcPublicKey[] = {0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01};
// OID secp256r1 1.2.840.10045.3.1.7.
static constexpr unsigned char Secp256r1[] = {0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07};
// OID secp384r1 1.3.132.0.34
static constexpr unsigned char Secp384r1[] = {0x06, 0x05, 0x2b, 0x81, 0x04, 0x00, 0x22};
// Version 1. Per https://tools.ietf.org/html/rfc5915#section-3
static const unsigned char PrivateKeyVersion[] = {0x02, 0x01, 0x01};
// Tagged type [1]
static const unsigned char TaggedType1 = 0xa1;

// Per Section 2.3.4 of http://www.secg.org/sec1-v2.pdf
// We only support uncompressed point format.
static bool doesUncompressedPointMatchNamedCurve(CryptoKeyEC::NamedCurve curve, size_t size)
{
    switch (curve) {
    case CryptoKeyEC::NamedCurve::P256:
        return size == 65;
    case CryptoKeyEC::NamedCurve::P384:
        return size == 97;
    case CryptoKeyEC::NamedCurve::P521:
        break;
    }

    ASSERT_NOT_REACHED();
    return false;
}

// Per Section 2.3.5 of http://www.secg.org/sec1-v2.pdf
static bool doesFieldElementMatchNamedCurve(CryptoKeyEC::NamedCurve curve, size_t size)
{
    switch (curve) {
    case CryptoKeyEC::NamedCurve::P256:
        return size == 32;
    case CryptoKeyEC::NamedCurve::P384:
        return size == 48;
    case CryptoKeyEC::NamedCurve::P521:
        break;
    }

    ASSERT_NOT_REACHED();
    return false;
}

static size_t getKeySizeFromNamedCurve(CryptoKeyEC::NamedCurve curve)
{
    switch (curve) {
    case CryptoKeyEC::NamedCurve::P256:
        return 256;
    case CryptoKeyEC::NamedCurve::P384:
        return 384;
    case CryptoKeyEC::NamedCurve::P521:
        break;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

CryptoKeyEC::~CryptoKeyEC()
{
    CCECCryptorRelease(m_platformKey);
}

size_t CryptoKeyEC::keySizeInBits() const
{
    int result = CCECGetKeySize(m_platformKey);
    return result ? result : 0;
}

bool CryptoKeyEC::platformSupportedCurve(NamedCurve curve)
{
    return curve == NamedCurve::P256 || curve == NamedCurve::P384;
}

std::optional<CryptoKeyPair> CryptoKeyEC::platformGeneratePair(CryptoAlgorithmIdentifier identifier, NamedCurve curve, bool extractable, CryptoKeyUsageBitmap usages)
{
    size_t size = getKeySizeFromNamedCurve(curve);
    CCECCryptorRef ccPublicKey;
    CCECCryptorRef ccPrivateKey;
    if (CCECCryptorGeneratePair(size, &ccPublicKey, &ccPrivateKey))
        return std::nullopt;

    auto publicKey = CryptoKeyEC::create(identifier, curve, CryptoKeyType::Public, ccPublicKey, true, usages);
    auto privateKey = CryptoKeyEC::create(identifier, curve, CryptoKeyType::Private, ccPrivateKey, extractable, usages);
    return CryptoKeyPair { WTFMove(publicKey), WTFMove(privateKey) };
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportRaw(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    if (!doesUncompressedPointMatchNamedCurve(curve, keyData.size()))
        return nullptr;

    CCECCryptorRef ccPublicKey;
    if (CCECCryptorImportKey(kCCImportKeyBinary, keyData.data(), keyData.size(), ccECKeyPublic, &ccPublicKey))
        return nullptr;

    return create(identifier, curve, CryptoKeyType::Public, ccPublicKey, extractable, usages);
}

Vector<uint8_t> CryptoKeyEC::platformExportRaw() const
{
    size_t expectedSize = keySizeInBits() / 4 + 1; // Per Section 2.3.4 of http://www.secg.org/sec1-v2.pdf
    Vector<uint8_t> result(expectedSize);
    size_t size = result.size();
    if (UNLIKELY(CCECCryptorExportKey(kCCImportKeyBinary, result.data(), &size, ccECKeyPublic, m_platformKey) || size != expectedSize))
        return { };
    return result;
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportJWKPublic(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& x, Vector<uint8_t>&& y, bool extractable, CryptoKeyUsageBitmap usages)
{
    if (!doesFieldElementMatchNamedCurve(curve, x.size()) || !doesFieldElementMatchNamedCurve(curve, y.size()))
        return nullptr;

    size_t size = getKeySizeFromNamedCurve(curve);
    CCECCryptorRef ccPublicKey;
    if (CCECCryptorCreateFromData(size, x.data(), x.size(), y.data(), y.size(), &ccPublicKey))
        return nullptr;

    return create(identifier, curve, CryptoKeyType::Public, ccPublicKey, extractable, usages);
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

    CCECCryptorRef ccPrivateKey;
    if (CCECCryptorImportKey(kCCImportKeyBinary, binaryInput.data(), binaryInput.size(), ccECKeyPrivate, &ccPrivateKey))
        return nullptr;

    return create(identifier, curve, CryptoKeyType::Private, ccPrivateKey, extractable, usages);
}

bool CryptoKeyEC::platformAddFieldElements(JsonWebKey& jwk) const
{
    size_t keySizeInBytes = keySizeInBits() / 8;
    size_t publicKeySize = keySizeInBytes * 2 + 1; // 04 + X + Y per Section 2.3.4 of http://www.secg.org/sec1-v2.pdf
    size_t privateKeySize = keySizeInBytes * 3 + 1; // 04 + X + Y + D

    Vector<uint8_t> result(privateKeySize);
    size_t size = result.size();
    switch (type()) {
    case CryptoKeyType::Public:
        if (UNLIKELY(CCECCryptorExportKey(kCCImportKeyBinary, result.data(), &size, ccECKeyPublic, m_platformKey)))
            return false;
        break;
    case CryptoKeyType::Private:
        if (UNLIKELY(CCECCryptorExportKey(kCCImportKeyBinary, result.data(), &size, ccECKeyPrivate, m_platformKey)))
            return false;
        break;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }

    if (UNLIKELY((size != publicKeySize) && (size != privateKeySize)))
        return false;
    jwk.x = WTF::base64URLEncode(result.data() + 1, keySizeInBytes);
    jwk.y = WTF::base64URLEncode(result.data() + keySizeInBytes + 1, keySizeInBytes);
    if (size > publicKeySize)
        jwk.d = WTF::base64URLEncode(result.data() + publicKeySize, keySizeInBytes);
    return true;
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
        ASSERT_NOT_REACHED();
        oid = nullptr;
        oidSize = 0;
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
    index += bytesUsedToEncodedLength(keyData[index]) + 1; // Read length, InitialOctet

    if (!doesUncompressedPointMatchNamedCurve(curve, keyData.size() - index))
        return nullptr;

    CCECCryptorRef ccPublicKey;
    if (CCECCryptorImportKey(kCCImportKeyBinary, keyData.data() + index, keyData.size() - index, ccECKeyPublic, &ccPublicKey))
        return nullptr;

    return create(identifier, curve, CryptoKeyType::Public, ccPublicKey, extractable, usages);
}

Vector<uint8_t> CryptoKeyEC::platformExportSpki() const
{
    size_t expectedKeySize = keySizeInBits() / 4 + 1; // Per Section 2.3.4 of http://www.secg.org/sec1-v2.pdf
    Vector<uint8_t> keyBytes(expectedKeySize);
    size_t keySize = keyBytes.size();
    if (UNLIKELY(CCECCryptorExportKey(kCCImportKeyBinary, keyBytes.data(), &keySize, ccECKeyPublic, m_platformKey) || keySize != expectedKeySize))
        return { };

    // The following addes SPKI header to a raw EC public key.
    // Once the underlying crypto library is updated to output SPKI EC Key, we should remove this hack.
    // <rdar://problem/30987628>
    const uint8_t* oid;
    size_t oidSize = getOID(namedCurve(), oid);

    // SEQUENCE + length(1) + OID id-ecPublicKey + OID secp256r1/OID secp384r1 + BIT STRING + length(?) + InitialOctet + Key size
    size_t totalSize = sizeof(IdEcPublicKey) + oidSize + bytesNeededForEncodedLength(keySize + 1) + keySize + 4;

    Vector<uint8_t> result;
    result.reserveCapacity(totalSize + bytesNeededForEncodedLength(totalSize) + 1);
    result.append(SequenceMark);
    addEncodedASN1Length(result, totalSize);
    result.append(SequenceMark);
    addEncodedASN1Length(result, sizeof(IdEcPublicKey) + oidSize);
    result.append(IdEcPublicKey, sizeof(IdEcPublicKey));
    result.append(oid, oidSize);
    result.append(BitStringMark);
    addEncodedASN1Length(result, keySize + 1);
    result.append(InitialOctet);
    result.append(keyBytes.data(), keyBytes.size());

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

    if (keyData.size() < index + getKeySizeFromNamedCurve(curve) / 8)
        return nullptr;
    size_t privateKeyPos = index;
    index += getKeySizeFromNamedCurve(curve) / 8 + 1; // Read privateKey, TaggedType1
    if (keyData.size() < index + 1)
        return nullptr;
    index += bytesUsedToEncodedLength(keyData[index]) + 1; // Read length, BIT STRING
    if (keyData.size() < index + 1)
        return nullptr;
    index += bytesUsedToEncodedLength(keyData[index]) + 1; // Read length, InitialOctet

    // KeyBinary = uncompressed point + private key
    Vector<uint8_t> keyBinary;
    keyBinary.append(keyData.data() + index, keyData.size() - index);
    if (!doesUncompressedPointMatchNamedCurve(curve, keyBinary.size()))
        return nullptr;
    keyBinary.append(keyData.data() + privateKeyPos, getKeySizeFromNamedCurve(curve) / 8);

    CCECCryptorRef ccPrivateKey;
    if (CCECCryptorImportKey(kCCImportKeyBinary, keyBinary.data(), keyBinary.size(), ccECKeyPrivate, &ccPrivateKey))
        return nullptr;

    return create(identifier, curve, CryptoKeyType::Private, ccPrivateKey, extractable, usages);
}

Vector<uint8_t> CryptoKeyEC::platformExportPkcs8() const
{
    size_t keySizeInBytes = keySizeInBits() / 8;
    size_t expectedKeySize = keySizeInBytes * 3 + 1; // 04 + X + Y + D
    Vector<uint8_t> keyBytes(expectedKeySize);
    size_t keySize = keyBytes.size();
    if (UNLIKELY(CCECCryptorExportKey(kCCImportKeyBinary, keyBytes.data(), &keySize, ccECKeyPrivate, m_platformKey) || keySize != expectedKeySize))
        return { };

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
    // VERSION + SEQUENCE + length(1) + OID id-ecPublicKey + OID secp256r1/OID secp384r1 + OCTET STRING + length(?) + privateKeySize
    size_t totalSize = sizeof(Version) + sizeof(IdEcPublicKey) + oidSize + bytesNeededForEncodedLength(privateKeySize) + privateKeySize + 3;

    Vector<uint8_t> result;
    result.reserveCapacity(totalSize + bytesNeededForEncodedLength(totalSize) + 1);
    result.append(SequenceMark);
    addEncodedASN1Length(result, totalSize);
    result.append(Version, sizeof(Version));
    result.append(SequenceMark);
    addEncodedASN1Length(result, sizeof(IdEcPublicKey) + oidSize);
    result.append(IdEcPublicKey, sizeof(IdEcPublicKey));
    result.append(oid, oidSize);
    result.append(OctetStringMark);
    addEncodedASN1Length(result, privateKeySize);
    result.append(SequenceMark);
    addEncodedASN1Length(result, ecPrivateKeySize);
    result.append(PrivateKeyVersion, sizeof(PrivateKeyVersion));
    result.append(OctetStringMark);
    addEncodedASN1Length(result, keySizeInBytes);
    result.append(keyBytes.data() + publicKeySize - 1, keySizeInBytes);
    result.append(TaggedType1);
    addEncodedASN1Length(result, taggedTypeSize);
    result.append(BitStringMark);
    addEncodedASN1Length(result, publicKeySize);
    result.append(InitialOctet);
    result.append(keyBytes.data(), publicKeySize - 1);

    return result;
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO)
