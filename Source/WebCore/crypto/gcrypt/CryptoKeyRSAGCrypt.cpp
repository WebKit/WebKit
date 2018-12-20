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

#if ENABLE(WEB_CRYPTO)

#include "CryptoAlgorithmRegistry.h"
#include "CryptoKeyPair.h"
#include "CryptoKeyRSAComponents.h"
#include "GCryptUtilities.h"
#include "ScriptExecutionContext.h"
#include <JavaScriptCore/GenericTypedArrayViewInlines.h>
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/JSGenericTypedArrayViewInlines.h>
#include <pal/crypto/gcrypt/Handle.h>
#include <pal/crypto/gcrypt/Utilities.h>
#include <pal/crypto/tasn1/Utilities.h>

namespace WebCore {

static size_t getRSAModulusLength(gcry_sexp_t keySexp)
{
    // Retrieve the s-expression token for the public modulus N of the given RSA key.
    PAL::GCrypt::Handle<gcry_sexp_t> nSexp(gcry_sexp_find_token(keySexp, "n", 0));
    if (!nSexp)
        return 0;

    // Retrieve the MPI length for the corresponding s-expression token, in bits.
    auto length = mpiLength(nSexp);
    if (!length)
        return 0;

    return *length * 8;
}

static Vector<uint8_t> getRSAKeyParameter(gcry_sexp_t keySexp, const char* name)
{
    // Retrieve the s-expression token for the specified parameter of the given RSA key.
    PAL::GCrypt::Handle<gcry_sexp_t> paramSexp(gcry_sexp_find_token(keySexp, name, 0));
    if (!paramSexp)
        return { };

    // Retrieve the MPI data for the corresponding s-expression token.
    auto data = mpiData(paramSexp);
    if (!data)
        return { };

    return WTFMove(data.value());
}

RefPtr<CryptoKeyRSA> CryptoKeyRSA::create(CryptoAlgorithmIdentifier identifier, CryptoAlgorithmIdentifier hash, bool hasHash, const CryptoKeyRSAComponents& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    // When creating a private key, we require the p and q prime information.
    if (keyData.type() == CryptoKeyRSAComponents::Type::Private && !keyData.hasAdditionalPrivateKeyParameters())
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
        if (keyData.type() == CryptoKeyRSAComponents::Type::Private)
            valid &= !keyData.privateExponent().isEmpty() && !keyData.firstPrimeInfo().primeFactor.isEmpty() && !keyData.secondPrimeInfo().primeFactor.isEmpty();

        if (!valid)
            return nullptr;
    }

