/*
 * Copyright (C) 2016 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <memory>
#include <openssl/X509.h>

namespace WebCore {

template <typename T>
struct OpenSSLCryptoPtrDeleter {
    void operator()(T* ptr) const = delete;
};

template <typename T>
using OpenSSLCryptoPtr = std::unique_ptr<T, OpenSSLCryptoPtrDeleter<T>>;

template <>
struct OpenSSLCryptoPtrDeleter<EVP_CIPHER_CTX> {
    void operator()(EVP_CIPHER_CTX* ptr) const
    {
        EVP_CIPHER_CTX_free(ptr);
    }
};

using EvpCipherCtxPtr = OpenSSLCryptoPtr<EVP_CIPHER_CTX>;

template <>
struct OpenSSLCryptoPtrDeleter<EVP_MD_CTX> {
    void operator()(EVP_MD_CTX* ptr) const
    {
        EVP_MD_CTX_free(ptr);
    }
};

using EvpDigestCtxPtr = OpenSSLCryptoPtr<EVP_MD_CTX>;

template <>
struct OpenSSLCryptoPtrDeleter<EVP_PKEY> {
    void operator()(EVP_PKEY* ptr) const
    {
        EVP_PKEY_free(ptr);
    }
};

using EvpPKeyPtr = OpenSSLCryptoPtr<EVP_PKEY>;

template <>
struct OpenSSLCryptoPtrDeleter<EVP_PKEY_CTX> {
    void operator()(EVP_PKEY_CTX* ptr) const
    {
        EVP_PKEY_CTX_free(ptr);
    }
};

using EvpPKeyCtxPtr = OpenSSLCryptoPtr<EVP_PKEY_CTX>;

template <>
struct OpenSSLCryptoPtrDeleter<RSA> {
    void operator()(RSA* ptr) const
    {
        RSA_free(ptr);
    }
};

using RSAPtr = OpenSSLCryptoPtr<RSA>;

template <>
struct OpenSSLCryptoPtrDeleter<EC_KEY> {
    void operator()(EC_KEY* ptr) const
    {
        EC_KEY_free(ptr);
    }
};

using ECKeyPtr = OpenSSLCryptoPtr<EC_KEY>;

template <>
struct OpenSSLCryptoPtrDeleter<EC_POINT> {
    void operator()(EC_POINT* ptr) const
    {
        EC_POINT_clear_free(ptr);
    }
};

using ECPointPtr = OpenSSLCryptoPtr<EC_POINT>;


template <>
struct OpenSSLCryptoPtrDeleter<PKCS8_PRIV_KEY_INFO> {
    void operator()(PKCS8_PRIV_KEY_INFO* ptr) const
    {
        PKCS8_PRIV_KEY_INFO_free(ptr);
    }
};

using PKCS8PrivKeyInfoPtr = OpenSSLCryptoPtr<PKCS8_PRIV_KEY_INFO>;

template <>
struct OpenSSLCryptoPtrDeleter<BIGNUM> {
    void operator()(BIGNUM* ptr) const
    {
        BN_clear_free(ptr);
    }
};

using BIGNUMPtr = OpenSSLCryptoPtr<BIGNUM>;

template <>
struct OpenSSLCryptoPtrDeleter<BN_CTX> {
    void operator()(BN_CTX* ptr) const
    {
        BN_CTX_free(ptr);
    }
};

using BNCtxPtr = OpenSSLCryptoPtr<BN_CTX>;

template <>
struct OpenSSLCryptoPtrDeleter<ECDSA_SIG> {
    void operator()(ECDSA_SIG* ptr) const
    {
        ECDSA_SIG_free(ptr);
    }
};

using ECDSASigPtr = OpenSSLCryptoPtr<ECDSA_SIG>;

template <>
struct OpenSSLCryptoPtrDeleter<ASN1_SEQUENCE_ANY> {
    void operator()(ASN1_SEQUENCE_ANY* ptr) const
    {
        sk_ASN1_TYPE_pop_free(ptr, ASN1_TYPE_free);
    }
};

using ASN1SequencePtr = OpenSSLCryptoPtr<ASN1_SEQUENCE_ANY>;

} // namespace WebCore
