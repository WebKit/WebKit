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

#pragma once

#include "CryptoAlgorithmIdentifier.h"
#include <array>
#include <cstring>
#include <gcrypt.h>
#include <pal/crypto/CryptoDigest.h>
#include <pal/crypto/gcrypt/Handle.h>
#include <pal/crypto/gcrypt/Utilities.h>
#include <wtf/Optional.h>

namespace WebCore {

namespace CryptoConstants {

static const std::array<uint8_t, 18> s_ecPublicKeyIdentifier { { "1.2.840.10045.2.1" } };
static const std::array<uint8_t, 13> s_ecDHIdentifier { { "1.3.132.1.12" } };

static const std::array<uint8_t, 20> s_secp256r1Identifier { { "1.2.840.10045.3.1.7" } };
static const std::array<uint8_t, 13> s_secp384r1Identifier { { "1.3.132.0.34" } };
static const std::array<uint8_t, 13> s_secp521r1Identifier { { "1.3.132.0.35" } };

static const std::array<uint8_t, 21> s_rsaEncryptionIdentifier { { "1.2.840.113549.1.1.1" } };
static const std::array<uint8_t, 21> s_RSAES_OAEPIdentifier { { "1.2.840.113549.1.1.7" } };
static const std::array<uint8_t, 22> s_RSASSA_PSSIdentifier { { "1.2.840.113549.1.1.10" } };

static const std::array<uint8_t, 2> s_asn1NullValue { { 0x05, 0x00 } };
static const std::array<uint8_t, 1> s_asn1Version0 { { 0x00 } };
static const std::array<uint8_t, 1> s_asn1Version1 { { 0x01 } };

static const std::array<uint8_t, 1> s_ecUncompressedFormatLeadingByte { { 0x04 } };

template<size_t N>
static inline bool matches(const void* lhs, size_t size, const std::array<uint8_t, N>& rhs)
{
    if (size != rhs.size())
        return false;

    return !std::memcmp(lhs, rhs.data(), rhs.size());
}

} // namespace CryptoConstants

Optional<const char*> hashAlgorithmName(CryptoAlgorithmIdentifier);

Optional<int> hmacAlgorithm(CryptoAlgorithmIdentifier);
Optional<int> digestAlgorithm(CryptoAlgorithmIdentifier);
Optional<PAL::CryptoDigest::Algorithm> hashCryptoDigestAlgorithm(CryptoAlgorithmIdentifier);

Optional<size_t> mpiLength(gcry_mpi_t);
Optional<size_t> mpiLength(gcry_sexp_t);
Optional<Vector<uint8_t>> mpiData(gcry_mpi_t);
Optional<Vector<uint8_t>> mpiZeroPrefixedData(gcry_mpi_t, size_t targetLength);
Optional<Vector<uint8_t>> mpiData(gcry_sexp_t);
Optional<Vector<uint8_t>> mpiZeroPrefixedData(gcry_sexp_t, size_t targetLength);
Optional<Vector<uint8_t>> mpiSignedData(gcry_mpi_t);
Optional<Vector<uint8_t>> mpiSignedData(gcry_sexp_t);

} // namespace WebCore
