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
#include "CryptoKeyRSA.h"

#if ENABLE(WEB_CRYPTO)

#include "CryptoKeyRSAComponents.h"
#include "NotImplemented.h"

namespace WebCore {

RefPtr<CryptoKeyRSA> CryptoKeyRSA::create(CryptoAlgorithmIdentifier, CryptoAlgorithmIdentifier hash, bool hasHash, const CryptoKeyRSAComponents&, bool extractable, CryptoKeyUsageBitmap)
{
    notImplemented();
    return nullptr;
}

bool CryptoKeyRSA::isRestrictedToHash(CryptoAlgorithmIdentifier&) const
{
    notImplemented();
    return true;
}

size_t CryptoKeyRSA::keySizeInBits() const
{
    notImplemented();
    return 0;
}

void CryptoKeyRSA::generatePair(CryptoAlgorithmIdentifier, CryptoAlgorithmIdentifier hash, bool hasHash, unsigned modulusLength, const Vector<uint8_t>& publicExponent, bool extractable, CryptoKeyUsageBitmap, KeyPairCallback&&, VoidCallback&& failureCallback, ScriptExecutionContext*)
{
    notImplemented();
}

RefPtr<CryptoKeyRSA> CryptoKeyRSA::importSpki(CryptoAlgorithmIdentifier, Optional<CryptoAlgorithmIdentifier> hash, Vector<uint8_t>&&, bool extractable, CryptoKeyUsageBitmap)
{
    notImplemented();
    return nullptr;
}

RefPtr<CryptoKeyRSA> CryptoKeyRSA::importPkcs8(CryptoAlgorithmIdentifier, Optional<CryptoAlgorithmIdentifier> hash, Vector<uint8_t>&&, bool extractable, CryptoKeyUsageBitmap)
{
    notImplemented();
    return nullptr;
}

ExceptionOr<Vector<uint8_t>> CryptoKeyRSA::exportSpki() const
{
    notImplemented();
    return Exception { NotSupportedError };
}

ExceptionOr<Vector<uint8_t>> CryptoKeyRSA::exportPkcs8() const
{
    notImplemented();
    return Exception { NotSupportedError };
}

std::unique_ptr<CryptoKeyRSAComponents> CryptoKeyRSA::exportData() const
{
    notImplemented();
    return nullptr;
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO)
