/*
 * Copyright (C) 2023 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "CryptoAlgorithmEd25519.h"

#include "CryptoKeyOKP.h"
#include "ExceptionOr.h"
#include "GCryptUtilities.h"

namespace WebCore {

static bool extractEDDSASignatureInteger(Vector<uint8_t>& signature, gcry_sexp_t signatureSexp, ASCIILiteral integerName, size_t keySizeInBytes)
{
    // Retrieve byte data of the specified integer.
    PAL::GCrypt::Handle<gcry_sexp_t> integerSexp(gcry_sexp_find_token(signatureSexp, integerName, 0));
    if (!integerSexp)
        return false;

    auto integerData = mpiData(integerSexp);
    if (!integerData)
        return false;

    size_t dataSize = integerData->size();
    if (dataSize >= keySizeInBytes) {
        // Append the last `keySizeInBytes` bytes of the data Vector, if available.
        signature.append(integerData->subspan(dataSize - keySizeInBytes, keySizeInBytes));
    } else {
        // If not, prefix the binary data with zero bytes.
        for (size_t paddingSize = keySizeInBytes - dataSize; paddingSize > 0; --paddingSize)
            signature.append(0x00);
        signature.appendVector(*integerData);
    }

    return true;
}

static ExceptionOr<Vector<uint8_t>> signEd25519(const Vector<uint8_t>& sk, size_t keySizeInBytes, const Vector<uint8_t>& data)
{
    ASSERT(keySizeInBytes == 32);

    // Construct the data s-expression that contains raw hashed data.
    PAL::GCrypt::Handle<gcry_sexp_t> dataSexp;
    {
        gcry_error_t error = gcry_sexp_build(&dataSexp, nullptr, "(data(flags eddsa)(hash-algo sha512) (value %b))",
            data.size(), data.span().data());
        if (error != GPG_ERR_NO_ERROR) {
            PAL::GCrypt::logError(error);
            return Exception { ExceptionCode::OperationError };
        }
    }

    // Construct the `private-key` expression that will also be used for the EC context.
    PAL::GCrypt::Handle<gcry_sexp_t> keySexp;
    gcry_error_t error = gcry_sexp_build(&keySexp, nullptr, "(private-key(ecc(curve Ed25519)(flags eddsa)(d %b)))",
        sk.size(), sk.span().data());
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return Exception { ExceptionCode::OperationError };
    }

    // Perform the PK signing, retrieving a sig-val s-expression of the following form:
    // (sig-val
    //   (dsa
    //     (r r-mpi)
    //     (s s-mpi)))
    PAL::GCrypt::Handle<gcry_sexp_t> signatureSexp;
    error = gcry_pk_sign(&signatureSexp, dataSexp, keySexp);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return Exception { ExceptionCode::OperationError };
    }

    // Retrieve MPI data of the resulting r and s integers. They are concatenated into
    // a single buffer, properly accounting for integers that don't match the key in size.
    Vector<uint8_t> signature;
    signature.reserveInitialCapacity(64);

    if (!extractEDDSASignatureInteger(signature, signatureSexp, "r"_s, keySizeInBytes)
        || !extractEDDSASignatureInteger(signature, signatureSexp, "s"_s, keySizeInBytes))
        return Exception { ExceptionCode::OperationError };

    return signature;
}

static bool isSmallOrderPoint(std::span<const uint8_t> point)
{
    // Encodings of the points of order 1, 2, 4 and 8, ignoring the sign bit,
    // including the non-canonical encodings of y = 0 and y = 1.
    static constexpr std::array<std::array<uint8_t, 32>, 7> smallOrderPoints { {
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 0x26, 0xe8, 0x95, 0x8f, 0xc2, 0xb2, 0x27, 0xb0, 0x45, 0xc3, 0xf4, 0x89, 0xf2, 0xef, 0x98, 0xf0, 0xd5, 0xdf, 0xac, 0x05, 0xd3, 0xc6, 0x33, 0x39, 0xb1, 0x38, 0x02, 0x88, 0x6d, 0x53, 0xfc, 0x05 },
        { 0xc7, 0x17, 0x6a, 0x70, 0x3d, 0x4d, 0xd8, 0x4f, 0xba, 0x3c, 0x0b, 0x76, 0x0d, 0x10, 0x67, 0x0f, 0x2a, 0x20, 0x53, 0xfa, 0x2c, 0x39, 0xcc, 0xc6, 0x4e, 0xc7, 0xfd, 0x77, 0x92, 0xac, 0x03, 0x7a },
        { 0xec, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f },
        { 0xed, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f },
        { 0xee, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f },
    } };

    if (point.size() != 32)
        return false;

    return std::ranges::any_of(smallOrderPoints, [&](auto& smallOrderPoint) {
        // The encoding is y in little-endian, with the top bit of the last byte holding the sign of x.
        // Ignore that bit, so that each entry matches both points that share the same y.
        return equalSpans(point.first(31), std::span { smallOrderPoint }.first(31)) && (point[31] & 0x7f) == smallOrderPoint[31];
    });
}

static ExceptionOr<bool> verifyEd25519(const Vector<uint8_t>& key, size_t keyLengthInBytes, const Vector<uint8_t>& signature, const Vector<uint8_t>& data)
{
    if (signature.size() != keyLengthInBytes * 2)
        return false;

    // Bail if the public key or the signature's R value is a small-order point,
    // which the spec requires to fail verification.
    if (isSmallOrderPoint(key.span()) || isSmallOrderPoint(signature.span().first(keyLengthInBytes)))
        return false;

    // Construct the sig-val s-expression, extracting the r and s components from the signature vector.
    PAL::GCrypt::Handle<gcry_sexp_t> signatureSexp;
    gcry_error_t error = gcry_sexp_build(&signatureSexp, nullptr, "(sig-val(eddsa(r %b)(s %b)))",
        keyLengthInBytes, signature.span().data(), keyLengthInBytes, signature.subspan(keyLengthInBytes).data());
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return false;
    }

    // Construct the data s-expression that contains raw hashed data.
    PAL::GCrypt::Handle<gcry_sexp_t> dataSexp;
    {
        gcry_error_t error = gcry_sexp_build(&dataSexp, nullptr, "(data(flags eddsa)(hash-algo sha512) (value %b))",
            data.size(), data.span().data());
        if (error != GPG_ERR_NO_ERROR) {
            PAL::GCrypt::logError(error);
            return false;
        }
    }

    // Construct the `public-key` expression to be used for generating the MPI structure.
    PAL::GCrypt::Handle<gcry_sexp_t> keySexp;
    error = gcry_sexp_build(&keySexp, nullptr, "(public-key(ecc(curve Ed25519)(q %b)))",
        key.size(), key.span().data());
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return false;
    }

    // Perform the PK verification. We report success if there's no error returned, or
    // a failure in any other case. OperationError should not be returned at this point,
    // avoiding spilling information about the exact cause of verification failure.
    error = gcry_pk_verify(signatureSexp, dataSexp, keySexp);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return false;
    }
    return { error == GPG_ERR_NO_ERROR };
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmEd25519::platformSign(const CryptoKeyOKP& key, const Vector<uint8_t>& data)
{
    return signEd25519(key.platformKey(), key.keySizeInBytes(), data);
}

ExceptionOr<bool> CryptoAlgorithmEd25519::platformVerify(const CryptoKeyOKP& key, const Vector<uint8_t>& signature, const Vector<uint8_t>& data)
{
    return verifyEd25519(key.platformKey(), key.keySizeInBytes(), signature, data);
}

} // namespace WebCore