    CryptoKeyType keyType;
    switch (keyData.type()) {
    case CryptoKeyRSAComponents::Type::Public:
        keyType = CryptoKeyType::Public;
        break;
    case CryptoKeyRSAComponents::Type::Private:
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
static Optional<uint32_t> exponentVectorToUInt32(const Vector<uint8_t>& exponent)
{
    if (exponent.size() > 4) {
        if (std::any_of(exponent.begin(), exponent.end() - 4, [](uint8_t element) { return !!element; }))
            return WTF::nullopt;
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

    context->postTask(
        [algorithm, hash, hasHash, extractable, usage, publicKeySexp = publicKeySexp.release(), privateKeySexp = privateKeySexp.release(), callback = WTFMove(callback)](auto&) mutable {
            auto publicKey = CryptoKeyRSA::create(algorithm, hash, hasHash, CryptoKeyType::Public, publicKeySexp, true, usage);
            auto privateKey = CryptoKeyRSA::create(algorithm, hash, hasHash, CryptoKeyType::Private, privateKeySexp, extractable, usage);

            callback(CryptoKeyPair { WTFMove(publicKey), WTFMove(privateKey) });
        });
}

static bool supportedAlgorithmIdentifier(const uint8_t* data, size_t size)
{
    // FIXME: This is far from sufficient. Per the spec, when importing for key algorithm
    // - RSASSA-PKCS1-v1_5:
    //     - rsaEncryption, sha{1,256,384,512}WithRSAEncryption OIDs must be supported
    //     - in case of sha{1,256,384,512}WithRSAEncryption OIDs the specified hash algorithm
    //       has to match the algorithm in the OID
    // - RSA-PSS:
    //     - rsaEncryption, id-RSASSA-PSS OIDs must be supported
    //     - in case of id-RSASSA-PSS OID the parameters field of AlgorithmIdentifier has
    //       to be decoded as RSASSA-PSS-params ASN.1 structure, and the hashAlgorithm field
    //       of that structure has to contain one of id-sha{1,256,384,512} OIDs that match
    //       the specified hash algorithm
    // - RSA-OAEP:
    //     - rsaEncryption, id-RSAES-OAEP OIDS must be supported
    //     - in case of id-RSAES-OAEP OID the parameters field of AlgorithmIdentifier has
    //       to be decoded as RSAES-OAEP-params ASN.1 structure, and the hashAlgorithm field
    //       of that structure has to contain one of id-sha{1,256,384,512} OIDs that match
    //       the specified hash algorithm

    if (CryptoConstants::matches(data, size, CryptoConstants::s_rsaEncryptionIdentifier))
        return true;
    if (CryptoConstants::matches(data, size, CryptoConstants::s_RSAES_OAEPIdentifier))
        return false; // Not yet supported.
    if (CryptoConstants::matches(data, size, CryptoConstants::s_RSASSA_PSSIdentifier))
        return false; // Not yet supported.
    return false;
}

RefPtr<CryptoKeyRSA> CryptoKeyRSA::importSpki(CryptoAlgorithmIdentifier identifier, Optional<CryptoAlgorithmIdentifier> hash, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    // Decode the `SubjectPublicKeyInfo` structure using the provided key data.
    PAL::TASN1::Structure spki;
    if (!PAL::TASN1::decodeStructure(&spki, "WebCrypto.SubjectPublicKeyInfo", keyData))
        return nullptr;

    // Validate `algorithm.algorithm`.
    {
        auto algorithm = PAL::TASN1::elementData(spki, "algorithm.algorithm");
        if (!algorithm)
            return nullptr;

        if (!supportedAlgorithmIdentifier(algorithm->data(), algorithm->size()))
            return nullptr;
    }

    // Decode the `RSAPublicKey` structure using the `subjectPublicKey` data.
    PAL::TASN1::Structure rsaPublicKey;
    {
        auto subjectPublicKey = PAL::TASN1::elementData(spki, "subjectPublicKey");
        if (!subjectPublicKey)
            return nullptr;

        if (!PAL::TASN1::decodeStructure(&rsaPublicKey, "WebCrypto.RSAPublicKey", *subjectPublicKey))
            return nullptr;
    }

    // Retrieve the `modulus` and `publicExponent` data and embed it into the `public-key` s-expression.
    PAL::GCrypt::Handle<gcry_sexp_t> platformKey;
    {
        auto modulus = PAL::TASN1::elementData(rsaPublicKey, "modulus");
        auto publicExponent = PAL::TASN1::elementData(rsaPublicKey, "publicExponent");
        if (!modulus || !publicExponent)
            return nullptr;

        gcry_error_t error = gcry_sexp_build(&platformKey, nullptr, "(public-key(rsa(n %b)(e %b)))",
            modulus->size(), modulus->data(), publicExponent->size(), publicExponent->data());
        if (error != GPG_ERR_NO_ERROR) {
            PAL::GCrypt::logError(error);
            return nullptr;
        }
    }

    return adoptRef(new CryptoKeyRSA(identifier, hash.value_or(CryptoAlgorithmIdentifier::SHA_1), !!hash, CryptoKeyType::Public, platformKey.release(), extractable, usages));
}

RefPtr<CryptoKeyRSA> CryptoKeyRSA::importPkcs8(CryptoAlgorithmIdentifier identifier, Optional<CryptoAlgorithmIdentifier> hash, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    // Decode the `PrivateKeyInfo` structure using the provided key data.
    PAL::TASN1::Structure pkcs8;
    if (!PAL::TASN1::decodeStructure(&pkcs8, "WebCrypto.PrivateKeyInfo", keyData))
        return nullptr;

    // Validate `version`.
    {
        auto version = PAL::TASN1::elementData(pkcs8, "version");
        if (!version)
            return nullptr;

        if (!CryptoConstants::matches(version->data(), version->size(), CryptoConstants::s_asn1Version0))
            return nullptr;
    }

    // Validate `privateKeyAlgorithm.algorithm`.
    {
        auto algorithm = PAL::TASN1::elementData(pkcs8, "privateKeyAlgorithm.algorithm");
        if (!algorithm)
            return nullptr;

        if (!supportedAlgorithmIdentifier(algorithm->data(), algorithm->size()))
            return nullptr;
    }

    // Decode the `RSAPrivateKey` structure using the `privateKey` data.
    PAL::TASN1::Structure rsaPrivateKey;
    {
        auto privateKey = PAL::TASN1::elementData(pkcs8, "privateKey");
        if (!privateKey)
            return nullptr;

        if (!PAL::TASN1::decodeStructure(&rsaPrivateKey, "WebCrypto.RSAPrivateKey", *privateKey))
            return nullptr;
    }

    // Validate `privateKey.version`.
    {
        auto version = PAL::TASN1::elementData(rsaPrivateKey, "version");
        if (!version)
            return nullptr;

        if (!CryptoConstants::matches(version->data(), version->size(), CryptoConstants::s_asn1Version0))
            return nullptr;
    }

    // Retrieve the `modulus`, `publicExponent`, `privateExponent`, `prime1`, `prime2`,
    // `exponent1`, `exponent2` and `coefficient` data and embed it into the `public-key` s-expression.
    PAL::GCrypt::Handle<gcry_sexp_t> platformKey;
    {
        auto modulus = PAL::TASN1::elementData(rsaPrivateKey, "modulus");
        auto publicExponent = PAL::TASN1::elementData(rsaPrivateKey, "publicExponent");
        auto privateExponent = PAL::TASN1::elementData(rsaPrivateKey, "privateExponent");
        auto prime1 = PAL::TASN1::elementData(rsaPrivateKey, "prime1");
        auto prime2 = PAL::TASN1::elementData(rsaPrivateKey, "prime2");
        auto exponent1 = PAL::TASN1::elementData(rsaPrivateKey, "exponent1");
        auto exponent2 = PAL::TASN1::elementData(rsaPrivateKey, "exponent2");
        auto coefficient = PAL::TASN1::elementData(rsaPrivateKey, "coefficient");

        if (!modulus || !publicExponent || !privateExponent
            || !prime1 || !prime2 || !exponent1 || !exponent2 || !coefficient)
            return nullptr;

        // libgcrypt inverts the use of p and q parameters, so we have to recalculate the `coefficient` value.
        PAL::GCrypt::Handle<gcry_mpi_t> uMPI(gcry_mpi_new(0));
        {
            PAL::GCrypt::Handle<gcry_mpi_t> pMPI;
            gcry_error_t error = gcry_mpi_scan(&pMPI, GCRYMPI_FMT_USG, prime1->data(), prime1->size(), nullptr);
            if (error != GPG_ERR_NO_ERROR)
                return nullptr;

            PAL::GCrypt::Handle<gcry_mpi_t> qMPI;
            error = gcry_mpi_scan(&qMPI, GCRYMPI_FMT_USG, prime2->data(), prime2->size(), nullptr);
            if (error != GPG_ERR_NO_ERROR)
                return nullptr;

            gcry_mpi_invm(uMPI, qMPI, pMPI);
        }

        gcry_error_t error = gcry_sexp_build(&platformKey, nullptr, "(private-key(rsa(n %b)(e %b)(d %b)(p %b)(q %b)(u %M)))",
            modulus->size(), modulus->data(),
            publicExponent->size(), publicExponent->data(),
            privateExponent->size(), privateExponent->data(),
            prime2->size(), prime2->data(), prime1->size(), prime1->data(), uMPI.handle());
        if (error != GPG_ERR_NO_ERROR) {
            PAL::GCrypt::logError(error);
            return nullptr;
        }
    }

    return adoptRef(new CryptoKeyRSA(identifier, hash.value_or(CryptoAlgorithmIdentifier::SHA_1), !!hash, CryptoKeyType::Private, platformKey.release(), extractable, usages));
}

ExceptionOr<Vector<uint8_t>> CryptoKeyRSA::exportSpki() const
{
    if (type() != CryptoKeyType::Public)
        return Exception { InvalidAccessError };

    PAL::TASN1::Structure rsaPublicKey;
    {
        // Create the `RSAPublicKey` structure.
        if (!PAL::TASN1::createStructure("WebCrypto.RSAPublicKey", &rsaPublicKey))
            return Exception { OperationError };

        // Retrieve the modulus and public exponent s-expressions.
        PAL::GCrypt::Handle<gcry_sexp_t> modulusSexp(gcry_sexp_find_token(m_platformKey, "n", 0));
        PAL::GCrypt::Handle<gcry_sexp_t> publicExponentSexp(gcry_sexp_find_token(m_platformKey, "e", 0));
        if (!modulusSexp || !publicExponentSexp)
            return Exception { OperationError };

        // Retrieve MPI data for the modulus and public exponent components.
        auto modulus = mpiSignedData(modulusSexp);
        auto publicExponent = mpiSignedData(publicExponentSexp);
        if (!modulus || !publicExponent)
            return Exception { OperationError };

        // Write out the modulus data under `modulus`.
        if (!PAL::TASN1::writeElement(rsaPublicKey, "modulus", modulus->data(), modulus->size()))
            return Exception { OperationError };

        // Write out the public exponent data under `publicExponent`.
        if (!PAL::TASN1::writeElement(rsaPublicKey, "publicExponent", publicExponent->data(), publicExponent->size()))
            return Exception { OperationError };
    }

    PAL::TASN1::Structure spki;
    {
        // Create the `SubjectPublicKeyInfo` structure.
        if (!PAL::TASN1::createStructure("WebCrypto.SubjectPublicKeyInfo", &spki))
            return Exception { OperationError };

        // Write out the id-rsaEncryption identifier under `algorithm.algorithm`.
        // FIXME: In case the key algorithm is:
        // - RSA-PSS:
        //     - this should write out id-RSASSA-PSS, along with setting `algorithm.parameters`
        //       to a RSASSA-PSS-params structure
        // - RSA-OAEP:
        //     - this should write out id-RSAES-OAEP, along with setting `algorithm.parameters`
        //       to a RSAES-OAEP-params structure
        if (!PAL::TASN1::writeElement(spki, "algorithm.algorithm", CryptoConstants::s_rsaEncryptionIdentifier.data(), 1))
            return Exception { OperationError };

        // Write out the null value under `algorithm.parameters`.
        if (!PAL::TASN1::writeElement(spki, "algorithm.parameters", CryptoConstants::s_asn1NullValue.data(), CryptoConstants::s_asn1NullValue.size()))
            return Exception { OperationError };

        // Write out the `RSAPublicKey` data under `subjectPublicKey`. Because this is a
        // bit string parameter, the data size has to be multiplied by 8.
        {
            auto data = PAL::TASN1::encodedData(rsaPublicKey, "");
            if (!data || !PAL::TASN1::writeElement(spki, "subjectPublicKey", data->data(), data->size() * 8))
                return Exception { OperationError };
        }
    }

    // Retrieve the encoded `SubjectPublicKeyInfo` data and return it.
    auto result = PAL::TASN1::encodedData(spki, "");
    if (!result)
        return Exception { OperationError };

    return WTFMove(result.value());
}

ExceptionOr<Vector<uint8_t>> CryptoKeyRSA::exportPkcs8() const
{
    if (type() != CryptoKeyType::Private)
        return Exception { InvalidAccessError };

    PAL::TASN1::Structure rsaPrivateKey;
    {
        // Create the `RSAPrivateKey` structure.
        if (!PAL::TASN1::createStructure("WebCrypto.RSAPrivateKey", &rsaPrivateKey))
            return Exception { OperationError };

        // Write out '0' under `version`.
        if (!PAL::TASN1::writeElement(rsaPrivateKey, "version", "0", 0))
            return Exception { OperationError };

        // Retrieve the `n`, `e`, `d`, `q` and `p` s-expression tokens. libgcrypt swaps the usage of
        // the p and q primes internally, so we adjust the lookup accordingly.
        PAL::GCrypt::Handle<gcry_sexp_t> nSexp(gcry_sexp_find_token(m_platformKey, "n", 0));
        PAL::GCrypt::Handle<gcry_sexp_t> eSexp(gcry_sexp_find_token(m_platformKey, "e", 0));
        PAL::GCrypt::Handle<gcry_sexp_t> dSexp(gcry_sexp_find_token(m_platformKey, "d", 0));
        PAL::GCrypt::Handle<gcry_sexp_t> pSexp(gcry_sexp_find_token(m_platformKey, "q", 0));
        PAL::GCrypt::Handle<gcry_sexp_t> qSexp(gcry_sexp_find_token(m_platformKey, "p", 0));
        if (!nSexp || !eSexp || !dSexp || !pSexp || !qSexp)
            return Exception { OperationError };

        // Write the MPI data of retrieved s-expression tokens under `modulus`, `publicExponent`,
        // `privateExponent`, `prime1` and `prime2`.
        {
            auto modulus = mpiSignedData(nSexp);
            auto publicExponent = mpiSignedData(eSexp);
            auto privateExponent = mpiSignedData(dSexp);
            auto prime1 = mpiSignedData(pSexp);
            auto prime2 = mpiSignedData(qSexp);
            if (!modulus || !publicExponent || !privateExponent || !prime1 || !prime2)
                return Exception { OperationError };

            if (!PAL::TASN1::writeElement(rsaPrivateKey, "modulus", modulus->data(), modulus->size())
                || !PAL::TASN1::writeElement(rsaPrivateKey, "publicExponent", publicExponent->data(), publicExponent->size())
                || !PAL::TASN1::writeElement(rsaPrivateKey, "privateExponent", privateExponent->data(), privateExponent->size())
                || !PAL::TASN1::writeElement(rsaPrivateKey, "prime1", prime1->data(), prime1->size())
                || !PAL::TASN1::writeElement(rsaPrivateKey, "prime2", prime2->data(), prime2->size()))
                return Exception { OperationError };
        }

        // Manually compute the MPI values for the `exponent1`, `exponent2` and `coefficient`
        // parameters. Again note the swapped usage of the `p` and `q` s-expression parameters.
        {
            PAL::GCrypt::Handle<gcry_mpi_t> dMPI(gcry_sexp_nth_mpi(dSexp, 1, GCRYMPI_FMT_USG));
            PAL::GCrypt::Handle<gcry_mpi_t> pMPI(gcry_sexp_nth_mpi(pSexp, 1, GCRYMPI_FMT_USG));
            PAL::GCrypt::Handle<gcry_mpi_t> qMPI(gcry_sexp_nth_mpi(qSexp, 1, GCRYMPI_FMT_USG));
            if (!dMPI || !pMPI || !qMPI)
                return Exception { OperationError };

            // `exponent1`
            {
                PAL::GCrypt::Handle<gcry_mpi_t> dpMPI(gcry_mpi_set_ui(nullptr, 0));
                PAL::GCrypt::Handle<gcry_mpi_t> pm1MPI(gcry_mpi_set(nullptr, pMPI));
                gcry_mpi_sub_ui(pm1MPI, pm1MPI, 1);
                gcry_mpi_mod(dpMPI, dMPI, pm1MPI);

                auto dp = mpiSignedData(dpMPI);
                if (!dp || !PAL::TASN1::writeElement(rsaPrivateKey, "exponent1", dp->data(), dp->size()))
                    return Exception { OperationError };
            }

            // `exponent2`
            {
                PAL::GCrypt::Handle<gcry_mpi_t> dqMPI(gcry_mpi_set_ui(nullptr, 0));
                PAL::GCrypt::Handle<gcry_mpi_t> qm1MPI(gcry_mpi_set(nullptr, qMPI));
                gcry_mpi_sub_ui(qm1MPI, qm1MPI, 1);
                gcry_mpi_mod(dqMPI, dMPI, qm1MPI);

                auto dq = mpiSignedData(dqMPI);
                if (!dq || !PAL::TASN1::writeElement(rsaPrivateKey, "exponent2", dq->data(), dq->size()))
                    return Exception { OperationError };
            }

            // `coefficient`
            {
                PAL::GCrypt::Handle<gcry_mpi_t> qiMPI(gcry_mpi_set_ui(nullptr, 0));
                gcry_mpi_invm(qiMPI, qMPI, pMPI);

                auto qi = mpiSignedData(qiMPI);
                if (!qi || !PAL::TASN1::writeElement(rsaPrivateKey, "coefficient", qi->data(), qi->size()))
                    return Exception { OperationError };
            }
        }

        // Eliminate the optional `otherPrimeInfos` element.
        // FIXME: this should be supported in the future, if there is such information available.
        if (!PAL::TASN1::writeElement(rsaPrivateKey, "otherPrimeInfos", nullptr, 0))
            return Exception { OperationError };
    }

    PAL::TASN1::Structure pkcs8;
    {
        // Create the `PrivateKeyInfo` structure.
        if (!PAL::TASN1::createStructure("WebCrypto.PrivateKeyInfo", &pkcs8))
            return Exception { OperationError };

        // Write out '0' under `version`.
        if (!PAL::TASN1::writeElement(pkcs8, "version", "0", 0))
            return Exception { OperationError };

        // Write out the id-rsaEncryption identifier under `algorithm.algorithm`.
        // FIXME: In case the key algorithm is:
        // - RSA-PSS:
        //     - this should write out id-RSASSA-PSS, along with setting `algorithm.parameters`
        //       to a RSASSA-PSS-params structure
        // - RSA-OAEP:
        //     - this should write out id-RSAES-OAEP, along with setting `algorithm.parameters`
        //       to a RSAES-OAEP-params structure
        if (!PAL::TASN1::writeElement(pkcs8, "privateKeyAlgorithm.algorithm", "1.2.840.113549.1.1.1", 1))
            return Exception { OperationError };

        // Write out a null value under `algorithm.parameters`.
        if (!PAL::TASN1::writeElement(pkcs8, "privateKeyAlgorithm.parameters", CryptoConstants::s_asn1NullValue.data(), CryptoConstants::s_asn1NullValue.size()))
            return Exception { OperationError };

        // Write out the `RSAPrivateKey` data under `privateKey`.
        {
            auto data = PAL::TASN1::encodedData(rsaPrivateKey, "");
            if (!data || !PAL::TASN1::writeElement(pkcs8, "privateKey", data->data(), data->size()))
                return Exception { OperationError };
        }

        // Eliminate the optional `attributes` element.
        if (!PAL::TASN1::writeElement(pkcs8, "attributes", nullptr, 0))
            return Exception { OperationError };
    }

    // Retrieve the encoded `PrivateKeyInfo` data and return it.
    auto result = PAL::TASN1::encodedData(pkcs8, "");
    if (!result)
        return Exception { OperationError };

    return WTFMove(result.value());
}

auto CryptoKeyRSA::algorithm() const -> KeyAlgorithm
{
    auto modulusLength = getRSAModulusLength(m_platformKey);
    auto publicExponent = getRSAKeyParameter(m_platformKey, "e");

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
    switch (type()) {
    case CryptoKeyType::Public:
        return CryptoKeyRSAComponents::createPublic(getRSAKeyParameter(m_platformKey, "n"), getRSAKeyParameter(m_platformKey, "e"));
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

        CryptoKeyRSAComponents::PrimeInfo firstPrimeInfo;
        if (auto data = mpiData(pMPI))
            firstPrimeInfo.primeFactor = WTFMove(data.value());

        CryptoKeyRSAComponents::PrimeInfo secondPrimeInfo;
        if (auto data = mpiData(qMPI))
            secondPrimeInfo.primeFactor = WTFMove(data.value());

        // dp -- d mod (p - 1)
        {
            PAL::GCrypt::Handle<gcry_mpi_t> dpMPI(gcry_mpi_new(0));
            PAL::GCrypt::Handle<gcry_mpi_t> pm1MPI(gcry_mpi_new(0));
            gcry_mpi_sub_ui(pm1MPI, pMPI, 1);
            gcry_mpi_mod(dpMPI, dMPI, pm1MPI);

            if (auto data = mpiData(dpMPI))
                firstPrimeInfo.factorCRTExponent = WTFMove(data.value());
        }

        // dq -- d mod (q - 1)
        {
            PAL::GCrypt::Handle<gcry_mpi_t> dqMPI(gcry_mpi_new(0));
            PAL::GCrypt::Handle<gcry_mpi_t> qm1MPI(gcry_mpi_new(0));
            gcry_mpi_sub_ui(qm1MPI, qMPI, 1);
            gcry_mpi_mod(dqMPI, dMPI, qm1MPI);

            if (auto data = mpiData(dqMPI))
                secondPrimeInfo.factorCRTExponent = WTFMove(data.value());
        }

        // qi -- q^(-1) mod p
        {
            PAL::GCrypt::Handle<gcry_mpi_t> qiMPI(gcry_mpi_new(0));
            gcry_mpi_invm(qiMPI, qMPI, pMPI);

            if (auto data = mpiData(qiMPI))
                secondPrimeInfo.factorCRTCoefficient = WTFMove(data.value());
        }

        Vector<uint8_t> privateExponent;
        if (auto data = mpiData(dMPI))
            privateExponent = WTFMove(data.value());

        return CryptoKeyRSAComponents::createPrivateWithAdditionalData(
            getRSAKeyParameter(m_platformKey, "n"), getRSAKeyParameter(m_platformKey, "e"), WTFMove(privateExponent),
            WTFMove(firstPrimeInfo), WTFMove(secondPrimeInfo), Vector<CryptoKeyRSAComponents::PrimeInfo> { });
    }
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO)
