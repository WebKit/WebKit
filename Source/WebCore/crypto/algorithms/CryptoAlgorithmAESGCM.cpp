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
#include "CryptoAlgorithmAESGCM.h"

#include "CryptoAlgorithmAesGcmParams.h"
#include "CryptoAlgorithmAesKeyParams.h"
#include "CryptoKeyAES.h"
#include "ScriptExecutionContext.h"
#include <algorithm>
#include <ranges>
#include <wtf/CrossThreadCopier.h>

namespace WebCore {

namespace CryptoAlgorithmAESGCMInternal {
static constexpr auto ALG128 = "A128GCM"_s;
static constexpr auto ALG192 = "A192GCM"_s;
static constexpr auto ALG256 = "A256GCM"_s;
#if CPU(ADDRESS64)
static const uint64_t PlainTextMaxLength = 549755813632ULL; // 2^39 - 256
#endif
static constexpr std::array<uint8_t, 7> validTagLengths { 32, 64, 96, 104, 112, 120, 128 };
}

static inline bool usagesAreInvalidForCryptoAlgorithmAESGCM(CryptoKeyUsageBitmap usages)
{
    return usages & (CryptoKeyUsageSign | CryptoKeyUsageVerify | CryptoKeyUsageDeriveKey | CryptoKeyUsageDeriveBits);
}

static inline bool tagLengthIsValid(uint8_t tagLength)
{
    using namespace CryptoAlgorithmAESGCMInternal;
    return std::ranges::find(validTagLengths, tagLength) != std::ranges::end(validTagLengths);
}

Ref<CryptoAlgorithm> CryptoAlgorithmAESGCM::create()
{
    return adoptRef(*new CryptoAlgorithmAESGCM);
}

CryptoAlgorithmIdentifier CryptoAlgorithmAESGCM::identifier() const
{
    return s_identifier;
}

void CryptoAlgorithmAESGCM::encrypt(const CryptoAlgorithmParameters& parameters, Ref<CryptoKey>&& key, Vector<uint8_t>&& plainText, VectorCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext& context, WorkQueue& workQueue)
{
    using namespace CryptoAlgorithmAESGCMInternal;

    auto& aesParameters = downcast<CryptoAlgorithmAesGcmParams>(parameters);

#if CPU(ADDRESS64)
    if (plainText.size() > PlainTextMaxLength) {
        exceptionCallback(ExceptionCode::OperationError);
        return;
    }
    if (aesParameters.ivVector().size() > UINT64_MAX) {
        exceptionCallback(ExceptionCode::OperationError);
        return;
    }
    if (aesParameters.additionalDataVector().size() > UINT64_MAX) {
        exceptionCallback(ExceptionCode::OperationError);
        return;
    }
#endif

    aesParameters.tagLength = aesParameters.tagLength ? aesParameters.tagLength : CryptoAlgorithmAESGCM::DefaultTagLength;
    if (!tagLengthIsValid(*(aesParameters.tagLength))) {
        exceptionCallback(ExceptionCode::OperationError);
        return;
    }

    dispatchOperationInWorkQueue(workQueue, context, WTFMove(callback), WTFMove(exceptionCallback),
        [parameters = crossThreadCopy(aesParameters), key = WTFMove(key), plainText = WTFMove(plainText)] {
            return platformEncrypt(parameters, downcast<CryptoKeyAES>(key.get()), plainText);
    });
}

void CryptoAlgorithmAESGCM::decrypt(const CryptoAlgorithmParameters& parameters, Ref<CryptoKey>&& key, Vector<uint8_t>&& cipherText, VectorCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext& context, WorkQueue& workQueue)
{
    using namespace CryptoAlgorithmAESGCMInternal;

    auto& aesParameters = downcast<CryptoAlgorithmAesGcmParams>(parameters);

    aesParameters.tagLength = aesParameters.tagLength ? aesParameters.tagLength : CryptoAlgorithmAESGCM::DefaultTagLength;
    if (!tagLengthIsValid(*(aesParameters.tagLength))) {
        exceptionCallback(ExceptionCode::OperationError);
        return;
    }
    if (cipherText.size() < *(aesParameters.tagLength) / 8) {
        exceptionCallback(ExceptionCode::OperationError);
        return;
    }

#if CPU(ADDRESS64)
    if (aesParameters.ivVector().size() > UINT64_MAX) {
        exceptionCallback(ExceptionCode::OperationError);
        return;
    }
    if (aesParameters.additionalDataVector().size() > UINT64_MAX) {
        exceptionCallback(ExceptionCode::OperationError);
        return;
    }
#endif

    dispatchOperationInWorkQueue(workQueue, context, WTFMove(callback), WTFMove(exceptionCallback),
        [parameters = crossThreadCopy(aesParameters), key = WTFMove(key), cipherText = WTFMove(cipherText)] {
            return platformDecrypt(parameters, downcast<CryptoKeyAES>(key.get()), cipherText);
        });
}

void CryptoAlgorithmAESGCM::generateKey(const CryptoAlgorithmParameters& parameters, bool extractable, CryptoKeyUsageBitmap usages, KeyOrKeyPairCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext&)
{
    const auto& aesParameters = downcast<CryptoAlgorithmAesKeyParams>(parameters);

    if (usagesAreInvalidForCryptoAlgorithmAESGCM(usages)) {
        exceptionCallback(ExceptionCode::SyntaxError);
        return;
    }

    auto result = CryptoKeyAES::generate(CryptoAlgorithmIdentifier::AES_GCM, aesParameters.length, extractable, usages);
    if (!result) {
        exceptionCallback(ExceptionCode::OperationError);
        return;
    }

    callback(WTFMove(result));
}

void CryptoAlgorithmAESGCM::importKey(CryptoKeyFormat format, KeyData&& data, const CryptoAlgorithmParameters& parameters, bool extractable, CryptoKeyUsageBitmap usages, KeyCallback&& callback, ExceptionCallback&& exceptionCallback)
{
    using namespace CryptoAlgorithmAESGCMInternal;

    if (usagesAreInvalidForCryptoAlgorithmAESGCM(usages)) {
        exceptionCallback(ExceptionCode::SyntaxError);
        return;
    }

    RefPtr<CryptoKeyAES> result;
    switch (format) {
    case CryptoKeyFormat::Raw:
        result = CryptoKeyAES::importRaw(parameters.identifier, WTFMove(std::get<Vector<uint8_t>>(data)), extractable, usages);
        break;
    case CryptoKeyFormat::Jwk: {
        auto checkAlgCallback = [](size_t length, const String& alg) -> bool {
            switch (length) {
            case CryptoKeyAES::s_length128:
                return alg.isNull() || alg == ALG128;
            case CryptoKeyAES::s_length192:
                return alg.isNull() || alg == ALG192;
            case CryptoKeyAES::s_length256:
                return alg.isNull() || alg == ALG256;
            }
            return false;
        };
        result = CryptoKeyAES::importJwk(parameters.identifier, WTFMove(std::get<JsonWebKey>(data)), extractable, usages, WTFMove(checkAlgCallback));
        break;
    }
    default:
        exceptionCallback(ExceptionCode::NotSupportedError);
        return;
    }
    if (!result) {
        exceptionCallback(ExceptionCode::DataError);
        return;
    }

    callback(*result);
}

void CryptoAlgorithmAESGCM::exportKey(CryptoKeyFormat format, Ref<CryptoKey>&& key, KeyDataCallback&& callback, ExceptionCallback&& exceptionCallback)
{
    using namespace CryptoAlgorithmAESGCMInternal;
    const auto& aesKey = downcast<CryptoKeyAES>(key.get());

    if (aesKey.key().isEmpty()) {
        exceptionCallback(ExceptionCode::OperationError);
        return;
    }

    KeyData result;
    switch (format) {
    case CryptoKeyFormat::Raw:
        result = Vector<uint8_t>(aesKey.key());
        break;
    case CryptoKeyFormat::Jwk: {
        JsonWebKey jwk = aesKey.exportJwk();
        switch (aesKey.key().size() * 8) {
        case CryptoKeyAES::s_length128:
            jwk.alg = String(ALG128);
            break;
        case CryptoKeyAES::s_length192:
            jwk.alg = String(ALG192);
            break;
        case CryptoKeyAES::s_length256:
            jwk.alg = String(ALG256);
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        result = WTFMove(jwk);
        break;
    }
    default:
        exceptionCallback(ExceptionCode::NotSupportedError);
        return;
    }

    callback(format, WTFMove(result));
}

ExceptionOr<std::optional<size_t>> CryptoAlgorithmAESGCM::getKeyLength(const CryptoAlgorithmParameters& parameters)
{
    return CryptoKeyAES::getKeyLength(parameters);
}

} // namespace WebCore
