/*
 * Copyright (C) 2014 Igalia S.L.
 * Copyright (C) 2016 SoftAtHome
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

#include "CryptoAlgorithmHmacParamsDeprecated.h"
#include "CryptoKeyHMAC.h"
#include "ExceptionCode.h"
#include <gcrypt.h>
#include <wtf/CryptographicUtilities.h>

namespace WebCore {

static int getGCryptDigestAlgorithm(CryptoAlgorithmIdentifier hashFunction)
{
    switch (hashFunction) {
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
        return GCRY_MAC_NONE;
    }
}

static bool calculateSignature(int algorithm, const Vector<uint8_t>& key, const CryptoOperationData& data, Vector<uint8_t>& signature)
{
    size_t digestLength = gcry_mac_get_algo_maclen(algorithm);
    const void* keyData = key.data() ? key.data() : reinterpret_cast<const uint8_t*>("");

    bool result = false;
    gcry_mac_hd_t hd;
    gcry_error_t err;

    err = gcry_mac_open(&hd, algorithm, 0, nullptr);
    if (err)
        goto cleanup;

    err = gcry_mac_setkey(hd, keyData, key.size());
    if (err)
        goto cleanup;

    err = gcry_mac_write(hd, data.first, data.second);
    if (err)
        goto cleanup;

    signature.resize(digestLength);
    err = gcry_mac_read(hd, signature.data(), &digestLength);
    if (err)
        goto cleanup;

    signature.resize(digestLength);
    result = true;

cleanup:
    if (hd)
        gcry_mac_close(hd);

    return result;
}

void CryptoAlgorithmHMAC::platformSign(const CryptoAlgorithmHmacParamsDeprecated& parameters, const CryptoKeyHMAC& key, const CryptoOperationData& data, VectorCallback&& callback, VoidCallback&& failureCallback, ExceptionCode& ec)
{
    UNUSED_PARAM(failureCallback);
    int algorithm = getGCryptDigestAlgorithm(parameters.hash);
    if (algorithm == GCRY_MAC_NONE) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    Vector<uint8_t> signature;
    if (calculateSignature(algorithm, key.key(), data, signature))
        callback(signature);
    else
        failureCallback();
}

void CryptoAlgorithmHMAC::platformVerify(const CryptoAlgorithmHmacParamsDeprecated& parameters, const CryptoKeyHMAC& key, const CryptoOperationData& expectedSignature, const CryptoOperationData& data, BoolCallback&& callback, VoidCallback&& failureCallback, ExceptionCode& ec)
{
    UNUSED_PARAM(failureCallback);
    int algorithm = getGCryptDigestAlgorithm(parameters.hash);
    if (algorithm == GCRY_MAC_NONE) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    Vector<uint8_t> signature;
    if (!calculateSignature(algorithm, key.key(), data, signature)) {
        failureCallback();
        return;
    }

    // Using a constant time comparison to prevent timing attacks.
    bool result = signature.size() == expectedSignature.second && !constantTimeMemcmp(signature.data(), expectedSignature.first, signature.size());

    callback(result);
}

}

#endif // ENABLE(SUBTLE_CRYPTO)
