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
#include "GCryptUtilities.h"

namespace WebCore {

Optional<const char*> hashAlgorithmName(CryptoAlgorithmIdentifier identifier)
{
    switch (identifier) {
    case CryptoAlgorithmIdentifier::SHA_1:
        return "sha1";
    case CryptoAlgorithmIdentifier::SHA_224:
        return "sha224";
    case CryptoAlgorithmIdentifier::SHA_256:
        return "sha256";
    case CryptoAlgorithmIdentifier::SHA_384:
        return "sha384";
    case CryptoAlgorithmIdentifier::SHA_512:
        return "sha512";
    default:
        return WTF::nullopt;
    }
}

Optional<int> hmacAlgorithm(CryptoAlgorithmIdentifier identifier)
{
    switch (identifier) {
    case CryptoAlgorithmIdentifier::SHA_1:
        return GCRY_MAC_HMAC_SHA1;
    case CryptoAlgorithmIdentifier::SHA_224:
        return GCRY_MAC_HMAC_SHA224;
    case CryptoAlgorithmIdentifier::SHA_256:
        return GCRY_MAC_HMAC_SHA256;
    case CryptoAlgorithmIdentifier::SHA_384:
        return GCRY_MAC_HMAC_SHA384;
    case CryptoAlgorithmIdentifier::SHA_512:
        return GCRY_MAC_HMAC_SHA512;
    default:
        return WTF::nullopt;
    }
}

Optional<int> digestAlgorithm(CryptoAlgorithmIdentifier identifier)
{
    switch (identifier) {
    case CryptoAlgorithmIdentifier::SHA_1:
        return GCRY_MD_SHA1;
    case CryptoAlgorithmIdentifier::SHA_224:
        return GCRY_MD_SHA224;
    case CryptoAlgorithmIdentifier::SHA_256:
        return GCRY_MD_SHA256;
    case CryptoAlgorithmIdentifier::SHA_384:
        return GCRY_MD_SHA384;
    case CryptoAlgorithmIdentifier::SHA_512:
        return GCRY_MD_SHA512;
    default:
        return WTF::nullopt;
    }
}

Optional<PAL::CryptoDigest::Algorithm> hashCryptoDigestAlgorithm(CryptoAlgorithmIdentifier identifier)
{
    switch (identifier) {
    case CryptoAlgorithmIdentifier::SHA_1:
        return PAL::CryptoDigest::Algorithm::SHA_1;
    case CryptoAlgorithmIdentifier::SHA_224:
        return PAL::CryptoDigest::Algorithm::SHA_224;
    case CryptoAlgorithmIdentifier::SHA_256:
        return PAL::CryptoDigest::Algorithm::SHA_256;
    case CryptoAlgorithmIdentifier::SHA_384:
        return PAL::CryptoDigest::Algorithm::SHA_384;
    case CryptoAlgorithmIdentifier::SHA_512:
        return PAL::CryptoDigest::Algorithm::SHA_512;
    default:
        return WTF::nullopt;
    }
}

Optional<size_t> mpiLength(gcry_mpi_t paramMPI)
{
    // Retrieve the MPI length for the unsigned format.
    size_t dataLength = 0;
    gcry_error_t error = gcry_mpi_print(GCRYMPI_FMT_USG, nullptr, 0, &dataLength, paramMPI);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return WTF::nullopt;
    }

    return dataLength;
}

Optional<size_t> mpiLength(gcry_sexp_t paramSexp)
{
    // Retrieve the MPI value stored in the s-expression: (name mpi-data)
    PAL::GCrypt::Handle<gcry_mpi_t> paramMPI(gcry_sexp_nth_mpi(paramSexp, 1, GCRYMPI_FMT_USG));
    if (!paramMPI)
        return WTF::nullopt;

    return mpiLength(paramMPI);
}

Optional<Vector<uint8_t>> mpiData(gcry_mpi_t paramMPI)
{
    // Retrieve the MPI length.
    auto length = mpiLength(paramMPI);
    if (!length)
        return WTF::nullopt;

    // Copy the MPI data into a properly-sized buffer.
    Vector<uint8_t> output(*length);
    gcry_error_t error = gcry_mpi_print(GCRYMPI_FMT_USG, output.data(), output.size(), nullptr, paramMPI);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return WTF::nullopt;
    }

    return output;
}

Optional<Vector<uint8_t>> mpiZeroPrefixedData(gcry_mpi_t paramMPI, size_t targetLength)
{
    // Retrieve the MPI length. Bail if the retrieved length is longer than target length.
    auto length = mpiLength(paramMPI);
    if (!length || *length > targetLength)
        return WTF::nullopt;

    // Fill out the output buffer with zeros. Properly determine the zero prefix length,
    // and copy the MPI data into memory area following the prefix (if any).
    Vector<uint8_t> output(targetLength, 0);
    size_t prefixLength = targetLength - *length;
    gcry_error_t error = gcry_mpi_print(GCRYMPI_FMT_USG, output.data() + prefixLength, targetLength, nullptr, paramMPI);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return WTF::nullopt;
    }

    return output;
}

Optional<Vector<uint8_t>> mpiData(gcry_sexp_t paramSexp)
{
    // Retrieve the MPI value stored in the s-expression: (name mpi-data)
    PAL::GCrypt::Handle<gcry_mpi_t> paramMPI(gcry_sexp_nth_mpi(paramSexp, 1, GCRYMPI_FMT_USG));
    if (!paramMPI)
        return WTF::nullopt;

    return mpiData(paramMPI);
}

Optional<Vector<uint8_t>> mpiZeroPrefixedData(gcry_sexp_t paramSexp, size_t targetLength)
{
    // Retrieve the MPI value stored in the s-expression: (name mpi-data)
    PAL::GCrypt::Handle<gcry_mpi_t> paramMPI(gcry_sexp_nth_mpi(paramSexp, 1, GCRYMPI_FMT_USG));
    if (!paramMPI)
        return WTF::nullopt;

    return mpiZeroPrefixedData(paramMPI, targetLength);
}

Optional<Vector<uint8_t>> mpiSignedData(gcry_mpi_t mpi)
{
    auto data = mpiData(mpi);
    if (!data)
        return WTF::nullopt;

    if (data->at(0) & 0x80)
        data->insert(0, 0x00);

    return data;
}

Optional<Vector<uint8_t>> mpiSignedData(gcry_sexp_t paramSexp)
{
    auto data = mpiData(paramSexp);
    if (!data)
        return WTF::nullopt;

    if (data->at(0) & 0x80)
        data->insert(0, 0x00);

    return data;
}

} // namespace WebCore
