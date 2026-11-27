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
#include "CryptoAlgorithmECDSA.h"

#include "CryptoAlgorithmEcdsaParams.h"
#include "CryptoKeyEC.h"
#include "ExceptionOr.h"
#include "OpenSSLUtilities.h"
#include <openssl/ecdsa.h>

namespace WebCore {

static ECDSASigPtr signDigest(EVP_PKEY* key, std::span<const uint8_t> digest)
{
    // ECDSA_do_sign() is deprecated only in OpenSSL 3. Other libraries keep using it.
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    EvpPKeyCtxPtr ctx(EVP_PKEY_CTX_new(key, nullptr));
    if (!ctx)
        return nullptr;

    if (EVP_PKEY_sign_init(ctx.get()) <= 0)
        return nullptr;

    size_t derSignatureLen;
    if (EVP_PKEY_sign(ctx.get(), nullptr, &derSignatureLen, digest.data(), digest.size()) <= 0)
        return nullptr;

    Vector<uint8_t> derSignature(derSignatureLen);
    if (EVP_PKEY_sign(ctx.get(), derSignature.mutableSpan().data(), &derSignatureLen, digest.data(), digest.size()) <= 0)
        return nullptr;

    // EVP_PKEY_sign() produces a DER-encoded ECDSA-Sig-Value, but WebCrypto expects "r" and "s" concatenated.
    const uint8_t* ptr = derSignature.span().data();
    return ECDSASigPtr(d2i_ECDSA_SIG(nullptr, &ptr, derSignatureLen));
#else
    const EC_KEY* ecKey = EVP_PKEY_get0_EC_KEY(key);
    if (!ecKey)
        return nullptr;

    return ECDSASigPtr(ECDSA_do_sign(digest.data(), digest.size(), const_cast<EC_KEY*>(ecKey)));
#endif
}

static std::optional<bool> verifyDigest(EVP_PKEY* key, std::span<const uint8_t> digest, const ECDSA_SIG& sig)
{
    // ECDSA_do_verify() is deprecated only in OpenSSL 3. Other libraries keep using it.
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    int derSignatureLen = i2d_ECDSA_SIG(&sig, nullptr);
    if (derSignatureLen <= 0)
        return std::nullopt;

    Vector<uint8_t> derSignature(derSignatureLen);
    uint8_t* ptr = derSignature.mutableSpan().data();
    if (i2d_ECDSA_SIG(&sig, &ptr) != derSignatureLen)
        return std::nullopt;

    EvpPKeyCtxPtr ctx(EVP_PKEY_CTX_new(key, nullptr));
    if (!ctx)
        return std::nullopt;

    if (EVP_PKEY_verify_init(ctx.get()) <= 0)
        return std::nullopt;

    return EVP_PKEY_verify(ctx.get(), derSignature.span().data(), derSignature.size(), digest.data(), digest.size()) == 1;
#else
    const EC_KEY* ecKey = EVP_PKEY_get0_EC_KEY(key);
    if (!ecKey)
        return std::nullopt;

    return ECDSA_do_verify(digest.data(), digest.size(), &sig, const_cast<EC_KEY*>(ecKey)) == 1;
#endif
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmECDSA::platformSign(const CryptoAlgorithmEcdsaParams& parameters, const CryptoKeyEC& key, const Vector<uint8_t>& data)
{
    size_t keySizeInBytes = (key.keySizeInBits() + 7) / 8;

    const EVP_MD* md = digestAlgorithm(parameters.hashIdentifier);
    if (!md)
        return Exception { ExceptionCode::NotSupportedError };

    std::optional<Vector<uint8_t>> digest = calculateDigest(md, data);
    if (!digest)
        return Exception { ExceptionCode::OperationError };

    ECDSASigPtr sig = signDigest(key.platformKey().get(), digest->span());
    if (!sig)
        return Exception { ExceptionCode::OperationError };

    const BIGNUM* r;
    const BIGNUM* s;
    ECDSA_SIG_get0(sig.get(), &r, &s);

    // Concatenate r and s, expanding r and s to keySizeInBytes.
    Vector<uint8_t> signature = convertToBytesExpand(r, keySizeInBytes);
    signature.appendVector(convertToBytesExpand(s, keySizeInBytes));

    return signature;
}

ExceptionOr<bool> CryptoAlgorithmECDSA::platformVerify(const CryptoAlgorithmEcdsaParams& parameters, const CryptoKeyEC& key, const Vector<uint8_t>& signature, const Vector<uint8_t>& data)
{
    size_t keySizeInBytes = (key.keySizeInBits() + 7) / 8;

    // Bail if the signature size isn't double the key size (i.e. concatenated r and s components).
    if (signature.size() != keySizeInBytes * 2)
        return false;
    
    auto sig = ECDSASigPtr(ECDSA_SIG_new());
    auto r = BN_bin2bn(signature.span().data(), keySizeInBytes, nullptr);
    auto s = BN_bin2bn(signature.subspan(keySizeInBytes).data(), keySizeInBytes, nullptr);

    if (!ECDSA_SIG_set0(sig.get(), r, s))
        return Exception { ExceptionCode::OperationError };

    const EVP_MD* md = digestAlgorithm(parameters.hashIdentifier);
    if (!md)
        return Exception { ExceptionCode::NotSupportedError };

    std::optional<Vector<uint8_t>> digest = calculateDigest(md, data);
    if (!digest)
        return Exception { ExceptionCode::OperationError };

    std::optional<bool> result = verifyDigest(key.platformKey().get(), digest->span(), *sig);
    if (!result)
        return Exception { ExceptionCode::OperationError };

    return *result;
}

} // namespace WebCore
