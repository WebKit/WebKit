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
#include <openssl/evp.h>

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

} // namespace WebCore
