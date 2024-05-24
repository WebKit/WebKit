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

#pragma once

#include "BufferSource.h"
#include "CryptoAlgorithmAesCtrParamsInit.h"
#include "CryptoAlgorithmParameters.h"
#include <wtf/Vector.h>

namespace WebCore {

class CryptoAlgorithmAesCtrParams final : public CryptoAlgorithmParameters {
public:
    std::optional<BufferSource> counter;
    size_t length;

    CryptoAlgorithmAesCtrParams(CryptoAlgorithmIdentifier identifier)
        : CryptoAlgorithmParameters { WTFMove(identifier) }
    {
    }

    CryptoAlgorithmAesCtrParams(CryptoAlgorithmIdentifier identifier, CryptoAlgorithmAesCtrParamsInit init)
        : CryptoAlgorithmParameters { WTFMove(identifier), WTFMove(init) }
        , counter { WTFMove(init.counter) }
        , length { WTFMove(init.length) }
    {
    }

    Class parametersClass() const final { return Class::AesCtrParams; }

    const Vector<uint8_t>& counterVector() const
    {
        if (!m_counterVector.isEmpty() || !counter || !counter->length())
            return m_counterVector;

        if (counter)
            m_counterVector.append(counter->span());
        return m_counterVector;
    }

    CryptoAlgorithmAesCtrParams isolatedCopy() const
    {
        CryptoAlgorithmAesCtrParams result { identifier };
        result.m_counterVector = counterVector();
        result.length = length;

        return result;
    }

private:
    mutable Vector<uint8_t> m_counterVector;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CRYPTO_ALGORITHM_PARAMETERS(AesCtrParams)
