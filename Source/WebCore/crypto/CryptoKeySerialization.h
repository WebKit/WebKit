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

#ifndef CryptoKeySerialization_h
#define CryptoKeySerialization_h

#include "CryptoKeyUsage.h"
#include <wtf/Noncopyable.h>

#if ENABLE(SUBTLE_CRYPTO)

namespace WebCore {

class CryptoAlgorithm;
class CryptoAlgorithmParameters;
class CryptoKeyData;

typedef std::pair<const uint8_t*, size_t> CryptoOperationData;

class CryptoKeySerialization {
    WTF_MAKE_NONCOPYABLE(CryptoKeySerialization);
public:
    CryptoKeySerialization() { }
    virtual ~CryptoKeySerialization() { }

    // Returns false if suggested algorithm was not compatible with one stored in the serialization.
    virtual bool reconcileAlgorithm(std::unique_ptr<CryptoAlgorithm>&, std::unique_ptr<CryptoAlgorithmParameters>&) const = 0;

    virtual void reconcileUsages(CryptoKeyUsage&) const = 0;
    virtual void reconcileExtractable(bool&) const = 0;

    virtual std::unique_ptr<CryptoKeyData> keyData() const = 0;
};

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)
#endif // CryptoKeySerialization_h
