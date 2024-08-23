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
#include "CryptoAlgorithmX25519.h"

#include "CryptoKeyOKP.h"
#if HAVE(SWIFT_CPP_INTEROP)
#include <pal/PALSwift.h>
#endif
#include <pal/spi/cocoa/CoreCryptoSPI.h>

namespace WebCore {

#if HAVE(SWIFT_CPP_INTEROP)
static std::optional<Vector<uint8_t>> deriveBitsCryptoKit(const Vector<uint8_t>& baseKey, const Vector<uint8_t>& publicKey)
{
    if (baseKey.size() != ed25519KeySize || publicKey.size() != ed25519KeySize)
        return std::nullopt;
    auto rv = PAL::EdKey::deriveBits(PAL::EdKeyAgreementAlgorithm::x25519(), baseKey.span(), publicKey.span());
    if (rv.errorCode != Cpp::ErrorCodes::Success)
        return std::nullopt;
    return WTFMove(rv.result);
}
#else
static std::optional<Vector<uint8_t>> deriveBitsCoreCrypto(const Vector<uint8_t>& baseKey, const Vector<uint8_t>& publicKey)
{
    if (baseKey.size() != ed25519KeySize || publicKey.size() != ed25519KeySize)
        return std::nullopt;

    ccec25519pubkey derivedKey;
    static_assert(sizeof(derivedKey) == ed25519KeySize);
#if HAVE(CORE_CRYPTO_SIGNATURES_INT_RETURN_VALUE)
    if (cccurve25519(derivedKey, baseKey.data(), publicKey.data()))
        return std::nullopt;
#else
    cccurve25519(derivedKey, baseKey.data(), publicKey.data());
#endif
    return Vector<uint8_t>(std::span { derivedKey, ed25519KeySize });
}
#endif
std::optional<Vector<uint8_t>> CryptoAlgorithmX25519::platformDeriveBits(const CryptoKeyOKP& baseKey, const CryptoKeyOKP& publicKey)
{
#if HAVE(SWIFT_CPP_INTEROP)
    return deriveBitsCryptoKit(baseKey.platformKey(), publicKey.platformKey());
#else
    return deriveBitsCoreCrypto(baseKey.platformKey(), publicKey.platformKey());
#endif
}
} // namespace WebCore
