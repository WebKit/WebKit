/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "WebSocketDeflater.h"

#include <gtest/gtest.h>
#include <wtf/Vector.h>

using namespace WebCore;

namespace {

TEST(WebSocketDeflaterTest, TestCompressHello)
{
    // Test the first example on section 4.3 of the specification.
    OwnPtr<WebSocketDeflater> deflater = WebSocketDeflater::create(15);
    ASSERT_TRUE(deflater->initialize());
    OwnPtr<WebSocketInflater> inflater = WebSocketInflater::create();
    ASSERT_TRUE(inflater->initialize());
    const char* inputData = "Hello";
    const size_t inputLength = strlen(inputData);

    ASSERT_TRUE(deflater->addBytes(inputData, inputLength));
    ASSERT_TRUE(deflater->finish());
    const char* expectedFirst = "\xf2\x48\xcd\xc9\xc9\x07\x00";
    EXPECT_EQ(7U, deflater->size());
    EXPECT_EQ(0, memcmp(expectedFirst, deflater->data(), deflater->size()));
    ASSERT_TRUE(inflater->addBytes(deflater->data(), deflater->size()));
    ASSERT_TRUE(inflater->finish());
    EXPECT_EQ(inputLength, inflater->size());
    EXPECT_EQ(0, memcmp(inputData, inflater->data(), inflater->size()));

    deflater->reset();
    inflater->reset();

    ASSERT_TRUE(deflater->addBytes(inputData, inputLength));
    ASSERT_TRUE(deflater->finish());
    const char* expectedSecond = "\xf2\x00\x11\x00\x00";
    EXPECT_EQ(5U, deflater->size());
    EXPECT_EQ(0, memcmp(expectedSecond, deflater->data(), deflater->size()));
    ASSERT_TRUE(inflater->addBytes(deflater->data(), deflater->size()));
    ASSERT_TRUE(inflater->finish());
    EXPECT_EQ(inputLength, inflater->size());
    EXPECT_EQ(0, memcmp(inputData, inflater->data(), inflater->size()));
}

TEST(WebSocketDeflaterTest, TestMultipleAddBytesCalls)
{
    OwnPtr<WebSocketDeflater> deflater = WebSocketDeflater::create(15);
    ASSERT_TRUE(deflater->initialize());
    OwnPtr<WebSocketInflater> inflater = WebSocketInflater::create();
    ASSERT_TRUE(inflater->initialize());
    Vector<char> inputData(32);
    inputData.fill('a');

    for (size_t i = 0; i < inputData.size(); ++i)
        ASSERT_TRUE(deflater->addBytes(inputData.data() + i, 1));
    ASSERT_TRUE(deflater->finish());
    for (size_t i = 0; i < deflater->size(); ++i)
        ASSERT_TRUE(inflater->addBytes(deflater->data() + i, 1));
    ASSERT_TRUE(inflater->finish());
    EXPECT_EQ(inputData.size(), inflater->size());
    EXPECT_EQ(0, memcmp(inputData.data(), inflater->data(), inflater->size()));
}

TEST(WebSocketDeflaterTest, TestNoContextTakeOver)
{
    OwnPtr<WebSocketDeflater> deflater = WebSocketDeflater::create(15, WebSocketDeflater::DoNotTakeOverContext);
    ASSERT_TRUE(deflater->initialize());
    OwnPtr<WebSocketInflater> inflater = WebSocketInflater::create();
    ASSERT_TRUE(inflater->initialize());
    const char* expected = "\xf2\x48\xcd\xc9\xc9\x07\x00";
    const char* inputData = "Hello";
    const size_t inputLength = strlen(inputData);

    // If we don't take over context, the second result should be the identical
    // with the first one.
    for (size_t i = 0; i < 2; ++i) {
        ASSERT_TRUE(deflater->addBytes(inputData, inputLength));
        ASSERT_TRUE(deflater->finish());
        EXPECT_EQ(7U, deflater->size());
        EXPECT_EQ(0, memcmp(expected, deflater->data(), deflater->size()));
        ASSERT_TRUE(inflater->addBytes(deflater->data(), deflater->size()));
        ASSERT_TRUE(inflater->finish());
        EXPECT_EQ(inputLength, inflater->size());
        EXPECT_EQ(0, memcmp(inputData, inflater->data(), inflater->size()));
        deflater->reset();
        inflater->reset();
    }
}

TEST(WebSocketDeflaterTest, TestWindowBits)
{
    Vector<char> inputData(1024 + 64 * 2);
    inputData.fill('a');
    // Modify the head and tail of the inputData so that back-reference
    // can be used if the window size is sufficiently-large.
    for (size_t j = 0; j < 64; ++j) {
        inputData[j] = 'b';
        inputData[inputData.size() - j - 1] = 'b';
    }

    OwnPtr<WebSocketDeflater> deflater = WebSocketDeflater::create(8);
    ASSERT_TRUE(deflater->initialize());
    ASSERT_TRUE(deflater->addBytes(inputData.data(), inputData.size()));
    ASSERT_TRUE(deflater->finish());

    OwnPtr<WebSocketInflater> inflater = WebSocketInflater::create(8);
    ASSERT_TRUE(inflater->initialize());
    ASSERT_TRUE(inflater->addBytes(deflater->data(), deflater->size()));
    ASSERT_TRUE(inflater->finish());
    EXPECT_EQ(inputData.size(), inflater->size());
    EXPECT_EQ(0, memcmp(inputData.data(), inflater->data(), inflater->size()));
}

TEST(WebSocketDeflaterTest, TestLargeData)
{
    OwnPtr<WebSocketDeflater> deflater = WebSocketDeflater::create(15);
    ASSERT_TRUE(deflater->initialize());
    OwnPtr<WebSocketInflater> inflater = WebSocketInflater::create();
    ASSERT_TRUE(inflater->initialize());
    Vector<char> inputData(16 * 1024 * 1024);
    inputData.fill('a');

    ASSERT_TRUE(deflater->addBytes(inputData.data(), inputData.size()));
    ASSERT_TRUE(deflater->finish());
    ASSERT_TRUE(inflater->addBytes(deflater->data(), deflater->size()));
    ASSERT_TRUE(inflater->finish());
    EXPECT_EQ(inputData.size(), inflater->size());
    EXPECT_EQ(0, memcmp(inputData.data(), inflater->data(), inflater->size()));
}

}
