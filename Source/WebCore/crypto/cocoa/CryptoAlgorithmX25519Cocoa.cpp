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
#include <pal/spi/cocoa/CoreCryptoSPI.h>

namespace WebCore {

std::optional<Vector<uint8_t>> CryptoAlgorithmX25519::platformDeriveBits(const CryptoKeyOKP& baseKey, const CryptoKeyOKP& publicKey)
{
    if (baseKey.platformKey().size() != 32 || publicKey.platformKey().size() != 32)
        return std::nullopt;

    ccec25519pubkey derivedKey;
    static_assert(sizeof(derivedKey) == 32);
#if HAVE(CORE_CRYPTO_SIGNATURES_INT_RETURN_VALUE)
    if (cccurve25519(derivedKey, baseKey.platformKey().data(), publicKey.platformKey().data()))
        return std::nullopt;
#else
    cccurve25519(derivedKey, baseKey.platformKey().data(), publicKey.platformKey().data());
#endif
    return Vector<uint8_t>(std::span { derivedKey, 32 });
}

} // namespace WebCore
