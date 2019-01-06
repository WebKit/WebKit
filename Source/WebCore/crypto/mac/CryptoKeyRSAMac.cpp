/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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
#include "CryptoKeyRSA.h"

#if ENABLE(WEB_CRYPTO)

#include "CommonCryptoDERUtilities.h"
#include "CommonCryptoUtilities.h"
#include "CryptoAlgorithmRegistry.h"
#include "CryptoKeyPair.h"
#include "CryptoKeyRSAComponents.h"
#include "ScriptExecutionContext.h"
#include <JavaScriptCore/GenericTypedArrayViewInlines.h>
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/JSGenericTypedArrayViewInlines.h>
#include <wtf/MainThread.h>

namespace WebCore {

#if (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED < 110000) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 101300)
inline uint8_t* castDataArgumentToCCRSACryptorCreateFromDataIfNeeded(const uint8_t* value)
{
    return const_cast<uint8_t*>(value);
}
#else
inline const uint8_t* castDataArgumentToCCRSACryptorCreateFromDataIfNeeded(const uint8_t* value)
{
    return value;
}
#endif

// OID rsaEncryption: 1.2.840.113549.1.1.1. Per https://tools.ietf.org/html/rfc3279#section-2.3.1
static const unsigned char RSAOIDHeader[] = {0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00};

// FIXME: We should get rid of magic number 16384. It assumes that the length of provided key will not exceed 16KB.
// https://bugs.webkit.org/show_bug.cgi?id=164942
static CCCryptorStatus getPublicKeyComponents(const PlatformRSAKeyContainer& rsaKey, Vector<uint8_t>& modulus, Vector<uint8_t>& publicExponent)
{
    ASSERT(CCRSAGetKeyType(rsaKey.get()) == ccRSAKeyPublic || CCRSAGetKeyType(rsaKey.get()) == ccRSAKeyPrivate);
    bool keyIsPublic = CCRSAGetKeyType(rsaKey.get()) == ccRSAKeyPublic;
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    PlatformRSAKeyContainer publicKeyFromPrivateKey(keyIsPublic ? nullptr : CCRSACryptorGetPublicKeyFromPrivateKey(rsaKey.get()));
    ALLOW_DEPRECATED_DECLARATIONS_END

    modulus.resize(16384);
    size_t modulusLength = modulus.size();
    publicExponent.resize(16384);
    size_t exponentLength = publicExponent.size();
    CCCryptorStatus status = CCRSAGetKeyComponents(keyIsPublic ? rsaKey.get() : publicKeyFromPrivateKey.get(), modulus.data(), &modulusLength, publicExponent.data(), &exponentLength, 0, 0, 0, 0);
    if (status)
        return status;

    modulus.shrink(modulusLength);
    publicExponent.shrink(exponentLength);
    return status;
}

static CCCryptorStatus getPrivateKeyComponents(const PlatformRSAKeyContainer& rsaKey, Vector<uint8_t>& privateExponent, CryptoKeyRSAComponents::PrimeInfo& firstPrimeInfo, CryptoKeyRSAComponents::PrimeInfo& secondPrimeInfo)
{
    ASSERT(CCRSAGetKeyType(rsaKey.get()) == ccRSAKeyPrivate);

    Vector<uint8_t> unusedModulus(16384);
    size_t modulusLength = unusedModulus.size();
    privateExponent.resize(16384);
    size_t exponentLength = privateExponent.size();
    firstPrimeInfo.primeFactor.resize(16384);
    size_t pLength = firstPrimeInfo.primeFactor.size();
    secondPrimeInfo.primeFactor.resize(16384);
    size_t qLength = secondPrimeInfo.primeFactor.size();

    CCCryptorStatus status = CCRSAGetKeyComponents(rsaKey.get(), unusedModulus.data(), &modulusLength, privateExponent.data(), &exponentLength, firstPrimeInfo.primeFactor.data(), &pLength, secondPrimeInfo.primeFactor.data(), &qLength);
    if (status)
        return status;

    privateExponent.shrink(exponentLength);
    firstPrimeInfo.primeFactor.shrink(pLength);
    secondPrimeInfo.primeFactor.shrink(qLength);

#if HAVE(CCRSAGetCRTComponents)
    size_t dpSize;
    size_t dqSize;
    size_t qinvSize;
    if (auto status = CCRSAGetCRTComponentsSizes(rsaKey.get(), &dpSize, &dqSize, &qinvSize))
        return status;

    Vector<uint8_t> dp(dpSize);
    Vector<uint8_t> dq(dqSize);
    Vector<uint8_t> qinv(qinvSize);
    if (auto status = CCRSAGetCRTComponents(rsaKey.get(), dp.data(), dpSize, dq.data(), dqSize, qinv.data(), qinvSize))
        return status;

    firstPrimeInfo.factorCRTExponent = WTFMove(dp);
    secondPrimeInfo.factorCRTExponent = WTFMove(dq);
    secondPrimeInfo.factorCRTCoefficient = WTFMove(qinv);
#else
    CCBigNum d(privateExponent.data(), privateExponent.size());
    CCBigNum p(firstPrimeInfo.primeFactor.data(), firstPrimeInfo.primeFactor.size());
    CCBigNum q(secondPrimeInfo.primeFactor.data(), secondPrimeInfo.primeFactor.size());

    CCBigNum dp = d % (p - 1);
    CCBigNum dq = d % (q - 1);
    CCBigNum qi = q.inverse(p);

    firstPrimeInfo.factorCRTExponent = dp.data();
    secondPrimeInfo.factorCRTExponent = dq.data();
    secondPrimeInfo.factorCRTCoefficient = qi.data();
#endif

    return status;
}

CryptoKeyRSA::CryptoKeyRSA(CryptoAlgorithmIdentifier identifier, CryptoAlgorithmIdentifier hash, bool hasHash, CryptoKeyType type, PlatformRSAKeyContainer&& platformKey, bool extractable, CryptoKeyUsageBitmap usage)
    : CryptoKey(identifier, type, extractable, usage)
    , m_platformKey(WTFMove(platformKey))
    , m_restrictedToSpecificHash(hasHash)
    , m_hash(hash)
{
}

RefPtr<CryptoKeyRSA> CryptoKeyRSA::create(CryptoAlgorithmIdentifier identifier, CryptoAlgorithmIdentifier hash, bool hasHash, const CryptoKeyRSAComponents& keyData, bool extractable, CryptoKeyUsageBitmap usage)
{
    if (keyData.type() == CryptoKeyRSAComponents::Type::Private && !keyData.hasAdditionalPrivateKeyParameters()) {
        // <rdar://problem/15452324> tracks adding support.
        WTFLogAlways("Private keys without additional data are not supported");
        return nullptr;
    }
    if (keyData.otherPrimeInfos().size()) {
        // <rdar://problem/15444074> tracks adding support.
        WTFLogAlways("Keys with more than two primes are not supported");
        return nullptr;
    }
    // When an empty vector p is provided to CCRSACryptorCreateFromData to create a private key, it crashes.
    // <rdar://problem/30550228> tracks the issue.
    if (keyData.type() == CryptoKeyRSAComponents::Type::Private && keyData.firstPrimeInfo().primeFactor.isEmpty())
        return nullptr;

    CCRSACryptorRef cryptor = nullptr;
    // FIXME: It is so weired that we recaculate the private exponent from first prime factor and second prime factor,
    // given the fact that we have already had it. Also, the re-caculated private exponent may not match the given one.
    // See <rdar://problem/15452324>.
    CCCryptorStatus status = CCRSACryptorCreateFromData(
        keyData.type() == CryptoKeyRSAComponents::Type::Public ? ccRSAKeyPublic : ccRSAKeyPrivate,
        castDataArgumentToCCRSACryptorCreateFromDataIfNeeded(keyData.modulus().data()), keyData.modulus().size(),
        castDataArgumentToCCRSACryptorCreateFromDataIfNeeded(keyData.exponent().data()), keyData.exponent().size(),
        castDataArgumentToCCRSACryptorCreateFromDataIfNeeded(keyData.firstPrimeInfo().primeFactor.data()), keyData.firstPrimeInfo().primeFactor.size(),
        castDataArgumentToCCRSACryptorCreateFromDataIfNeeded(keyData.secondPrimeInfo().primeFactor.data()), keyData.secondPrimeInfo().primeFactor.size(),
        &cryptor);

    if (status) {
        LOG_ERROR("Couldn't create RSA key from data, error %d", status);
        return nullptr;
    }

    return adoptRef(new CryptoKeyRSA(identifier, hash, hasHash, keyData.type() == CryptoKeyRSAComponents::Type::Public ? CryptoKeyType::Public : CryptoKeyType::Private, PlatformRSAKeyContainer(cryptor), extractable, usage));
}

bool CryptoKeyRSA::isRestrictedToHash(CryptoAlgorithmIdentifier& identifier) const
{
    if (!m_restrictedToSpecificHash)
        return false;

    identifier = m_hash;
    return true;
}

size_t CryptoKeyRSA::keySizeInBits() const
{
    Vector<uint8_t> modulus;
    Vector<uint8_t> publicExponent;
    CCCryptorStatus status = getPublicKeyComponents(m_platformKey, modulus, publicExponent);
    if (status) {
        WTFLogAlways("Couldn't get RSA key components, status %d", status);
        return 0;
    }

    return modulus.size() * 8;
}

auto CryptoKeyRSA::algorithm() const -> KeyAlgorithm
{
    // FIXME: Add a version of getPublicKeyComponents that returns Uint8Array, rather
    // than Vector<uint8_t>, to avoid a copy of the data.

    Vector<uint8_t> modulus;
    Vector<uint8_t> publicExponent;
    CCCryptorStatus status = getPublicKeyComponents(m_platformKey, modulus, publicExponent);
    if (status) {
        WTFLogAlways("Couldn't get RSA key components, status %d", status);
        publicExponent.clear();

        CryptoRsaKeyAlgorithm result;
        result.name = CryptoAlgorithmRegistry::singleton().name(algorithmIdentifier());
        result.modulusLength = 0;
        result.publicExponent = Uint8Array::tryCreate(0);
        return result;
    }

    size_t modulusLength = modulus.size() * 8;

    if (m_restrictedToSpecificHash) {
        CryptoRsaHashedKeyAlgorithm result;
        result.name = CryptoAlgorithmRegistry::singleton().name(algorithmIdentifier());
        result.modulusLength = modulusLength;
        result.publicExponent = Uint8Array::tryCreate(publicExponent.data(), publicExponent.size());
        result.hash.name = CryptoAlgorithmRegistry::singleton().name(m_hash);
        return result;
    }
    
    CryptoRsaKeyAlgorithm result;
    result.name = CryptoAlgorithmRegistry::singleton().name(algorithmIdentifier());
    result.modulusLength = modulusLength;
    result.publicExponent = Uint8Array::tryCreate(publicExponent.data(), publicExponent.size());
    return result;
}

std::unique_ptr<CryptoKeyRSAComponents> CryptoKeyRSA::exportData() const
{
    switch (CCRSAGetKeyType(m_platformKey.get())) {
    case ccRSAKeyPublic: {
        Vector<uint8_t> modulus;
        Vector<uint8_t> publicExponent;
        CCCryptorStatus status = getPublicKeyComponents(m_platformKey, modulus, publicExponent);
        if (status) {
            WTFLogAlways("Couldn't get RSA key components, status %d", status);
            return nullptr;
        }
        return CryptoKeyRSAComponents::createPublic(modulus, publicExponent);
    }
    case ccRSAKeyPrivate: {
        Vector<uint8_t> modulus;
        Vector<uint8_t> publicExponent;
        CCCryptorStatus status = getPublicKeyComponents(m_platformKey, modulus, publicExponent);
        if (status) {
            WTFLogAlways("Couldn't get RSA key components, status %d", status);
            return nullptr;
        }
        Vector<uint8_t> privateExponent;
        CryptoKeyRSAComponents::PrimeInfo firstPrimeInfo;
        CryptoKeyRSAComponents::PrimeInfo secondPrimeInfo;
        Vector<CryptoKeyRSAComponents::PrimeInfo> otherPrimeInfos; // Always empty, CommonCrypto only supports two primes (cf. <rdar://problem/15444074>).
        status = getPrivateKeyComponents(m_platformKey, privateExponent, firstPrimeInfo, secondPrimeInfo);
        if (status) {
            WTFLogAlways("Couldn't get RSA key components, status %d", status);
            return nullptr;
        }
        return CryptoKeyRSAComponents::createPrivateWithAdditionalData(modulus, publicExponent, privateExponent, firstPrimeInfo, secondPrimeInfo, otherPrimeInfos);
    }
    default:
        return nullptr;
    }
}

static bool bigIntegerToUInt32(const Vector<uint8_t>& bigInteger, uint32_t& result)
{
    result = 0;
    for (size_t i = 0; i + 4 < bigInteger.size(); ++i) {
        if (bigInteger[i])
            return false; // Too big to fit in 32 bits.
    }

    for (size_t i = bigInteger.size() > 4 ? bigInteger.size() - 4 : 0; i < bigInteger.size(); ++i) {
        result <<= 8;
        result += bigInteger[i];
    }
    return true;
}

// FIXME: We should use WorkQueue here instead of dispatch_async once WebKitSubtleCrypto is deprecated.
// https://bugs.webkit.org/show_bug.cgi?id=164943
void CryptoKeyRSA::generatePair(CryptoAlgorithmIdentifier algorithm, CryptoAlgorithmIdentifier hash, bool hasHash, unsigned modulusLength, const Vector<uint8_t>& publicExponent, bool extractable, CryptoKeyUsageBitmap usage, KeyPairCallback&& callback, VoidCallback&& failureCallback, ScriptExecutionContext* context)
{
    uint32_t e;
    if (!bigIntegerToUInt32(publicExponent, e)) {
        // Adding support is tracked as <rdar://problem/15444034>.
        WTFLogAlways("Public exponent is too big, not supported");
        failureCallback();
        return;
    }

    // We only use the callback functions when back on the main/worker thread, but captured variables are copied on a secondary thread too.
    KeyPairCallback* localCallback = new KeyPairCallback(WTFMove(callback));
    VoidCallback* localFailureCallback = new VoidCallback(WTFMove(failureCallback));
    auto contextIdentifier = context->contextIdentifier();

    // FIXME: There is a risk that localCallback and localFailureCallback are never freed.
    // Fix this by using unique pointers and move them from one lambda to the other.
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        CCRSACryptorRef ccPublicKey = nullptr;
        CCRSACryptorRef ccPrivateKey = nullptr;
        CCCryptorStatus status = CCRSACryptorGeneratePair(modulusLength, e, &ccPublicKey, &ccPrivateKey);
        if (status) {
            WTFLogAlways("Could not generate a key pair, status %d", status);
            ScriptExecutionContext::postTaskTo(contextIdentifier, [localCallback, localFailureCallback](auto&) {
                (*localFailureCallback)();
                delete localCallback;
                delete localFailureCallback;
            });
            return;
        }
        ScriptExecutionContext::postTaskTo(contextIdentifier, [algorithm, hash, hasHash, extractable, usage, localCallback, localFailureCallback, ccPublicKey = PlatformRSAKeyContainer(ccPublicKey), ccPrivateKey = PlatformRSAKeyContainer(ccPrivateKey)](auto&) mutable {
            auto publicKey = CryptoKeyRSA::create(algorithm, hash, hasHash, CryptoKeyType::Public, WTFMove(ccPublicKey), true, usage);
            auto privateKey = CryptoKeyRSA::create(algorithm, hash, hasHash, CryptoKeyType::Private, WTFMove(ccPrivateKey), extractable, usage);

            (*localCallback)(CryptoKeyPair { WTFMove(publicKey), WTFMove(privateKey) });

            delete localCallback;
            delete localFailureCallback;
        });
    });
}

