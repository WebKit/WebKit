/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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

#include "CommonCryptoUtilities.h"
#include "CryptoAlgorithmAesGcmParams.h"
#include "CryptoKeyAES.h"
#include "CryptoTypesBridging.h"
#include <pal/crypto/CryptoAlgorithmAESGCMCocoa.h>

namespace WebCore {

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESGCM::platformEncrypt(const CryptoAlgorithmAesGcmParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& plainText)
{
    if (parameters.ivVector().size() >= 12)
        return toException(PAL::Crypto::encryptCryptoKitAESGCM(parameters.ivVector(), key.key(), plainText, parameters.additionalDataVector(), parameters.tagLength.value_or(0) / 8));
    return toException(PAL::Crypto::encryptAESGCM(parameters.ivVector(), key.key(), plainText, parameters.additionalDataVector(), parameters.tagLength.value_or(0) / 8));
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESGCM::platformDecrypt(const CryptoAlgorithmAesGcmParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& cipherText)
{
    // FIXME: Add decrypt with CryptoKit once rdar://92701544 is resolved.
    return toException(PAL::Crypto::decyptAESGCM(parameters.ivVector(), key.key(), cipherText, parameters.additionalDataVector(), parameters.tagLength.value_or(0) / 8));
}

} // namespace WebCore
