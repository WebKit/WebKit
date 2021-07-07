/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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

#include "CryptoAlgorithmRegistry.h"
#include "CryptoKeyPair.h"
#include "CryptoKeyRSAComponents.h"
#include "OpenSSLUtilities.h"
#include <JavaScriptCore/TypedArrayInlines.h>
#include <openssl/X509.h>
#include <openssl/evp.h>

namespace WebCore {

static size_t getRSAModulusLength(RSA* rsa)
{
    if (!rsa)
        return 0;
    return BN_num_bytes(rsa->n) * 8;
}

RefPtr<CryptoKeyRSA> CryptoKeyRSA::create(CryptoAlgorithmIdentifier identifier, CryptoAlgorithmIdentifier hash, bool hasHash, const CryptoKeyRSAComponents& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    CryptoKeyType keyType;
    switch (keyData.type()) {
    case CryptoKeyRSAComponents::Type::Public:
        keyType = CryptoKeyType::Public;
        break;
    case CryptoKeyRSAComponents::Type::Private:
        keyType = CryptoKeyType::Private;
        break;
    default:
        return nullptr;
    }

    // When creating a private key, we require the p and q prime information.
    if (keyType == CryptoKeyType::Private && !keyData.hasAdditionalPrivateKeyParameters())
        return nullptr;

    // But we don't currently support creating keys with any additional prime information.
    if (!keyData.otherPrimeInfos().isEmpty())
        return nullptr;

    // For both public and private keys, we need the public modulus and exponent.
    if (keyData.modulus().isEmpty() || keyData.exponent().isEmpty())
        return nullptr;

    // For private keys, we require the private exponent, as well as p and q prime information.
    if (keyType == CryptoKeyType::Private) {
        if (keyData.privateExponent().isEmpty() || keyData.firstPrimeInfo().primeFactor.isEmpty() || keyData.secondPrimeInfo().primeFactor.isEmpty())
            return nullptr;
    }

    auto rsa = RSAPtr(RSA_new());
    if (!rsa)
        return nullptr;

    rsa->n = convertToBigNumber(rsa->n, keyData.modulus());
    rsa->e = convertToBigNumber(rsa->e, keyData.exponent());
    if (!rsa->n || !rsa->e)
        return nullptr;

    if (keyType == CryptoKeyType::Private) {
        rsa->d = convertToBigNumber(rsa->d, keyData.privateExponent());
        rsa->p = convertToBigNumber(rsa->p, keyData.firstPrimeInfo().primeFactor);
        rsa->q = convertToBigNumber(rsa->q, keyData.secondPrimeInfo().primeFactor);
        if (!rsa->d || !rsa->p || !rsa->q)
            return nullptr;

        // We set dmp1, dmpq1, and iqmp member of the RSA struct if the keyData has corresponding data.

        // dmp1 -- d mod (p - 1)
        if (!keyData.firstPrimeInfo().factorCRTExponent.isEmpty())
            rsa->dmp1 = convertToBigNumber(rsa->dmp1, keyData.firstPrimeInfo().factorCRTExponent);
        // dmq1 -- d mod (q - 1)
        if (!keyData.secondPrimeInfo().factorCRTExponent.isEmpty())
            rsa->dmq1 = convertToBigNumber(rsa->dmq1, keyData.secondPrimeInfo().factorCRTExponent);
        // iqmp -- q^(-1) mod p
        if (!keyData.secondPrimeInfo().factorCRTCoefficient.isEmpty())
            rsa->iqmp = convertToBigNumber(rsa->iqmp, keyData.secondPrimeInfo().factorCRTCoefficient);
    }

    auto pkey = EvpPKeyPtr(EVP_PKEY_new());
    if (!pkey)
        return nullptr;

    if (EVP_PKEY_set1_RSA(pkey.get(), rsa.get()) != 1)
        return nullptr;

    return adoptRef(new CryptoKeyRSA(identifier, hash, hasHash, keyType, WTFMove(pkey), extractable, usages));
}

CryptoKeyRSA::CryptoKeyRSA(CryptoAlgorithmIdentifier identifier, CryptoAlgorithmIdentifier hash, bool hasHash, CryptoKeyType type, PlatformRSAKeyContainer&& platformKey, bool extractable, CryptoKeyUsageBitmap usages)
    : CryptoKey(identifier, type, extractable, usages)
    , m_platformKey(WTFMove(platformKey))
    , m_restrictedToSpecificHash(hasHash)
    , m_hash(hash)
{
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
    RSA* rsa = EVP_PKEY_get0_RSA(m_platformKey.get());
    if (!rsa)
        return 0;

    return getRSAModulusLength(rsa);
}

// Convert the exponent vector to a 32-bit value, if possible.
static std::optional<uint32_t> exponentVectorToUInt32(const Vector<uint8_t>& exponent)
{
    if (exponent.size() > 4) {
        if (std::any_of(exponent.begin(), exponent.end() - 4, [](uint8_t element) { return !!element; }))
            return std::nullopt;
    }

    uint32_t result = 0;
    for (size_t size = exponent.size(), i = std::min<size_t>(4, size); i > 0; --i) {
        result <<= 8;
        result += exponent[size - i];
    }

    return result;
}

void CryptoKeyRSA::generatePair(CryptoAlgorithmIdentifier algorithm, CryptoAlgorithmIdentifier hash, bool hasHash, unsigned modulusLength, const Vector<uint8_t>& publicExponent, bool extractable, CryptoKeyUsageBitmap usages, KeyPairCallback&& callback, VoidCallback&& failureCallback, ScriptExecutionContext*)
{
    // OpenSSL doesn't report an error if the exponent is smaller than three or even.
    auto e = exponentVectorToUInt32(publicExponent);
    if (!e || *e < 3 || !(*e & 0x1)) {
        failureCallback();
        return;
    }

    auto exponent = BIGNUMPtr(convertToBigNumber(nullptr, publicExponent));
    auto privateRSA = RSAPtr(RSA_new());
    if (!exponent || RSA_generate_key_ex(privateRSA.get(), modulusLength, exponent.get(), nullptr) <= 0) {
        failureCallback();
        return;
    }

    auto publicRSA = RSAPtr(RSAPublicKey_dup(privateRSA.get()));
    if (!publicRSA) {
        failureCallback();
        return;
    }

    auto privatePKey = EvpPKeyPtr(EVP_PKEY_new());
    if (EVP_PKEY_set1_RSA(privatePKey.get(), privateRSA.get()) <= 0) {
        failureCallback();
        return;
    }

    auto publicPKey = EvpPKeyPtr(EVP_PKEY_new());
    if (EVP_PKEY_set1_RSA(publicPKey.get(), publicRSA.get()) <= 0) {
        failureCallback();
        return;
    }

    auto publicKey = CryptoKeyRSA::create(algorithm, hash, hasHash, CryptoKeyType::Public, WTFMove(publicPKey), true, usages);
    auto privateKey = CryptoKeyRSA::create(algorithm, hash, hasHash, CryptoKeyType::Private, WTFMove(privatePKey), extractable, usages);
    callback(CryptoKeyPair { WTFMove(publicKey), WTFMove(privateKey) });
}

RefPtr<CryptoKeyRSA> CryptoKeyRSA::importSpki(CryptoAlgorithmIdentifier identifier, std::optional<CryptoAlgorithmIdentifier> hash, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    // We need a local pointer variable to pass to d2i (DER to internal) functions().
    const uint8_t* ptr = keyData.data();

    // We use d2i_PUBKEY() to import a public key.
    auto pkey = EvpPKeyPtr(d2i_PUBKEY(nullptr, &ptr, keyData.size()));
    if (!pkey || EVP_PKEY_type(pkey->type) != EVP_PKEY_RSA)
        return nullptr;

    return adoptRef(new CryptoKeyRSA(identifier, hash.value_or(CryptoAlgorithmIdentifier::SHA_1), !!hash, CryptoKeyType::Public, WTFMove(pkey), extractable, usages));
}

RefPtr<CryptoKeyRSA> CryptoKeyRSA::importPkcs8(CryptoAlgorithmIdentifier identifier, std::optional<CryptoAlgorithmIdentifier> hash, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    // We need a local pointer variable to pass to d2i (DER to internal) functions().
    const uint8_t* ptr = keyData.data();

    // We use d2i_PKCS8_PRIV_KEY_INFO() to import a private key.
    auto p8inf = PKCS8PrivKeyInfoPtr(d2i_PKCS8_PRIV_KEY_INFO(nullptr, &ptr, keyData.size()));
    if (!p8inf)
        return nullptr;

    auto pkey = EvpPKeyPtr(EVP_PKCS82PKEY(p8inf.get()));
    if (!pkey || EVP_PKEY_type(pkey->type) != EVP_PKEY_RSA)
        return nullptr;

    return adoptRef(new CryptoKeyRSA(identifier, hash.value_or(CryptoAlgorithmIdentifier::SHA_1), !!hash, CryptoKeyType::Private, WTFMove(pkey), extractable, usages));
}

ExceptionOr<Vector<uint8_t>> CryptoKeyRSA::exportSpki() const
{
    if (type() != CryptoKeyType::Public)
        return Exception { InvalidAccessError };

    int len = i2d_PUBKEY(platformKey(), nullptr);
    if (len < 0)
        return Exception { OperationError };

    Vector<uint8_t> keyData(len);
    auto ptr = keyData.data();
    if (i2d_PUBKEY(platformKey(), &ptr) < 0)
        return Exception { OperationError };

    return keyData;
}

ExceptionOr<Vector<uint8_t>> CryptoKeyRSA::exportPkcs8() const
{
    if (type() != CryptoKeyType::Private)
        return Exception { InvalidAccessError };

    auto p8inf = PKCS8PrivKeyInfoPtr(EVP_PKEY2PKCS8(platformKey()));
    if (!p8inf)
        return Exception { OperationError };

    int len = i2d_PKCS8_PRIV_KEY_INFO(p8inf.get(), nullptr);
    if (len < 0)
        return Exception { OperationError };

    Vector<uint8_t> keyData(len);
    auto ptr = keyData.data();
    if (i2d_PKCS8_PRIV_KEY_INFO(p8inf.get(), &ptr) < 0)
        return Exception { OperationError };

    return keyData;
}

auto CryptoKeyRSA::algorithm() const -> KeyAlgorithm
{
    RSA* rsa = EVP_PKEY_get0_RSA(platformKey());

    auto modulusLength = rsa ? BN_num_bytes(rsa->n) * 8 : 0;
    auto publicExponent = rsa ? convertToBytes(rsa->e) : Vector<uint8_t> { };

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
    RSA* rsa = EVP_PKEY_get0_RSA(platformKey());
    if (!rsa)
        return nullptr;

    switch (type()) {
    case CryptoKeyType::Public:
        // We need the public modulus and exponent for the public key.
        if (!rsa->n || !rsa->e)
            return nullptr;
        return CryptoKeyRSAComponents::createPublic(convertToBytes(rsa->n), convertToBytes(rsa->e));
    case CryptoKeyType::Private: {
        // We need the public modulus, exponent, and private exponent, as well as p and q prime information.
        if (!rsa->n || !rsa->e || !rsa->d || !rsa->p || !rsa->q)
            return nullptr;

        CryptoKeyRSAComponents::PrimeInfo firstPrimeInfo;
        firstPrimeInfo.primeFactor = convertToBytes(rsa->p);

        CryptoKeyRSAComponents::PrimeInfo secondPrimeInfo;
        secondPrimeInfo.primeFactor = convertToBytes(rsa->q);

        auto context = BNCtxPtr(BN_CTX_new());
        // dmp1 -- d mod (p - 1)
        if (rsa->dmp1)
            firstPrimeInfo.factorCRTExponent = convertToBytes(rsa->dmp1);
        else {
            auto dmp1 = BIGNUMPtr(BN_new());
            auto pm1 = BIGNUMPtr(BN_dup(rsa->p));
            if (BN_sub_word(pm1.get(), 1) == 1 && BN_mod(dmp1.get(), rsa->d, pm1.get(), context.get()) == 1)
                firstPrimeInfo.factorCRTExponent = convertToBytes(dmp1.get());
        }

        // dmq1 -- d mod (q - 1)
        if (rsa->dmq1)
            secondPrimeInfo.factorCRTExponent = convertToBytes(rsa->dmq1);
        else {
            auto dmq1 = BIGNUMPtr(BN_new());
            auto qm1 = BIGNUMPtr(BN_dup(rsa->q));
            if (BN_sub_word(qm1.get(), 1) == 1 && BN_mod(dmq1.get(), rsa->d, qm1.get(), context.get()) == 1)
                secondPrimeInfo.factorCRTExponent = convertToBytes(dmq1.get());
        }

        // iqmp -- q^(-1) mod p
        if (rsa->iqmp)
            secondPrimeInfo.factorCRTCoefficient = convertToBytes(rsa->iqmp);
        else {
            auto iqmp = BIGNUMPtr(BN_mod_inverse(nullptr, rsa->q, rsa->p, context.get()));
            if (iqmp)
                secondPrimeInfo.factorCRTCoefficient = convertToBytes(iqmp.get());
        }

        return CryptoKeyRSAComponents::createPrivateWithAdditionalData(
            convertToBytes(rsa->n), convertToBytes(rsa->e), convertToBytes(rsa->d),
            WTFMove(firstPrimeInfo), WTFMove(secondPrimeInfo), Vector<CryptoKeyRSAComponents::PrimeInfo> { });
    }
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO)