RefPtr<CryptoKeyRSA> CryptoKeyRSA::importSpki(CryptoAlgorithmIdentifier identifier, Optional<CryptoAlgorithmIdentifier> hash, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    // The current SecLibrary cannot import a SPKI format binary. Hence, we need to strip out the SPKI header.
    // This hack can be removed when <rdar://problem/29523286> is resolved.
    // The header format we assume is: SequenceMark(1) + Length(?) + rsaEncryption(15) + BitStringMark(1) + Length(?) + InitialOctet(1).
    // The header format could be varied. However since we don't have a full-fledged ASN.1 encoder/decoder, we want to restrict it to
    // the most common one for now.
    // Per https://tools.ietf.org/html/rfc5280#section-4.1. subjectPublicKeyInfo.
    size_t headerSize = 1;
    if (keyData.size() < headerSize + 1)
        return nullptr;
    headerSize += bytesUsedToEncodedLength(keyData[headerSize]) + sizeof(RSAOIDHeader) + sizeof(BitStringMark);
    if (keyData.size() < headerSize + 1)
        return nullptr;
    headerSize += bytesUsedToEncodedLength(keyData[headerSize]) + sizeof(InitialOctet);

    CCRSACryptorRef ccPublicKey = nullptr;
    if (CCRSACryptorImport(keyData.data() + headerSize, keyData.size() - headerSize, &ccPublicKey))
        return nullptr;

    // Notice: CryptoAlgorithmIdentifier::SHA_1 is just a placeholder. It should not have any effect if hash is WTF::nullopt.
    return adoptRef(new CryptoKeyRSA(identifier, hash.valueOr(CryptoAlgorithmIdentifier::SHA_1), !!hash, CryptoKeyType::Public, PlatformRSAKeyContainer(ccPublicKey), extractable, usages));
}

