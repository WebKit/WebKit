/*
 * Copyright (C) 2014 Igalia S.L. All rights reserved.
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

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoAlgorithmRegistry.h"
#include "CryptoKeyDataRSAComponents.h"
#include "CryptoKeyPair.h"
#include "ExceptionCode.h"
#include "NotImplemented.h"
#include "ScriptExecutionContext.h"
#include <pal/crypto/gcrypt/Handle.h>
#include <pal/crypto/gcrypt/Utilities.h>

namespace WebCore {

// Retrieve size of the public modulus N of the given RSA key, in bits.
static size_t getRSAModulusLength(gcry_sexp_t sexp)
{
    // The s-expression is of roughly the following form:
    // (private-key|public-key
    //   (rsa
    //     (n n-mpi)
    //     (e e-mpi)
    //     ...))
    PAL::GCrypt::Handle<gcry_sexp_t> nSexp(gcry_sexp_find_token(sexp, "n", 0));
    if (!nSexp)
        return 0;

    PAL::GCrypt::Handle<gcry_mpi_t> nMPI(gcry_sexp_nth_mpi(nSexp, 1, GCRYMPI_FMT_USG));
    if (!nMPI)
        return 0;

    size_t dataLength = 0;
    gcry_error_t error = gcry_mpi_print(GCRYMPI_FMT_USG, nullptr, 0, &dataLength, nMPI);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return 0;
    }

    return dataLength * 8;
}

static Vector<uint8_t> getParameterMPIData(gcry_mpi_t paramMPI)
{
    size_t dataLength = 0;
    gcry_error_t error = gcry_mpi_print(GCRYMPI_FMT_USG, nullptr, 0, &dataLength, paramMPI);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return { };
    }

    Vector<uint8_t> parameter(dataLength);
    error = gcry_mpi_print(GCRYMPI_FMT_USG, parameter.data(), parameter.size(), nullptr, paramMPI);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return { };
    }

    return parameter;
}

static Vector<uint8_t> getRSAKeyParameter(gcry_sexp_t sexp, const char* name)
{
    PAL::GCrypt::Handle<gcry_sexp_t> paramSexp(gcry_sexp_find_token(sexp, name, 0));
    if (!paramSexp)
        return { };

    PAL::GCrypt::Handle<gcry_mpi_t> paramMPI(gcry_sexp_nth_mpi(paramSexp, 1, GCRYMPI_FMT_USG));
    if (!paramMPI)
        return { };

    return getParameterMPIData(paramMPI);
}

RefPtr<CryptoKeyRSA> CryptoKeyRSA::create(CryptoAlgorithmIdentifier identifier, CryptoAlgorithmIdentifier hash, bool hasHash, const CryptoKeyDataRSAComponents& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    // When creating a private key, we require the p and q prime information.
    if (keyData.type() == CryptoKeyDataRSAComponents::Type::Private && !keyData.hasAdditionalPrivateKeyParameters())
        return nullptr;

    // But we don't currently support creating keys with any additional prime information.
    if (!keyData.otherPrimeInfos().isEmpty())
        return nullptr;

    // Validate the key data.
    {
        bool valid = true;

        // For both public and private keys, we need the public modulus and exponent.
        valid &= !keyData.modulus().isEmpty() && !keyData.exponent().isEmpty();

        // For private keys, we require the private exponent, as well as p and q prime information.
        if (keyData.type() == CryptoKeyDataRSAComponents::Type::Private)
            valid &= !keyData.privateExponent().isEmpty() && !keyData.firstPrimeInfo().primeFactor.isEmpty() && !keyData.secondPrimeInfo().primeFactor.isEmpty();

        if (!valid)
            return nullptr;
    }

    CryptoKeyType keyType;
    switch (keyData.type()) {
    case CryptoKeyDataRSAComponents::Type::Public:
        keyType = CryptoKeyType::Public;
        break;
    case CryptoKeyDataRSAComponents::Type::Private:
        keyType = CryptoKeyType::Private;
        break;
    }

    // Construct the key s-expression, using the data that's available.
    PAL::GCrypt::Handle<gcry_sexp_t> keySexp;
    {
        gcry_error_t error = GPG_ERR_NO_ERROR;

        switch (keyType) {
        case CryptoKeyType::Public:
            error = gcry_sexp_build(&keySexp, nullptr, "(public-key(rsa(n %b)(e %b)))",
                keyData.modulus().size(), keyData.modulus().data(),
                keyData.exponent().size(), keyData.exponent().data());
            break;
        case CryptoKeyType::Private:
            if (keyData.hasAdditionalPrivateKeyParameters()) {
                error = gcry_sexp_build(&keySexp, nullptr, "(private-key(rsa(n %b)(e %b)(d %b)(p %b)(q %b)))",
                    keyData.modulus().size(), keyData.modulus().data(),
                    keyData.exponent().size(), keyData.exponent().data(),
                    keyData.privateExponent().size(), keyData.privateExponent().data(),
                    keyData.secondPrimeInfo().primeFactor.size(), keyData.secondPrimeInfo().primeFactor.data(),
                    keyData.firstPrimeInfo().primeFactor.size(), keyData.firstPrimeInfo().primeFactor.data());
                break;
            }

            error = gcry_sexp_build(&keySexp, nullptr, "(private-key(rsa(n %b)(e %b)(d %b)))",
                keyData.modulus().size(), keyData.modulus().data(),
                keyData.exponent().size(), keyData.exponent().data(),
                keyData.privateExponent().size(), keyData.privateExponent().data());
            break;
        case CryptoKeyType::Secret:
            ASSERT_NOT_REACHED();
            return nullptr;
        }

        if (error != GPG_ERR_NO_ERROR) {
            PAL::GCrypt::logError(error);
            return nullptr;
        }
    }

    return adoptRef(new CryptoKeyRSA(identifier, hash, hasHash, keyType, keySexp.release(), extractable, usages));
}

CryptoKeyRSA::CryptoKeyRSA(CryptoAlgorithmIdentifier identifier, CryptoAlgorithmIdentifier hash, bool hasHash, CryptoKeyType type, PlatformRSAKey platformKey, bool extractable, CryptoKeyUsageBitmap usage)
    : CryptoKey(identifier, type, extractable, usage)
    , m_platformKey(platformKey)
    , m_restrictedToSpecificHash(hasHash)
    , m_hash(hash)
{
}

CryptoKeyRSA::~CryptoKeyRSA()
{
    if (m_platformKey)
        PAL::GCrypt::HandleDeleter<gcry_sexp_t>()(m_platformKey);
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
    return getRSAModulusLength(m_platformKey);
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

void CryptoKeyRSA::generatePair(CryptoAlgorithmIdentifier algorithm, CryptoAlgorithmIdentifier hash, bool hasHash, unsigned modulusLength, const Vector<uint8_t>& publicExponent, bool extractable, CryptoKeyUsageBitmap usage, KeyPairCallback&& callback, VoidCallback&& failureCallback, ScriptExecutionContext* context)
{
    // libgcrypt doesn't report an error if the exponent is smaller than three or even.
    auto e = exponentVectorToUInt32(publicExponent);
    if (!e || *e < 3 || !(*e & 0x1)) {
        failureCallback();
        return;
    }

    // libgcrypt doesn't support generating primes of less than 16 bits.
    if (modulusLength < 16) {
        failureCallback();
        return;
    }

    PAL::GCrypt::Handle<gcry_sexp_t> genkeySexp;
    gcry_error_t error = gcry_sexp_build(&genkeySexp, nullptr, "(genkey(rsa(nbits %d)(rsa-use-e %d)))", modulusLength, *e);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        failureCallback();
        return;
    }

    PAL::GCrypt::Handle<gcry_sexp_t> keyPairSexp;
    error = gcry_pk_genkey(&keyPairSexp, genkeySexp);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        failureCallback();
        return;
    }

    PAL::GCrypt::Handle<gcry_sexp_t> publicKeySexp(gcry_sexp_find_token(keyPairSexp, "public-key", 0));
    PAL::GCrypt::Handle<gcry_sexp_t> privateKeySexp(gcry_sexp_find_token(keyPairSexp, "private-key", 0));
    if (!publicKeySexp || !privateKeySexp) {
        failureCallback();
        return;
    }

    context->ref();
    context->postTask(
        [algorithm, hash, hasHash, extractable, usage, publicKeySexp = publicKeySexp.release(), privateKeySexp = privateKeySexp.release(), callback = WTFMove(callback)](ScriptExecutionContext& context) mutable {
            auto publicKey = CryptoKeyRSA::create(algorithm, hash, hasHash, CryptoKeyType::Public, publicKeySexp, true, usage);
            auto privateKey = CryptoKeyRSA::create(algorithm, hash, hasHash, CryptoKeyType::Private, privateKeySexp, extractable, usage);

            callback(CryptoKeyPair { WTFMove(publicKey), WTFMove(privateKey) });
            context.deref();
        });
}

RefPtr<CryptoKeyRSA> CryptoKeyRSA::importSpki(CryptoAlgorithmIdentifier, std::optional<CryptoAlgorithmIdentifier>, Vector<uint8_t>&&, bool, CryptoKeyUsageBitmap)
{
    notImplemented();

    return nullptr;
}

RefPtr<CryptoKeyRSA> CryptoKeyRSA::importPkcs8(CryptoAlgorithmIdentifier, std::optional<CryptoAlgorithmIdentifier>, Vector<uint8_t>&&, bool, CryptoKeyUsageBitmap)
{
    notImplemented();

    return nullptr;
}

ExceptionOr<Vector<uint8_t>> CryptoKeyRSA::exportSpki() const
{
    notImplemented();

    return Exception { NOT_SUPPORTED_ERR };
}

ExceptionOr<Vector<uint8_t>> CryptoKeyRSA::exportPkcs8() const
{
    notImplemented();

    return Exception { NOT_SUPPORTED_ERR };
}

std::unique_ptr<KeyAlgorithm> CryptoKeyRSA::buildAlgorithm() const
{
    String name = CryptoAlgorithmRegistry::singleton().name(algorithmIdentifier());
    size_t modulusLength = getRSAModulusLength(m_platformKey);
    Vector<uint8_t> publicExponent = getRSAKeyParameter(m_platformKey, "e");

    if (m_restrictedToSpecificHash)
        return std::make_unique<RsaHashedKeyAlgorithm>(name, modulusLength, WTFMove(publicExponent), CryptoAlgorithmRegistry::singleton().name(m_hash));
    return std::make_unique<RsaKeyAlgorithm>(name, modulusLength, WTFMove(publicExponent));
}

std::unique_ptr<CryptoKeyData> CryptoKeyRSA::exportData() const
{
    ASSERT(extractable());

    switch (type()) {
    case CryptoKeyType::Public:
        return CryptoKeyDataRSAComponents::createPublic(getRSAKeyParameter(m_platformKey, "n"), getRSAKeyParameter(m_platformKey, "e"));
    case CryptoKeyType::Private: {
        auto parameterMPI =
            [](gcry_sexp_t sexp, const char* name) -> gcry_mpi_t {
                PAL::GCrypt::Handle<gcry_sexp_t> paramSexp(gcry_sexp_find_token(sexp, name, 0));
                if (!paramSexp)
                    return nullptr;
                return gcry_sexp_nth_mpi(paramSexp, 1, GCRYMPI_FMT_USG);
            };

        PAL::GCrypt::Handle<gcry_mpi_t> dMPI(parameterMPI(m_platformKey, "d"));
        // libgcrypt internally uses p and q such that p < q, while usually it's q < p.
        // Switch the two primes here and continue with assuming the latter.
        PAL::GCrypt::Handle<gcry_mpi_t> pMPI(parameterMPI(m_platformKey, "q"));
        PAL::GCrypt::Handle<gcry_mpi_t> qMPI(parameterMPI(m_platformKey, "p"));
        if (!dMPI || !pMPI || !qMPI)
            return nullptr;

        CryptoKeyDataRSAComponents::PrimeInfo firstPrimeInfo { getParameterMPIData(pMPI), { }, { } };
        CryptoKeyDataRSAComponents::PrimeInfo secondPrimeInfo { getParameterMPIData(qMPI), { }, { } };

        // dp -- d mod (p - 1)
        {
            PAL::GCrypt::Handle<gcry_mpi_t> dpMPI(gcry_mpi_new(0));
            PAL::GCrypt::Handle<gcry_mpi_t> pm1MPI(gcry_mpi_new(0));
            gcry_mpi_sub_ui(pm1MPI, pMPI, 1);
            gcry_mpi_mod(dpMPI, dMPI, pm1MPI);
            firstPrimeInfo.factorCRTExponent = getParameterMPIData(dpMPI);
        }

        // dq -- d mod (q - 1)
        {
            PAL::GCrypt::Handle<gcry_mpi_t> dqMPI(gcry_mpi_new(0));
            PAL::GCrypt::Handle<gcry_mpi_t> qm1MPI(gcry_mpi_new(0));
            gcry_mpi_sub_ui(qm1MPI, qMPI, 1);
            gcry_mpi_mod(dqMPI, dMPI, qm1MPI);
            secondPrimeInfo.factorCRTExponent = getParameterMPIData(dqMPI);
        }

        // qi -- q^(-1) mod p
        {
            PAL::GCrypt::Handle<gcry_mpi_t> qiMPI(gcry_mpi_new(0));
            gcry_mpi_invm(qiMPI, qMPI, pMPI);
            secondPrimeInfo.factorCRTCoefficient = getParameterMPIData(qiMPI);
        }

        return CryptoKeyDataRSAComponents::createPrivateWithAdditionalData(getRSAKeyParameter(m_platformKey, "n"),
            getRSAKeyParameter(m_platformKey, "e"), getParameterMPIData(dMPI),
            WTFMove(firstPrimeInfo), WTFMove(secondPrimeInfo), Vector<CryptoKeyDataRSAComponents::PrimeInfo> { });
    }
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)
