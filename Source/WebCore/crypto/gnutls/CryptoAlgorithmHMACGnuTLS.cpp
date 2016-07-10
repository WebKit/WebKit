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
#include "CryptoAlgorithmHMAC.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoAlgorithmHmacParams.h"
#include "CryptoKeyHMAC.h"
#include "ExceptionCode.h"
#include <gnutls/gnutls.h>
#include <gnutls/crypto.h>
#include <wtf/CryptographicUtilities.h>

namespace WebCore {

static gnutls_mac_algorithm_t getGnutlsDigestAlgorithm(CryptoAlgorithmIdentifier hashFunction)
{
    switch (hashFunction) {
    case CryptoAlgorithmIdentifier::SHA_1:
        return GNUTLS_MAC_SHA1;
    case CryptoAlgorithmIdentifier::SHA_224:
        return GNUTLS_MAC_SHA224;
    case CryptoAlgorithmIdentifier::SHA_256:
        return GNUTLS_MAC_SHA256;
    case CryptoAlgorithmIdentifier::SHA_384:
        return GNUTLS_MAC_SHA384;
    case CryptoAlgorithmIdentifier::SHA_512:
        return GNUTLS_MAC_SHA512;
    default:
        return GNUTLS_MAC_UNKNOWN;
    }
}

static Vector<uint8_t> calculateSignature(gnutls_mac_algorithm_t algorithm, const Vector<uint8_t>& key, const CryptoOperationData& data)
{
    size_t digestLength = gnutls_hmac_get_len(algorithm);

    Vector<uint8_t> result(digestLength);
    const void* keyData = key.data() ? key.data() : reinterpret_cast<const uint8_t*>("");
    int ret = gnutls_hmac_fast(algorithm, keyData, key.size(), data.first, data.second, result.data());
    ASSERT(ret == GNUTLS_E_SUCCESS);
    UNUSED_PARAM(ret);

    return result;
}

void CryptoAlgorithmHMAC::platformSign(const CryptoAlgorithmHmacParams& parameters, const CryptoKeyHMAC& key, const CryptoOperationData& data, VectorCallback&& callback, VoidCallback&& failureCallback, ExceptionCode& ec)
{
    UNUSED_PARAM(failureCallback);
    gnutls_mac_algorithm_t algorithm = getGnutlsDigestAlgorithm(parameters.hash);
    if (algorithm == GNUTLS_MAC_UNKNOWN) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    Vector<uint8_t> signature = calculateSignature(algorithm, key.key(), data);

    callback(signature);
}

void CryptoAlgorithmHMAC::platformVerify(const CryptoAlgorithmHmacParams& parameters, const CryptoKeyHMAC& key, const CryptoOperationData& expectedSignature, const CryptoOperationData& data, BoolCallback&& callback, VoidCallback&& failureCallback, ExceptionCode& ec)
{
    UNUSED_PARAM(failureCallback);
    gnutls_mac_algorithm_t algorithm = getGnutlsDigestAlgorithm(parameters.hash);
    if (algorithm == GNUTLS_MAC_UNKNOWN) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    Vector<uint8_t> signature = calculateSignature(algorithm, key.key(), data);

    // Using a constant time comparison to prevent timing attacks.
    bool result = signature.size() == expectedSignature.second && !constantTimeMemcmp(signature.data(), expectedSignature.first, signature.size());

    callback(result);
}

}

#endif // ENABLE(SUBTLE_CRYPTO)
