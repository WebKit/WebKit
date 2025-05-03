/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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
#include "CryptoAlgorithmHKDF.h"

#include "CryptoAlgorithmHkdfParams.h"
#include "CryptoKeyRaw.h"
#include "OpenSSLUtilities.h"


#if defined(OPENSSL_VERSION_MAJOR) && OPENSSL_VERSION_MAJOR > 1
#include <openssl/core_names.h>
#include <openssl/kdf.h>
#else
#include <openssl/hkdf.h>
#endif

namespace WebCore {

#if defined(OPENSSL_VERSION_MAJOR) && OPENSSL_VERSION_MAJOR > 1
static std::optional<const char*> hashAlgorithmName(CryptoAlgorithmIdentifier identifier)
{
    switch (identifier) {
    case CryptoAlgorithmIdentifier::SHA_1:
        return SN_sha1;
    case CryptoAlgorithmIdentifier::SHA_224:
        return SN_sha224;
    case CryptoAlgorithmIdentifier::SHA_256:
        return SN_sha256;
    case CryptoAlgorithmIdentifier::SHA_384:
        return SN_sha384;
    case CryptoAlgorithmIdentifier::SHA_512:
        return SN_sha512;
    default:
        return std::nullopt;
    }
}
#endif

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmHKDF::platformDeriveBits(const CryptoAlgorithmHkdfParams& parameters, const CryptoKeyRaw& key, size_t length)
{
#if defined(OPENSSL_VERSION_MAJOR) && OPENSSL_VERSION_MAJOR > 1
    auto algorithm = hashAlgorithmName(parameters.hashIdentifier);
    if (!algorithm)
        return Exception { ExceptionCode::NotSupportedError };

    auto kdf = EVPKDFPtr(EVP_KDF_fetch(nullptr, "HKDF", nullptr));
    auto kctx = EVPKDFCtxPtr(EVP_KDF_CTX_new(kdf.get()));

    OSSL_PARAM params[5], *p = params;
    *p++ = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST, const_cast<char*>(*algorithm), strlen(*algorithm));
    *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY, reinterpret_cast<void*>(const_cast<unsigned char*>(key.key().data())), key.key().size());
    *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, reinterpret_cast<void*>(const_cast<unsigned char*>(parameters.saltVector().data())), parameters.saltVector().size());
    *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO, reinterpret_cast<void*>(const_cast<unsigned char*>(parameters.infoVector().data())), parameters.infoVector().size());
    *p = OSSL_PARAM_construct_end();

    Vector<uint8_t> output(length / 8);
    if (EVP_KDF_derive(kctx.get(), output.data(), output.size(), params) <= 0)
        return Exception { ExceptionCode::OperationError };

    return output;
#else
    auto algorithm = digestAlgorithm(parameters.hashIdentifier);
    if (!algorithm)
        return Exception { ExceptionCode::NotSupportedError };

    Vector<uint8_t> output(length / 8);
    if (HKDF(output.data(), output.size(), algorithm, key.key().data(), key.key().size(), parameters.saltVector().data(), parameters.saltVector().size(), parameters.infoVector().data(), parameters.infoVector().size()) <= 0)
        return Exception { ExceptionCode::OperationError };

    return output;
#endif
}

} // namespace WebCore
