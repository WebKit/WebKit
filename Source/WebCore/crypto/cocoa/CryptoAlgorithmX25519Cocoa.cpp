/*
 * Copyright (C) 2023 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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
#include <pal/PALSwift.h>
#include <pal/spi/cocoa/CoreCryptoSPI.h>

namespace WebCore {

static std::optional<Vector<uint8_t>> deriveBitsCryptoKit(const Vector<uint8_t>& baseKey, const Vector<uint8_t>& publicKey)
{
#if !defined(CLANG_WEBKIT_BRANCH)
    if (baseKey.size() != ed25519KeySize || publicKey.size() != ed25519KeySize)
        return std::nullopt;
    auto rv = PAL::EdKey::deriveBits(PAL::EdKeyAgreementAlgorithm::x25519(), baseKey.span(), publicKey.span());
    if (rv.errorCode != Cpp::ErrorCodes::Success)
        return std::nullopt;
    return WTFMove(rv.result);
#else
    UNUSED_PARAM(baseKey);
    UNUSED_PARAM(publicKey);
    RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("CLANG_WEBKIT_BRANCH");
#endif
}

std::optional<Vector<uint8_t>> CryptoAlgorithmX25519::platformDeriveBits(const CryptoKeyOKP& baseKey, const CryptoKeyOKP& publicKey)
{
    return deriveBitsCryptoKit(baseKey.platformKey(), publicKey.platformKey());
}
} // namespace WebCore