ExceptionOr<Vector<uint8_t>> CryptoKeyRSA::exportSpki() const
{
    if (type() != CryptoKeyType::Public)
        return Exception { InvalidAccessError };

    // The current SecLibrary cannot output a valid SPKI format binary. Hence, we need the following hack.
    // This hack can be removed when <rdar://problem/29523286> is resolved.
    // Estimated size in produced bytes format. Per https://tools.ietf.org/html/rfc3279#section-2.3.1. RSAPublicKey.
    // O(size) = Sequence(1) + Length(3) + Integer(1) + Length(3) + Modulus + Integer(1) + Length(3) + Exponent
    Vector<uint8_t> keyBytes(keySizeInBits() / 4);
    size_t keySize = keyBytes.size();
    if (CCRSACryptorExport(platformKey(), keyBytes.data(), &keySize))
        return Exception { OperationError };
    keyBytes.shrink(keySize);

    // RSAOIDHeader + BitStringMark + Length + keySize + InitialOctet
    size_t totalSize = sizeof(RSAOIDHeader) + bytesNeededForEncodedLength(keySize + 1) + keySize + 2;

    // Per https://tools.ietf.org/html/rfc5280#section-4.1. subjectPublicKeyInfo.
    Vector<uint8_t> result;
    result.reserveCapacity(totalSize + bytesNeededForEncodedLength(totalSize) + 1);
    result.append(SequenceMark);
    addEncodedASN1Length(result, totalSize);
    result.append(RSAOIDHeader, sizeof(RSAOIDHeader));
    result.append(BitStringMark);
    addEncodedASN1Length(result, keySize + 1);
    result.append(InitialOctet);
    result.append(keyBytes.data(), keyBytes.size());

    return WTFMove(result);
}

