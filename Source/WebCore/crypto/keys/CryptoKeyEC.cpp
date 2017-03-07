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
#include "CryptoKeyEC.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoAlgorithmRegistry.h"
#include "CryptoKeyData.h"
#include "ExceptionCode.h"

namespace WebCore {

static const char* const P256 = "P-256";
static const char* const P384 = "P-384";

CryptoKeyEC::CryptoKeyEC(CryptoAlgorithmIdentifier identifier, NamedCurve curve, CryptoKeyType type, PlatformECKey platformKey, bool extractable, CryptoKeyUsageBitmap usages)
    : CryptoKey(identifier, type, extractable, usages)
    , m_platformKey(platformKey)
    , m_curve(curve)
{
}

ExceptionOr<CryptoKeyPair> CryptoKeyEC::generatePair(CryptoAlgorithmIdentifier identifier, const String& namedCurve, bool extractable, CryptoKeyUsageBitmap usages)
{
    NamedCurve curve;
    if (namedCurve == P256)
        curve = NamedCurve::P256;
    else if (namedCurve == P384)
        curve = NamedCurve::P384;
    else
        return Exception { NOT_SUPPORTED_ERR };

    auto result = platformGeneratePair(identifier, curve, extractable, usages);
    if (!result)
        return Exception { OperationError };

    return WTFMove(*result);
}

std::unique_ptr<KeyAlgorithm> CryptoKeyEC::buildAlgorithm() const
{
    String name = CryptoAlgorithmRegistry::singleton().name(algorithmIdentifier());
    switch (m_curve) {
    case NamedCurve::P256:
        return std::make_unique<EcKeyAlgorithm>(name, String(P256));
    case NamedCurve::P384:
        return std::make_unique<EcKeyAlgorithm>(name, String(P384));
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

std::unique_ptr<CryptoKeyData> CryptoKeyEC::exportData() const
{
    // A dummy implementation for now.
    return std::make_unique<CryptoKeyData>(CryptoKeyData::Format::OctetSequence);
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)
