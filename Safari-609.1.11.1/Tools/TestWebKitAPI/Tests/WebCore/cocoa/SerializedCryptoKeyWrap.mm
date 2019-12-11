/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#import "config.h"

#import <WebCore/SerializedCryptoKeyWrap.h>
#import <wtf/MainThread.h>

namespace TestWebKitAPI {

class SerializedCryptoKeyWrapTest : public testing::Test {
public:
    virtual void SetUp()
    {
        WTF::initializeMainThread();
        WebCore::deleteDefaultWebCryptoMasterKey();
    }

    virtual void TearDown()
    {
        WebCore::deleteDefaultWebCryptoMasterKey();
    }
};

TEST_F(SerializedCryptoKeyWrapTest, GetDefaultWebCryptoMasterKey)
{
    Vector<uint8_t> masterKey1;
    EXPECT_TRUE(WebCore::getDefaultWebCryptoMasterKey(masterKey1));

    Vector<uint8_t> masterKey2;
    EXPECT_TRUE(WebCore::getDefaultWebCryptoMasterKey(masterKey2));

    EXPECT_TRUE(masterKey1 == masterKey2);
}

TEST_F(SerializedCryptoKeyWrapTest, DeleteDefaultWebCryptoMasterKey)
{
    Vector<uint8_t> masterKey1;
    EXPECT_TRUE(WebCore::getDefaultWebCryptoMasterKey(masterKey1));
    EXPECT_TRUE(WebCore::deleteDefaultWebCryptoMasterKey());

    Vector<uint8_t> masterKey2;
    EXPECT_TRUE(WebCore::getDefaultWebCryptoMasterKey(masterKey2));

    EXPECT_TRUE(masterKey1 != masterKey2);
}

TEST_F(SerializedCryptoKeyWrapTest, SerializedCryptoKeyWrapUnwrap)
{
    Vector<uint8_t> masterKey;
    EXPECT_TRUE(WebCore::getDefaultWebCryptoMasterKey(masterKey));

    Vector<uint8_t> cryptoKey(16, 1);
    Vector<uint8_t> wrappedKey;
    EXPECT_TRUE(WebCore::wrapSerializedCryptoKey(masterKey, cryptoKey, wrappedKey));
    EXPECT_TRUE(cryptoKey != wrappedKey);
    // ensure wrappedKey doesn't contain cryptoKey
    bool notContained = true;
    size_t limit = wrappedKey.size() - cryptoKey.size() + 1;
    for (size_t i = 0; i < limit; i++) {
        size_t j = 0;
        for (; j < cryptoKey.size() && wrappedKey[i + j] == cryptoKey[j]; j++) { }
        if (j >= cryptoKey.size()) {
            notContained = false;
            break;
        }
    }
    EXPECT_TRUE(notContained);

    Vector<uint8_t> unwrappedKey;
    EXPECT_TRUE(WebCore::unwrapSerializedCryptoKey(masterKey, wrappedKey, unwrappedKey));
    EXPECT_TRUE(unwrappedKey == cryptoKey);
}

} // namespace TestWebKitAPI
