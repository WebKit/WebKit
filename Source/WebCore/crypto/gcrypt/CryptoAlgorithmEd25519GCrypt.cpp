/*
 * Copyright (C) 2023 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2,1 of the License, or (at your option) any later version.
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

#if ENABLE(WEB_CRYPTO)

#include "CryptoKeyOKP.h"
#include "GCryptUtilities.h"

namespace WebCore {

static bool extractEDDSASignatureInteger(Vector<uint8_t>& signature, gcry_sexp_t signatureSexp, const char* integerName, size_t keySizeInBytes)
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
        signature.append(&integerData->at(dataSize - keySizeInBytes), keySizeInBytes);
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
            data.size(), data.data());
        if (error != GPG_ERR_NO_ERROR) {
            PAL::GCrypt::logError(error);
            return Exception { ExceptionCode::OperationError };
        }
    }

    // Construct the `private-key` expression that will also be used for the EC context.
    PAL::GCrypt::Handle<gcry_sexp_t> keySexp;
    gcry_error_t error = gcry_sexp_build(&keySexp, nullptr, "(private-key(ecc(curve Ed25519)(flags eddsa)(d %b)))",
        sk.size(), sk.data());
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

    if (!extractEDDSASignatureInteger(signature, signatureSexp, "r", keySizeInBytes)
        || !extractEDDSASignatureInteger(signature, signatureSexp, "s", keySizeInBytes))
        return Exception { ExceptionCode::OperationError };

    return signature;
}

static ExceptionOr<bool> verifyEd25519(const Vector<uint8_t>& key, size_t keyLengthInBytes, const Vector<uint8_t>& signature, const Vector<uint8_t>& data)
{
    if (signature.size() != keyLengthInBytes * 2)
        return false;

    // Construct the sig-val s-expression, extracting the r and s components from the signature vector.
    PAL::GCrypt::Handle<gcry_sexp_t> signatureSexp;
    gcry_error_t error = gcry_sexp_build(&signatureSexp, nullptr, "(sig-val(eddsa(r %b)(s %b)))",
        keyLengthInBytes, signature.data(), keyLengthInBytes, signature.data() + keyLengthInBytes);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return false;
    }

    // Construct the data s-expression that contains raw hashed data.
    PAL::GCrypt::Handle<gcry_sexp_t> dataSexp;
    {
        gcry_error_t error = gcry_sexp_build(&dataSexp, nullptr, "(data(flags eddsa)(hash-algo sha512) (value %b))",
            data.size(), data.data());
        if (error != GPG_ERR_NO_ERROR) {
            PAL::GCrypt::logError(error);
            return false;
        }
    }

    // Construct the `public-key` expression to be used for generating the MPI structure.
    PAL::GCrypt::Handle<gcry_sexp_t> keySexp;
    error = gcry_sexp_build(&keySexp, nullptr, "(public-key(ecc(curve Ed25519)(q %b)))",
        key.size(), key.data());
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

}
#endif