RefPtr<CryptoKeyRSA> CryptoKeyRSA::importPkcs8(CryptoAlgorithmIdentifier identifier, Optional<CryptoAlgorithmIdentifier> hash, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    // The current SecLibrary cannot import a PKCS8 format binary. Hence, we need to strip out the PKCS8 header.
    // This hack can be removed when <rdar://problem/29523286> is resolved.
    // The header format we assume is: SequenceMark(1) + Length(?) + Version(3) + rsaEncryption(15) + OctetStringMark(1) + Length(?).
    // The header format could be varied. However since we don't have a full-fledged ASN.1 encoder/decoder, we want to restrict it to
    // the most common one for now.
    // Per https://tools.ietf.org/html/rfc5208#section-5. PrivateKeyInfo.
    // We also assume there is no optional parameters.
    size_t headerSize = 1;
    if (keyData.size() < headerSize + 1)
        return nullptr;
    headerSize += bytesUsedToEncodedLength(keyData[headerSize]) + sizeof(Version) + sizeof(RSAOIDHeader) + sizeof(OctetStringMark);
    if (keyData.size() < headerSize + 1)
        return nullptr;
    headerSize += bytesUsedToEncodedLength(keyData[headerSize]);

    CCRSACryptorRef ccPrivateKey = nullptr;
    if (CCRSACryptorImport(keyData.data() + headerSize, keyData.size() - headerSize, &ccPrivateKey))
        return nullptr;

    // Notice: CryptoAlgorithmIdentifier::SHA_1 is just a placeholder. It should not have any effect if hash is WTF::nullopt.
    return adoptRef(new CryptoKeyRSA(identifier, hash.valueOr(CryptoAlgorithmIdentifier::SHA_1), !!hash, CryptoKeyType::Private, PlatformRSAKeyContainer(ccPrivateKey), extractable, usages));
}

