/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "CryptoAlgorithmECDH.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CommonCryptoUtilities.h"
#include "CryptoKeyEC.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

void CryptoAlgorithmECDH::platformDeriveBits(Ref<CryptoKey>&& baseKey, Ref<CryptoKey>&& publicKey, unsigned long length, Callback&& callback, ScriptExecutionContext& context, WorkQueue& workQueue)
{
    context.ref();
    workQueue.dispatch([baseKey = WTFMove(baseKey), publicKey = WTFMove(publicKey), length, callback = WTFMove(callback), &context]() mutable {
        auto& ecBaseKey = downcast<CryptoKeyEC>(baseKey.get());
        auto& ecPublicKey = downcast<CryptoKeyEC>(publicKey.get());

        std::optional<Vector<uint8_t>> result = std::nullopt;
        Vector<uint8_t> derivedKey(ecBaseKey.keySizeInBits() / 8); // Per https://tools.ietf.org/html/rfc6090#section-4.
        size_t size = derivedKey.size();
        if (!CCECCryptorComputeSharedSecret(ecBaseKey.platformKey(), ecPublicKey.platformKey(), derivedKey.data(), &size))
            result = WTFMove(derivedKey);

        context.postTask([result = WTFMove(result), length, callback = WTFMove(callback)](ScriptExecutionContext& context) mutable {
            callback(WTFMove(result), length);
            context.deref();
        });
    });
}

}

#endif // ENABLE(SUBTLE_CRYPTO)
