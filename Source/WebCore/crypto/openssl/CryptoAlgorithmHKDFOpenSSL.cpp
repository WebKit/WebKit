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
#include "ExceptionOr.h"
#include "OpenSSLUtilities.h"

#if __has_include(<openssl/hkdf.h>)
#include <openssl/hkdf.h>
#else
#include <openssl/evp.h>
#include <openssl/kdf.h>

// This function is available in hkdf.h but that only exists for LibreSSL and BoringSSL, not
// plain OpenSSL. Therefore we provide our own implementation here.
int HKDF(unsigned char* output, size_t outSize, const evp_md_st* algorithm,
    const unsigned char* inKey, size_t inKeySize,
    const unsigned char* inSalt, size_t inSaltSize,
    const unsigned char* inInfo, size_t inInfoSize)
{
    EVP_PKEY_CTX* kctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);

    EVP_PKEY_CTX_set_hkdf_md(kctx, EVP_sha256());
    EVP_PKEY_CTX_set1_hkdf_salt(kctx, inSalt, inSaltSize);
    EVP_PKEY_CTX_set1_hkdf_key(kctx, inKey, inKeySize);
    EVP_PKEY_CTX_add1_hkdf_info(kctx, inInfo, inInfoSize);

    int ret = EVP_PKEY_derive(kctx, output, &outSize);

    EVP_PKEY_CTX_free(kctx);

    return ret;
}
#endif

namespace WebCore {

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmHKDF::platformDeriveBits(const CryptoAlgorithmHkdfParams& parameters, const CryptoKeyRaw& key, size_t length)
{
    auto algorithm = digestAlgorithm(parameters.hashIdentifier);
    if (!algorithm)
        return Exception { ExceptionCode::NotSupportedError };

    Vector<uint8_t> output(length / 8);
    if (HKDF(output.mutableSpan().data(), output.size(), algorithm, key.key().span().data(), key.key().size(), parameters.saltVector().span().data(), parameters.saltVector().size(), parameters.infoVector().span().data(), parameters.infoVector().size()) <= 0)
        return Exception { ExceptionCode::OperationError };

    return output;
}

} // namespace WebCore
