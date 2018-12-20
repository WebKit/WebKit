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

#include "CryptoKeyPair.h"
#include "GCryptUtilities.h"
#include "JsonWebKey.h"
#include <pal/crypto/gcrypt/Handle.h>
#include <pal/crypto/gcrypt/Utilities.h>
#include <pal/crypto/tasn1/Utilities.h>
#include <wtf/text/Base64.h>

namespace WebCore {

static const char* curveName(CryptoKeyEC::NamedCurve curve)
{
    switch (curve) {
    case CryptoKeyEC::NamedCurve::P256:
        return "NIST P-256";
    case CryptoKeyEC::NamedCurve::P384:
        return "NIST P-384";
    case CryptoKeyEC::NamedCurve::P521:
        return "NIST P-521";
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

static const uint8_t* curveIdentifier(CryptoKeyEC::NamedCurve curve)
{
    switch (curve) {
    case CryptoKeyEC::NamedCurve::P256:
        return CryptoConstants::s_secp256r1Identifier.data();
    case CryptoKeyEC::NamedCurve::P384:
        return CryptoConstants::s_secp384r1Identifier.data();
    case CryptoKeyEC::NamedCurve::P521:
        return CryptoConstants::s_secp521r1Identifier.data();
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

static size_t curveSize(CryptoKeyEC::NamedCurve curve)
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

static unsigned curveUncompressedFieldElementSize(CryptoKeyEC::NamedCurve curve)
{
    switch (curve) {
    case CryptoKeyEC::NamedCurve::P256:
        return 32;
    case CryptoKeyEC::NamedCurve::P384:
        return 48;
    case CryptoKeyEC::NamedCurve::P521:
        return 66;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

static unsigned curveUncompressedPointSize(CryptoKeyEC::NamedCurve curve)
{
    return 2 * curveUncompressedFieldElementSize(curve) + 1;
}

CryptoKeyEC::~CryptoKeyEC()
{
    if (m_platformKey)
        PAL::GCrypt::HandleDeleter<gcry_sexp_t>()(m_platformKey);
}

size_t CryptoKeyEC::keySizeInBits() const
{
    size_t size = curveSize(m_curve);
    ASSERT(size == gcry_pk_get_nbits(m_platformKey));
    return size;
}

bool CryptoKeyEC::platformSupportedCurve(NamedCurve curve)
{
    return curve == NamedCurve::P256 || curve == NamedCurve::P384 || curve == NamedCurve::P521;
}

Optional<CryptoKeyPair> CryptoKeyEC::platformGeneratePair(CryptoAlgorithmIdentifier identifier, NamedCurve curve, bool extractable, CryptoKeyUsageBitmap usages)
{
    PAL::GCrypt::Handle<gcry_sexp_t> genkeySexp;
    gcry_error_t error = gcry_sexp_build(&genkeySexp, nullptr, "(genkey(ecc(curve %s)))", curveName(curve));
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return WTF::nullopt;
    }

    PAL::GCrypt::Handle<gcry_sexp_t> keyPairSexp;
    error = gcry_pk_genkey(&keyPairSexp, genkeySexp);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return WTF::nullopt;
    }

    PAL::GCrypt::Handle<gcry_sexp_t> publicKeySexp(gcry_sexp_find_token(keyPairSexp, "public-key", 0));
    PAL::GCrypt::Handle<gcry_sexp_t> privateKeySexp(gcry_sexp_find_token(keyPairSexp, "private-key", 0));
    if (!publicKeySexp || !privateKeySexp)
        return WTF::nullopt;

    auto publicKey = CryptoKeyEC::create(identifier, curve, CryptoKeyType::Public, publicKeySexp.release(), true, usages);
    auto privateKey = CryptoKeyEC::create(identifier, curve, CryptoKeyType::Private, privateKeySexp.release(), extractable, usages);
    return CryptoKeyPair { WTFMove(publicKey), WTFMove(privateKey) };
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportRaw(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    if (keyData.size() != curveUncompressedPointSize(curve))
        return nullptr;

    PAL::GCrypt::Handle<gcry_sexp_t> platformKey;
    gcry_error_t error = gcry_sexp_build(&platformKey, nullptr, "(public-key(ecc(curve %s)(q %b)))",
        curveName(curve), keyData.size(), keyData.data());
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return nullptr;
    }

    return create(identifier, curve, CryptoKeyType::Public, platformKey.release(), extractable, usages);
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportJWKPublic(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& x, Vector<uint8_t>&& y, bool extractable, CryptoKeyUsageBitmap usages)
{
    unsigned uncompressedFieldElementSize = curveUncompressedFieldElementSize(curve);
    if (x.size() != uncompressedFieldElementSize || y.size() != uncompressedFieldElementSize)
        return nullptr;

    // Construct the Vector that represents the EC point in uncompressed format.
    Vector<uint8_t> q;
    q.reserveInitialCapacity(curveUncompressedPointSize(curve));
    q.append(CryptoConstants::s_ecUncompressedFormatLeadingByte.data(), CryptoConstants::s_ecUncompressedFormatLeadingByte.size());
    q.appendVector(x);
    q.appendVector(y);

    PAL::GCrypt::Handle<gcry_sexp_t> platformKey;
    gcry_error_t error = gcry_sexp_build(&platformKey, nullptr, "(public-key(ecc(curve %s)(q %b)))",
        curveName(curve), q.size(), q.data());
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return nullptr;
    }

    return create(identifier, curve, CryptoKeyType::Public, platformKey.release(), extractable, usages);
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportJWKPrivate(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& x, Vector<uint8_t>&& y, Vector<uint8_t>&& d, bool extractable, CryptoKeyUsageBitmap usages)
{
    unsigned uncompressedFieldElementSize = curveUncompressedFieldElementSize(curve);
    if (x.size() != uncompressedFieldElementSize || y.size() != uncompressedFieldElementSize || d.size() != uncompressedFieldElementSize)
        return nullptr;

    // Construct the Vector that represents the EC point in uncompressed format.
    Vector<uint8_t> q;
    q.reserveInitialCapacity(curveUncompressedPointSize(curve));
    q.append(CryptoConstants::s_ecUncompressedFormatLeadingByte.data(), CryptoConstants::s_ecUncompressedFormatLeadingByte.size());
    q.appendVector(x);
    q.appendVector(y);

    PAL::GCrypt::Handle<gcry_sexp_t> platformKey;
    gcry_error_t error = gcry_sexp_build(&platformKey, nullptr, "(private-key(ecc(curve %s)(q %b)(d %b)))",
        curveName(curve), q.size(), q.data(), d.size(), d.data());
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return nullptr;
    }

    return create(identifier, curve, CryptoKeyType::Private, platformKey.release(), extractable, usages);
}

static bool supportedAlgorithmIdentifier(CryptoAlgorithmIdentifier keyIdentifier, const Vector<uint8_t>& identifier)
{
    auto* data = identifier.data();
    auto size = identifier.size();

    switch (keyIdentifier) {
    case CryptoAlgorithmIdentifier::ECDSA:
        // ECDSA only supports id-ecPublicKey algorithms for imported keys.
        if (CryptoConstants::matches(data, size, CryptoConstants::s_ecPublicKeyIdentifier))
            return true;
        return false;
    case CryptoAlgorithmIdentifier::ECDH:
        // ECDH supports both id-ecPublicKey and ic-ecDH algorithms for imported keys.
        if (CryptoConstants::matches(data, size, CryptoConstants::s_ecPublicKeyIdentifier))
            return true;
        if (CryptoConstants::matches(data, size, CryptoConstants::s_ecDHIdentifier))
            return true;
        return false;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    return false;
}

static Optional<CryptoKeyEC::NamedCurve> curveForIdentifier(const Vector<uint8_t>& identifier)
{
    auto* data = identifier.data();
    auto size = identifier.size();

    if (CryptoConstants::matches(data, size, CryptoConstants::s_secp256r1Identifier))
        return CryptoKeyEC::NamedCurve::P256;
    if (CryptoConstants::matches(data, size, CryptoConstants::s_secp384r1Identifier))
        return CryptoKeyEC::NamedCurve::P384;
    if (CryptoConstants::matches(data, size, CryptoConstants::s_secp521r1Identifier))
        return CryptoKeyEC::NamedCurve::P521;

    return WTF::nullopt;
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportSpki(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
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

        if (!supportedAlgorithmIdentifier(identifier, *algorithm))
            return nullptr;
    }

    // Validate `algorithm.parameters` and therein embedded `ECParameters`.
    {
        auto parameters = PAL::TASN1::elementData(spki, "algorithm.parameters");
        if (!parameters)
            return nullptr;

        // Decode the `ECParameters` structure using the `algorithm.parameters` data.
        PAL::TASN1::Structure ecParameters;
        if (!PAL::TASN1::decodeStructure(&ecParameters, "WebCrypto.ECParameters", *parameters))
            return nullptr;

        auto namedCurve = PAL::TASN1::elementData(ecParameters, "namedCurve");
        if (!namedCurve)
            return nullptr;

        auto parameterCurve = curveForIdentifier(*namedCurve);
        if (!parameterCurve || *parameterCurve != curve)
            return nullptr;
    }

    // Retrieve the `subjectPublicKey` data and embed it into the `public-key` s-expression.
    PAL::GCrypt::Handle<gcry_sexp_t> platformKey;
    {
        auto subjectPublicKey = PAL::TASN1::elementData(spki, "subjectPublicKey");
        if (!subjectPublicKey)
            return nullptr;

        // Bail if the `subjectPublicKey` data size doesn't match the size of an uncompressed point
        // for this curve, or if the first byte in the `subjectPublicKey` data isn't 0x04, as required
        // for an uncompressed EC point encoded in an octet string.
        if (subjectPublicKey->size() != curveUncompressedPointSize(curve)
            || !CryptoConstants::matches(subjectPublicKey->data(), 1, CryptoConstants::s_ecUncompressedFormatLeadingByte))
            return nullptr;

        // Convert X and Y coordinate data into MPIs.
        unsigned coordinateSize = curveUncompressedFieldElementSize(curve);
        PAL::GCrypt::Handle<gcry_mpi_t> xMPI, yMPI;
        {
            gcry_error_t error = gcry_mpi_scan(&xMPI, GCRYMPI_FMT_USG, &subjectPublicKey->at(1), coordinateSize, nullptr);
            if (error != GPG_ERR_NO_ERROR) {
                PAL::GCrypt::logError(error);
                return nullptr;
            }

            error = gcry_mpi_scan(&yMPI, GCRYMPI_FMT_USG, &subjectPublicKey->at(1 + coordinateSize), coordinateSize, nullptr);
            if (error != GPG_ERR_NO_ERROR) {
                PAL::GCrypt::logError(error);
                return nullptr;
            }
        }

        // Construct an MPI point from the X and Y coordinates and using 1 as the Z coordinate.
        // This always allocates the gcry_mpi_point_t object.
        PAL::GCrypt::Handle<gcry_mpi_point_t> point(gcry_mpi_point_set(nullptr, xMPI, yMPI, GCRYMPI_CONST_ONE));

        // Create an EC context for the specified curve.
        PAL::GCrypt::Handle<gcry_ctx_t> context;
        gcry_error_t error = gcry_mpi_ec_new(&context, nullptr, curveName(curve));
        if (error != GPG_ERR_NO_ERROR) {
            PAL::GCrypt::logError(error);
            return nullptr;
        }

        // Bail if the constructed MPI point is not on the specified EC curve.
        if (!gcry_mpi_ec_curve_point(point, context))
            return nullptr;

        error = gcry_sexp_build(&platformKey, nullptr, "(public-key(ecc(curve %s)(q %b)))",
            curveName(curve), subjectPublicKey->size(), subjectPublicKey->data());
        if (error != GPG_ERR_NO_ERROR) {
            PAL::GCrypt::logError(error);
            return nullptr;
        }
    }

    // Finally create a new CryptoKeyEC object, transferring to it ownership of the `public-key` s-expression.
    return create(identifier, curve, CryptoKeyType::Public, platformKey.release(), extractable, usages);
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportPkcs8(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
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

        if (!supportedAlgorithmIdentifier(identifier, *algorithm))
            return nullptr;
    }

    // Validate `privateKeyAlgorithm.parameters` and therein embedded `ECParameters`.
    {
        auto parameters = PAL::TASN1::elementData(pkcs8, "privateKeyAlgorithm.parameters");
        if (!parameters)
            return nullptr;

        PAL::TASN1::Structure ecParameters;
        if (!PAL::TASN1::decodeStructure(&ecParameters, "WebCrypto.ECParameters", *parameters))
            return nullptr;

        auto namedCurve = PAL::TASN1::elementData(ecParameters, "namedCurve");
        if (!namedCurve)
            return nullptr;

        auto parameterCurve = curveForIdentifier(*namedCurve);
        if (!parameterCurve || *parameterCurve != curve)
            return nullptr;
    }

    // Decode the `ECPrivateKey` structure using the `privateKey` data.
    PAL::TASN1::Structure ecPrivateKey;
    {
        auto privateKey = PAL::TASN1::elementData(pkcs8, "privateKey");
        if (!privateKey)
            return nullptr;

        if (!PAL::TASN1::decodeStructure(&ecPrivateKey, "WebCrypto.ECPrivateKey", *privateKey))
            return nullptr;
    }

    // Validate `privateKey.version`.
    {
        auto version = PAL::TASN1::elementData(ecPrivateKey, "version");
        if (!version)
            return nullptr;

        if (!CryptoConstants::matches(version->data(), version->size(), CryptoConstants::s_asn1Version1))
            return nullptr;
    }

    // Validate `privateKey.parameters.namedCurve`, if any.
    {
        auto namedCurve = PAL::TASN1::elementData(ecPrivateKey, "parameters.namedCurve");
        if (namedCurve) {
            auto parameterCurve = curveForIdentifier(*namedCurve);
            if (!parameterCurve || *parameterCurve != curve)
                return nullptr;
        }
    }

    // Validate `privateKey.publicKey`, if any, and scan the data into an MPI.
    PAL::GCrypt::Handle<gcry_mpi_t> publicKeyMPI;
    {
        auto publicKey = PAL::TASN1::elementData(ecPrivateKey, "publicKey");
        if (publicKey) {
            if (publicKey->size() != curveUncompressedPointSize(curve)
                || !CryptoConstants::matches(publicKey->data(), 1, CryptoConstants::s_ecUncompressedFormatLeadingByte))
                return nullptr;

            gcry_error_t error = gcry_mpi_scan(&publicKeyMPI, GCRYMPI_FMT_USG, publicKey->data(), publicKey->size(), nullptr);
            if (error != GPG_ERR_NO_ERROR) {
                PAL::GCrypt::logError(error);
                return nullptr;
            }
        }
    }

    // Retrieve the `privateKey.privateKey` data and embed it into the `private-key` s-expression.
    PAL::GCrypt::Handle<gcry_sexp_t> platformKey;
    {
        auto privateKey = PAL::TASN1::elementData(ecPrivateKey, "privateKey");
        if (!privateKey)
            return nullptr;

        // Validate the size of `privateKey`, making sure it fits the byte-size of the specified EC curve.
        if (privateKey->size() != (curveSize(curve) + 7) / 8)
            return nullptr;

        // Construct the `private-key` expression that will also be used for the EC context.
        gcry_error_t error = gcry_sexp_build(&platformKey, nullptr, "(private-key(ecc(curve %s)(d %b)))",
            curveName(curve), privateKey->size(), privateKey->data());
        if (error != GPG_ERR_NO_ERROR) {
            PAL::GCrypt::logError(error);
            return nullptr;
        }

        // Create an EC context for the specified curve.
        PAL::GCrypt::Handle<gcry_ctx_t> context;
        error = gcry_mpi_ec_new(&context, platformKey, nullptr);
        if (error != GPG_ERR_NO_ERROR) {
            PAL::GCrypt::logError(error);
            return nullptr;
        }

        // Set the 'q' value on the EC context if public key data was provided through the import.
        if (publicKeyMPI) {
            error = gcry_mpi_ec_set_mpi("q", publicKeyMPI, context);
            if (error != GPG_ERR_NO_ERROR) {
                PAL::GCrypt::logError(error);
                return nullptr;
            }
        }

        // Retrieve the `q` point. If the public key was provided through the PKCS#8 import, that
        // key value will be retrieved as an gcry_mpi_point_t. Otherwise, the `q` point value will
        // be computed on-the-fly by libgcrypt for the specified elliptic curve.
        PAL::GCrypt::Handle<gcry_mpi_point_t> point(gcry_mpi_ec_get_point("q", context, 1));
        if (!point)
            return nullptr;

        // Bail if the retrieved `q` MPI point is not on the specified EC curve.
        if (!gcry_mpi_ec_curve_point(point, context))
            return nullptr;
    }

    return create(identifier, curve, CryptoKeyType::Private, platformKey.release(), extractable, usages);
}

Vector<uint8_t> CryptoKeyEC::platformExportRaw() const
{
    PAL::GCrypt::Handle<gcry_ctx_t> context;
    gcry_error_t error = gcry_mpi_ec_new(&context, m_platformKey, nullptr);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return { };
    }

    PAL::GCrypt::Handle<gcry_mpi_t> qMPI(gcry_mpi_ec_get_mpi("q", context, 0));
    if (!qMPI)
        return { };

    auto q = mpiData(qMPI);
    if (!q || q->size() != curveUncompressedPointSize(m_curve))
        return { };

    return WTFMove(q.value());
}

bool CryptoKeyEC::platformAddFieldElements(JsonWebKey& jwk) const
{
    PAL::GCrypt::Handle<gcry_ctx_t> context;
    gcry_error_t error = gcry_mpi_ec_new(&context, m_platformKey, nullptr);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return false;
    }

    unsigned uncompressedFieldElementSize = curveUncompressedFieldElementSize(m_curve);

    PAL::GCrypt::Handle<gcry_mpi_t> qMPI(gcry_mpi_ec_get_mpi("q", context, 0));
    if (qMPI) {
        auto q = mpiData(qMPI);
        if (q && q->size() == curveUncompressedPointSize(m_curve)) {
            Vector<uint8_t> a;
            a.append(q->data() + 1, uncompressedFieldElementSize);
            jwk.x = base64URLEncode(a);

            Vector<uint8_t> b;
            b.append(q->data() + 1 + uncompressedFieldElementSize, uncompressedFieldElementSize);
            jwk.y = base64URLEncode(b);
        }
    }

    if (type() == Type::Private) {
        PAL::GCrypt::Handle<gcry_mpi_t> dMPI(gcry_mpi_ec_get_mpi("d", context, 0));
        if (dMPI) {
            auto d = mpiData(dMPI);
            if (d && d->size() <= uncompressedFieldElementSize) {
                // Zero-pad the private key data up to the field element size, if necessary.
                if (d->size() < uncompressedFieldElementSize) {
                    Vector<uint8_t> paddedData(uncompressedFieldElementSize - d->size(), 0);
                    paddedData.appendVector(*d);
                    *d = WTFMove(paddedData);
                }

                jwk.d = base64URLEncode(*d);
            }
        }
    }

    return true;
}

Vector<uint8_t> CryptoKeyEC::platformExportSpki() const
{
    PAL::TASN1::Structure ecParameters;
    {
        // Create the `ECParameters` structure.
        if (!PAL::TASN1::createStructure("WebCrypto.ECParameters", &ecParameters))
            return { };

        // Select the `namedCurve` object identifier as the target `ECParameters` choice.
        if (!PAL::TASN1::writeElement(ecParameters, "", "namedCurve", 1))
            return { };

        // Write out the EC curve identifier under `namedCurve`.
        if (!PAL::TASN1::writeElement(ecParameters, "namedCurve", curveIdentifier(m_curve), 1))
            return { };
    }

    PAL::TASN1::Structure spki;
    {
        // Create the `SubjectPublicKeyInfo` structure.
        if (!PAL::TASN1::createStructure("WebCrypto.SubjectPublicKeyInfo", &spki))
            return { };

        // Write out the id-ecPublicKey identifier under `algorithm.algorithm`.
        // FIXME: Per specification this should write out id-ecDH when the ECDH algorithm
        // is specified for this CryptoKeyEC object, but not even the W3C tests expect that.
        if (!PAL::TASN1::writeElement(spki, "algorithm.algorithm", CryptoConstants::s_ecPublicKeyIdentifier.data(), 1))
            return { };

        // Write out the `ECParameters` data under `algorithm.parameters`.
        {
            auto data = PAL::TASN1::encodedData(ecParameters, "");
            if (!data || !PAL::TASN1::writeElement(spki, "algorithm.parameters", data->data(), data->size()))
                return { };
        }

        // Retrieve the `q` s-expression, which should contain the public key data.
        PAL::GCrypt::Handle<gcry_sexp_t> qSexp(gcry_sexp_find_token(m_platformKey, "q", 0));
        if (!qSexp)
            return { };

        // Retrieve the `q` data, which should be in the uncompressed point format.
        // Validate the data size and the first byte (which should be 0x04).
        auto qData = mpiData(qSexp);
        if (!qData || qData->size() != curveUncompressedPointSize(m_curve)
            || !CryptoConstants::matches(qData->data(), 1, CryptoConstants::s_ecUncompressedFormatLeadingByte))
            return { };

        // Write out the public key data under `subjectPublicKey`. Because this is a
        // bit string parameter, the data size has to be multiplied by 8.
        if (!PAL::TASN1::writeElement(spki, "subjectPublicKey", qData->data(), qData->size() * 8))
            return { };
    }

    // Retrieve the encoded `SubjectPublicKeyInfo` data and return it.
    auto result = PAL::TASN1::encodedData(spki, "");
    if (!result)
        return { };

    return WTFMove(result.value());
}

Vector<uint8_t> CryptoKeyEC::platformExportPkcs8() const
{
    PAL::TASN1::Structure ecParameters;
    {
        // Create the `ECParameters` structure.
        if (!PAL::TASN1::createStructure("WebCrypto.ECParameters", &ecParameters))
            return { };

        // Select the `namedCurve` object identifier as the target `ECParameters` choice.
        if (!PAL::TASN1::writeElement(ecParameters, "", "namedCurve", 1))
            return { };

        // Write out the EC curve identifier under `namedCurve`.
        if (!PAL::TASN1::writeElement(ecParameters, "namedCurve", curveIdentifier(m_curve), 1))
            return { };
    }

    PAL::TASN1::Structure ecPrivateKey;
    {
        // Create the `ECPrivateKey` structure.
        if (!PAL::TASN1::createStructure("WebCrypto.ECPrivateKey", &ecPrivateKey))
            return { };

        // Write out '1' under `version`.
        if (!PAL::TASN1::writeElement(ecPrivateKey, "version", "1", 0))
            return { };

        // Construct the EC context that we'll use to retrieve private and public key data.
        PAL::GCrypt::Handle<gcry_ctx_t> context;
        gcry_error_t error = gcry_mpi_ec_new(&context, m_platformKey, nullptr);
        if (error != GPG_ERR_NO_ERROR)
            return { };

        {
            // Retrieve the `d` MPI that holds the private key data.
            PAL::GCrypt::Handle<gcry_mpi_t> dMPI(gcry_mpi_ec_get_mpi("d", context, 0));
            if (!dMPI)
                return { };

            unsigned uncompressedFieldElementSize = curveUncompressedFieldElementSize(m_curve);

            // Retrieve the `d` MPI data.
            auto data = mpiData(dMPI);
            if (!data || data->size() > uncompressedFieldElementSize)
                return { };

            // Zero-pad the private key data up to the field element size, if necessary.
            if (data->size() < uncompressedFieldElementSize) {
                Vector<uint8_t> paddedData(uncompressedFieldElementSize - data->size(), 0);
                paddedData.appendVector(*data);
                *data = WTFMove(paddedData);
            }

            // Write out the data under `privateKey`.
            if (!PAL::TASN1::writeElement(ecPrivateKey, "privateKey", data->data(), data->size()))
                return { };
        }

        // Eliminate the optional `parameters` element.
        if (!PAL::TASN1::writeElement(ecPrivateKey, "parameters", nullptr, 0))
            return { };

        {
            // Retrieve the `q` MPI that holds the public key data.
            PAL::GCrypt::Handle<gcry_mpi_t> qMPI(gcry_mpi_ec_get_mpi("q", context, 0));
            if (!qMPI)
                return { };

            // Retrieve the MPI data and write it out under `publicKey`. Because this is a
            // bit string parameter, the data size has to be multiplied by 8.
            auto data = mpiData(qMPI);
            if (!data || !PAL::TASN1::writeElement(ecPrivateKey, "publicKey", data->data(), data->size() * 8))
                return { };
        }
    }

    PAL::TASN1::Structure pkcs8;
    {
        // Create the `PrivateKeyInfo` structure.
        if (!PAL::TASN1::createStructure("WebCrypto.PrivateKeyInfo", &pkcs8))
            return { };

        // Write out '0' under `version`.
        if (!PAL::TASN1::writeElement(pkcs8, "version", "0", 0))
            return { };

        // Write out the id-ecPublicKey identifier under `privateKeyAlgorithm.algorithm`.
        // FIXME: Per specification this should write out id-ecDH when the ECDH algorithm
        // is specified for this CryptoKeyEC object, but not even the W3C tests expect that.
        if (!PAL::TASN1::writeElement(pkcs8, "privateKeyAlgorithm.algorithm", CryptoConstants::s_ecPublicKeyIdentifier.data(), 1))
            return { };

        // Write out the `ECParameters` data under `privateKeyAlgorithm.parameters`.
        {
            auto data = PAL::TASN1::encodedData(ecParameters, "");
            if (!data || !PAL::TASN1::writeElement(pkcs8, "privateKeyAlgorithm.parameters", data->data(), data->size()))
                return { };
        }

        // Write out the `ECPrivateKey` data under `privateKey`.
        {
            auto data = PAL::TASN1::encodedData(ecPrivateKey, "");
            if (!data || !PAL::TASN1::writeElement(pkcs8, "privateKey", data->data(), data->size()))
                return { };
        }

        // Eliminate the optional `attributes` element.
        if (!PAL::TASN1::writeElement(pkcs8, "attributes", nullptr, 0))
            return { };
    }

    // Retrieve the encoded `PrivateKeyInfo` data and return it.
    auto result = PAL::TASN1::encodedData(pkcs8, "");
    if (!result)
        return { };

    return WTFMove(result.value());
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO)
