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
#include "CryptoAlgorithmPBKDF2.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoAlgorithmPbkdf2Params.h"
#include "CryptoKeyRaw.h"
#include "ExceptionCode.h"
#include "ScriptExecutionContext.h"
#include <CommonCrypto/CommonKeyDerivation.h>

namespace WebCore {

static CCPseudoRandomAlgorithm commonCryptoHMACAlgorithm(CryptoAlgorithmIdentifier hashFunction)
{
    switch (hashFunction) {
    case CryptoAlgorithmIdentifier::SHA_1:
        return kCCPRFHmacAlgSHA1;
    case CryptoAlgorithmIdentifier::SHA_224:
        return kCCPRFHmacAlgSHA224;
    case CryptoAlgorithmIdentifier::SHA_256:
        return kCCPRFHmacAlgSHA256;
    case CryptoAlgorithmIdentifier::SHA_384:
        return kCCPRFHmacAlgSHA384;
    case CryptoAlgorithmIdentifier::SHA_512:
        return kCCPRFHmacAlgSHA512;
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

void CryptoAlgorithmPBKDF2::platformDeriveBits(std::unique_ptr<CryptoAlgorithmParameters>&& parameters, Ref<CryptoKey>&& baseKey, size_t length, VectorCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext& context, WorkQueue& workQueue)
{
    context.ref();
    workQueue.dispatch([parameters = WTFMove(parameters), baseKey = WTFMove(baseKey), length, callback = WTFMove(callback), exceptionCallback = WTFMove(exceptionCallback), &context]() mutable {
        auto& pbkdf2Parameters = downcast<CryptoAlgorithmPbkdf2Params>(*parameters);
        auto& rawKey = downcast<CryptoKeyRaw>(baseKey.get());

        Vector<uint8_t> result(length / 8);
        // <rdar://problem/32439955> Currently, CCKeyDerivationPBKDF bails out when an empty password/salt is provided.
        if (CCKeyDerivationPBKDF(kCCPBKDF2, reinterpret_cast<const char *>(rawKey.key().data()), rawKey.key().size(), pbkdf2Parameters.saltVector().data(), pbkdf2Parameters.saltVector().size(), commonCryptoHMACAlgorithm(pbkdf2Parameters.hashIdentifier), pbkdf2Parameters.iterations, result.data(), length / 8)) {
            // We should only dereference callbacks after being back to the Document/Worker threads.
            context.postTask([exceptionCallback = WTFMove(exceptionCallback), callback = WTFMove(callback)](ScriptExecutionContext& context) {
                exceptionCallback(OperationError);
                context.deref();
            });
            return;
        }
        // We should only dereference callbacks after being back to the Document/Worker threads.
        context.postTask([callback = WTFMove(callback), result = WTFMove(result), exceptionCallback = WTFMove(exceptionCallback)](ScriptExecutionContext& context) {
            callback(result);
            context.deref();
        });
    });
}

}

#endif // ENABLE(SUBTLE_CRYPTO)
