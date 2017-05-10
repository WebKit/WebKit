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

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoKeyPair.h"
#include "JsonWebKey.h"
#include "NotImplemented.h"
#include <pal/crypto/gcrypt/Handle.h>
#include <pal/crypto/gcrypt/Utilities.h>
#include <wtf/text/Base64.h>

namespace WebCore {

static size_t curveSize(CryptoKeyEC::NamedCurve curve)
{
    switch (curve) {
    case CryptoKeyEC::NamedCurve::P256:
        return 256;
    case CryptoKeyEC::NamedCurve::P384:
        return 384;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

static const char* curveName(CryptoKeyEC::NamedCurve curve)
{
    switch (curve) {
    case CryptoKeyEC::NamedCurve::P256:
        return "NIST P-256";
    case CryptoKeyEC::NamedCurve::P384:
        return "NIST P-384";
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

static unsigned uncompressedPointSizeForCurve(CryptoKeyEC::NamedCurve curve)
{
    switch (curve) {
    case CryptoKeyEC::NamedCurve::P256:
        return 65;
    case CryptoKeyEC::NamedCurve::P384:
        return 97;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

static unsigned uncompressedFieldElementSizeForCurve(CryptoKeyEC::NamedCurve curve)
{
    switch (curve) {
    case CryptoKeyEC::NamedCurve::P256:
        return 32;
    case CryptoKeyEC::NamedCurve::P384:
        return 48;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

static Vector<uint8_t> extractMPIData(gcry_mpi_t mpi)
{
    size_t dataLength = 0;
    gcry_error_t error = gcry_mpi_print(GCRYMPI_FMT_USG, nullptr, 0, &dataLength, mpi);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return { };
    }

    Vector<uint8_t> data(dataLength);
    error = gcry_mpi_print(GCRYMPI_FMT_USG, data.data(), data.size(), nullptr, mpi);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return { };
    }

    return data;
};

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

std::optional<CryptoKeyPair> CryptoKeyEC::platformGeneratePair(CryptoAlgorithmIdentifier identifier, NamedCurve curve, bool extractable, CryptoKeyUsageBitmap usages)
{
    PAL::GCrypt::Handle<gcry_sexp_t> genkeySexp;
    gcry_error_t error = gcry_sexp_build(&genkeySexp, nullptr, "(genkey(ecc(curve %s)))", curveName(curve));
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return std::nullopt;
    }

    PAL::GCrypt::Handle<gcry_sexp_t> keyPairSexp;
    error = gcry_pk_genkey(&keyPairSexp, genkeySexp);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return std::nullopt;
    }

    PAL::GCrypt::Handle<gcry_sexp_t> publicKeySexp(gcry_sexp_find_token(keyPairSexp, "public-key", 0));
    PAL::GCrypt::Handle<gcry_sexp_t> privateKeySexp(gcry_sexp_find_token(keyPairSexp, "private-key", 0));
    if (!publicKeySexp || !privateKeySexp)
        return std::nullopt;

    auto publicKey = CryptoKeyEC::create(identifier, curve, CryptoKeyType::Public, publicKeySexp.release(), true, usages);
    auto privateKey = CryptoKeyEC::create(identifier, curve, CryptoKeyType::Private, privateKeySexp.release(), extractable, usages);
    return CryptoKeyPair { WTFMove(publicKey), WTFMove(privateKey) };
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportRaw(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    if (keyData.size() != uncompressedPointSizeForCurve(curve))
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
    unsigned uncompressedFieldElementSize = uncompressedFieldElementSizeForCurve(curve);
    if (x.size() != uncompressedFieldElementSize || y.size() != uncompressedFieldElementSize)
        return nullptr;

    // Construct the Vector that represents the EC point in uncompressed format.
    Vector<uint8_t> q;
    q.reserveInitialCapacity(1 + 2 * uncompressedFieldElementSize);
    q.append(0x04);
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
    unsigned uncompressedFieldElementSize = uncompressedFieldElementSizeForCurve(curve);
    if (x.size() != uncompressedFieldElementSize || y.size() != uncompressedFieldElementSize || d.size() != uncompressedFieldElementSize)
        return nullptr;

    // Construct the Vector that represents the EC point in uncompressed format.
    Vector<uint8_t> q;
    q.reserveInitialCapacity(1 + 2 * uncompressedFieldElementSize);
    q.append(0x04);
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

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportSpki(CryptoAlgorithmIdentifier, NamedCurve, Vector<uint8_t>&&, bool, CryptoKeyUsageBitmap)
{
    notImplemented();

    return nullptr;
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportPkcs8(CryptoAlgorithmIdentifier, NamedCurve, Vector<uint8_t>&&, bool, CryptoKeyUsageBitmap)
{
    notImplemented();

    return nullptr;
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

    Vector<uint8_t> q = extractMPIData(qMPI);
    if (q.size() != uncompressedPointSizeForCurve(m_curve))
        return { };

    return q;
}

void CryptoKeyEC::platformAddFieldElements(JsonWebKey& jwk) const
{
    PAL::GCrypt::Handle<gcry_ctx_t> context;
    gcry_error_t error = gcry_mpi_ec_new(&context, m_platformKey, nullptr);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return;
    }

    unsigned uncompressedFieldElementSize = uncompressedFieldElementSizeForCurve(m_curve);

    PAL::GCrypt::Handle<gcry_mpi_t> qMPI(gcry_mpi_ec_get_mpi("q", context, 0));
    if (qMPI) {
        Vector<uint8_t> q = extractMPIData(qMPI);
        if (q.size() == uncompressedPointSizeForCurve(m_curve)) {
            Vector<uint8_t> a;
            a.append(q.data() + 1, uncompressedFieldElementSize);
            jwk.x = base64URLEncode(a);

            Vector<uint8_t> b;
            b.append(q.data() + 1 + uncompressedFieldElementSize, uncompressedFieldElementSize);
            jwk.y = base64URLEncode(b);
        }
    }

    if (type() == Type::Private) {
        PAL::GCrypt::Handle<gcry_mpi_t> dMPI(gcry_mpi_ec_get_mpi("d", context, 0));
        if (dMPI) {
            Vector<uint8_t> d = extractMPIData(dMPI);
            if (d.size() == uncompressedFieldElementSize)
                jwk.d = base64URLEncode(d);
        }
    }
}

Vector<uint8_t> CryptoKeyEC::platformExportSpki() const
{
    notImplemented();

    return { };
}

Vector<uint8_t> CryptoKeyEC::platformExportPkcs8() const
{
    notImplemented();

    return { };
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)
