/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Metrological Group B.V.
 * Copyright (C) 2017 Igalia S.L.
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
#include "CryptoAlgorithmECDH.h"

#if ENABLE(WEB_CRYPTO)

#include "CryptoKeyEC.h"
#include "GCryptUtilities.h"
#include <pal/crypto/gcrypt/Handle.h>
#include <pal/crypto/gcrypt/Utilities.h>

namespace WebCore {

static std::optional<Vector<uint8_t>> gcryptDerive(gcry_sexp_t baseKeySexp, gcry_sexp_t publicKeySexp, size_t keySizeInBytes)
{
    // First, retrieve private key data, which is roughly of the following form:
    // (private-key
    //   (ecc
    //     ...
    //     (d ...)))
    PAL::GCrypt::Handle<gcry_sexp_t> dataSexp;
    {
        PAL::GCrypt::Handle<gcry_sexp_t> dSexp(gcry_sexp_find_token(baseKeySexp, "d", 0));
        if (!dSexp)
            return std::nullopt;

        auto data = mpiData(dSexp);
        if (!data)
            return std::nullopt;

        gcry_sexp_build(&dataSexp, nullptr, "(data(flags raw)(value %b))", data->size(), data->data());
        if (!dataSexp)
            return std::nullopt;
    }

    // Encrypt the data s-expression with the public key.
    PAL::GCrypt::Handle<gcry_sexp_t> cipherSexp;
    gcry_error_t error = gcry_pk_encrypt(&cipherSexp, dataSexp, publicKeySexp);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return std::nullopt;
    }

    // Retrieve the shared point value from the generated s-expression, which is of the following form:
    // (enc-val
    //   (ecdh
    //     (s ...)
    //     (e ...)))
    PAL::GCrypt::Handle<gcry_mpi_t> xMPI(gcry_mpi_new(0));
    if (!xMPI)
        return std::nullopt;

    {
        PAL::GCrypt::Handle<gcry_sexp_t> sSexp(gcry_sexp_find_token(cipherSexp, "s", 0));
        if (!sSexp)
            return std::nullopt;

        PAL::GCrypt::Handle<gcry_mpi_t> sMPI(gcry_sexp_nth_mpi(sSexp, 1, GCRYMPI_FMT_USG));
        if (!sMPI)
            return std::nullopt;

        PAL::GCrypt::Handle<gcry_mpi_point_t> point(gcry_mpi_point_new(0));
        if (!point)
            return std::nullopt;

        error = gcry_mpi_ec_decode_point(point, sMPI, nullptr);
        if (error != GPG_ERR_NO_ERROR)
            return std::nullopt;

        // We're only interested in the x-coordinate.
        gcry_mpi_point_snatch_get(xMPI, nullptr, nullptr, point.release());
    }

    return mpiZeroPrefixedData(xMPI, keySizeInBytes);
}

std::optional<Vector<uint8_t>> CryptoAlgorithmECDH::platformDeriveBits(const CryptoKeyEC& baseKey, const CryptoKeyEC& publicKey)
{
    return gcryptDerive(baseKey.platformKey(), publicKey.platformKey(), (baseKey.keySizeInBits() + 7) / 8);
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO)
