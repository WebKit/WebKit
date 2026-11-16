/*
 * Copyright (C) 2026 Jonathan Powers. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "Helpers/Test.h"
#include <WebCore/CryptoAlgorithmAESCBC.h>
#include <WebCore/CryptoAlgorithmAesCbcCfbParams.h>
#include <WebCore/CryptoKeyAES.h>
#include <WebCore/ExceptionOr.h>
#include <WebCore/ScriptWrappableInlines.h>

namespace TestWebKitAPI {

using namespace WebCore;

TEST(CryptoAlgorithmAESCBC, DecryptEmptyCipherText)
{
    for (auto keySize : { 16, 24, 32 }) {
        auto key = CryptoKeyAES::importRaw(CryptoAlgorithmIdentifier::AES_CBC, Vector<uint8_t>(FillWith { }, keySize, 0), false, CryptoKeyUsageDecrypt);
        ASSERT_TRUE(key);

        auto result = CryptoAlgorithmAESCBC::platformDecrypt({ }, *key, { });
        ASSERT_TRUE(result.hasException());
        EXPECT_EQ(result.exception().code(), ExceptionCode::OperationError);
    }
}

TEST(CryptoAlgorithmAESCBC, EncryptDecryptEmptyPlainText)
{
    for (auto keySize : { 16, 24, 32 }) {
        auto key = CryptoKeyAES::importRaw(CryptoAlgorithmIdentifier::AES_CBC, Vector<uint8_t>(FillWith { }, keySize, 0), false, CryptoKeyUsageEncrypt | CryptoKeyUsageDecrypt);
        ASSERT_TRUE(key);

        auto encrypted = CryptoAlgorithmAESCBC::platformEncrypt({ }, *key, { });
        ASSERT_FALSE(encrypted.hasException());
        auto cipherText = encrypted.releaseReturnValue();
        EXPECT_EQ(cipherText.size(), 16u);

        auto decrypted = CryptoAlgorithmAESCBC::platformDecrypt({ }, *key, cipherText);
        ASSERT_FALSE(decrypted.hasException());
        EXPECT_TRUE(decrypted.releaseReturnValue().isEmpty());
    }
}

TEST(CryptoAlgorithmAESCBC, DecryptEmptyCipherTextWithoutPadding)
{
    for (auto keySize : { 16, 24, 32 }) {
        auto key = CryptoKeyAES::importRaw(CryptoAlgorithmIdentifier::AES_CBC, Vector<uint8_t>(FillWith { }, keySize, 0), false, CryptoKeyUsageDecrypt);
        ASSERT_TRUE(key);

        auto result = CryptoAlgorithmAESCBC::platformDecrypt({ }, *key, { }, CryptoAlgorithmAESCBC::Padding::No);
        ASSERT_FALSE(result.hasException());
        EXPECT_TRUE(result.releaseReturnValue().isEmpty());
    }
}

} // namespace TestWebKitAPI
