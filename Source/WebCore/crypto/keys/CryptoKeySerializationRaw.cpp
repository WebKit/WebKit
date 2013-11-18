/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "CryptoKeySerializationRaw.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoAlgorithm.h"
#include "CryptoKey.h"
#include "CryptoKeyDataOctetSequence.h"

namespace WebCore {

CryptoKeySerializationRaw::CryptoKeySerializationRaw(const CryptoOperationData& data)
{
    m_data.append(data.first, data.second);
}

CryptoKeySerializationRaw::~CryptoKeySerializationRaw()
{
}

bool CryptoKeySerializationRaw::reconcileAlgorithm(std::unique_ptr<CryptoAlgorithm>&, std::unique_ptr<CryptoAlgorithmParameters>&) const
{
    return true;
}

void CryptoKeySerializationRaw::reconcileUsages(CryptoKeyUsage&) const
{
}

void CryptoKeySerializationRaw::reconcileExtractable(bool&) const
{
}

std::unique_ptr<CryptoKeyData> CryptoKeySerializationRaw::keyData() const
{
    return CryptoKeyDataOctetSequence::create(m_data);
}

bool CryptoKeySerializationRaw::serialize(const CryptoKey& key, Vector<uint8_t>& result)
{
    std::unique_ptr<CryptoKeyData> keyData = key.exportData();
    if (!keyData) {
        // This generally shouldn't happen as long as all key types implement exportData(), but as underlying libraries return errors, there may be some rare failure conditions.
        return false;
    }

    if (!isCryptoKeyDataOctetSequence(*keyData))
        return false;

    result.appendVector(toCryptoKeyDataOctetSequence(*keyData).octetSequence());
    return true;
}


} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)