ExceptionOr<Vector<uint8_t>> CryptoKeyRSA::exportPkcs8() const
{
    if (type() != CryptoKeyType::Private)
        return Exception { InvalidAccessError };

    // The current SecLibrary cannot output a valid PKCS8 format binary. Hence, we need the following hack.
    // This hack can be removed when <rdar://problem/29523286> is resolved.
    // Estimated size in produced bytes format. Per https://tools.ietf.org/html/rfc3447#appendix-A.1.2. RSAPrivateKey.
    // O(size) = Sequence(1) + Length(3) + Integer(1) + Length(3) + Modulus + Integer(1) + Length(3) + publicExponent + Integer(1) + Length(3) +
    // privateExponent + Integer(1) + Length(3) + prime1 + Integer(1) + Length(3) + prime2 + Integer(1) + Length(3) + exponent1 + Integer(1) +
    // Length(3) + exponent2 + Integer(1) + Length(3) + coefficient.
    Vector<uint8_t> keyBytes(keySizeInBits());
    size_t keySize = keyBytes.size();
    if (CCRSACryptorExport(platformKey(), keyBytes.data(), &keySize))
        return Exception { OperationError };
    keyBytes.shrink(keySize);

    // Version + RSAOIDHeader + OctetStringMark + Length + keySize
    size_t totalSize = sizeof(Version) + sizeof(RSAOIDHeader) + bytesNeededForEncodedLength(keySize) + keySize + 1;

    // Per https://tools.ietf.org/html/rfc5208#section-5. PrivateKeyInfo.
    Vector<uint8_t> result;
    result.reserveCapacity(totalSize + bytesNeededForEncodedLength(totalSize) + 1);
    result.append(SequenceMark);
    addEncodedASN1Length(result, totalSize);
    result.append(Version, sizeof(Version));
    result.append(RSAOIDHeader, sizeof(RSAOIDHeader));
    result.append(OctetStringMark);
    addEncodedASN1Length(result, keySize);
    result.append(keyBytes.data(), keyBytes.size());

    return WTFMove(result);
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO)
