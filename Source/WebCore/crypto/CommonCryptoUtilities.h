/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_CRYPTO)

#include "CryptoAlgorithmIdentifier.h"
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonRandom.h>
#include <pal/spi/cocoa/CommonCryptoSPI.h>
#include <wtf/Vector.h>

#if !HAVE(CCRSAGetCRTComponents)
typedef struct _CCBigNumRef *CCBigNumRef;
#endif

namespace WebCore {

#if !HAVE(CCRSAGetCRTComponents)

// Only need CCBigNum for the code used when we don't have CCRSAGetCRTComponents.
class CCBigNum {
public:
    CCBigNum(const uint8_t*, size_t);
    ~CCBigNum();

    CCBigNum(const CCBigNum&);
    CCBigNum(CCBigNum&&);
    CCBigNum& operator=(const CCBigNum&);
    CCBigNum& operator=(CCBigNum&&);

    Vector<uint8_t> data() const;

    CCBigNum operator-(uint32_t) const;
    CCBigNum operator%(const CCBigNum&) const;
    CCBigNum inverse(const CCBigNum& modulus) const;

private:
    CCBigNum(CCBigNumRef);

    CCBigNumRef m_number;
};

#endif

bool getCommonCryptoDigestAlgorithm(CryptoAlgorithmIdentifier, CCDigestAlgorithm&);

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO)
