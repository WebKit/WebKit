/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2015 Canon Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "SharedBufferTest.h"
#include "Test.h"
#include <WebCore/SharedBuffer.h>
#include <wtf/MainThread.h>
#include <wtf/StringExtras.h>

using namespace WebCore;

namespace TestWebKitAPI {

TEST_F(SharedBufferTest, createWithContentsOfMissingFile)
{
    RefPtr<SharedBuffer> buffer = SharedBuffer::createWithContentsOfFile(String("not_existing_file"));
    ASSERT_NULL(buffer);
}

TEST_F(SharedBufferTest, createWithContentsOfExistingFile)
{
    RefPtr<SharedBuffer> buffer = SharedBuffer::createWithContentsOfFile(tempFilePath());
    ASSERT_NOT_NULL(buffer);
    EXPECT_TRUE(buffer->size() == strlen(SharedBufferTest::testData()));
    EXPECT_TRUE(String(SharedBufferTest::testData()) == String(buffer->data(), buffer->size()));
}

TEST_F(SharedBufferTest, createWithContentsOfExistingEmptyFile)
{
    RefPtr<SharedBuffer> buffer = SharedBuffer::createWithContentsOfFile(tempEmptyFilePath());
    ASSERT_NOT_NULL(buffer);
    EXPECT_TRUE(buffer->isEmpty());
}

TEST_F(SharedBufferTest, copyBufferCreatedWithContentsOfExistingFile)
{
    RefPtr<SharedBuffer> buffer = SharedBuffer::createWithContentsOfFile(tempFilePath());
    ASSERT_NOT_NULL(buffer);
    RefPtr<SharedBuffer> copy = buffer->copy();
    EXPECT_GT(buffer->size(), 0U);
    EXPECT_TRUE(buffer->size() == copy->size());
    EXPECT_TRUE(!memcmp(buffer->data(), copy->data(), buffer->size()));
}

TEST_F(SharedBufferTest, clearBufferCreatedWithContentsOfExistingFile)
{
    RefPtr<SharedBuffer> buffer = SharedBuffer::createWithContentsOfFile(tempFilePath());
    ASSERT_NOT_NULL(buffer);
    buffer->clear();
    EXPECT_TRUE(!buffer->size());
    EXPECT_TRUE(!buffer->data());
}

TEST_F(SharedBufferTest, appendBufferCreatedWithContentsOfExistingFile)
{
    RefPtr<SharedBuffer> buffer = SharedBuffer::createWithContentsOfFile(tempFilePath());
    ASSERT_NOT_NULL(buffer);
    buffer->append("a", 1);
    EXPECT_TRUE(buffer->size() == (strlen(SharedBufferTest::testData()) + 1));
    EXPECT_TRUE(!memcmp(buffer->data(), SharedBufferTest::testData(), strlen(SharedBufferTest::testData())));
    EXPECT_EQ('a', buffer->data()[strlen(SharedBufferTest::testData())]);
}

TEST_F(SharedBufferTest, tryCreateArrayBuffer)
{
    char testData0[] = "Hello";
    char testData1[] = "World";
    char testData2[] = "Goodbye";
    RefPtr<SharedBuffer> sharedBuffer = SharedBuffer::create(testData0, strlen(testData0));
    sharedBuffer->append(testData1, strlen(testData1));
    sharedBuffer->append(testData2, strlen(testData2));
    RefPtr<ArrayBuffer> arrayBuffer = sharedBuffer->tryCreateArrayBuffer();
    char expectedConcatenation[] = "HelloWorldGoodbye";
    ASSERT_EQ(strlen(expectedConcatenation), arrayBuffer->byteLength());
    EXPECT_EQ(0, memcmp(expectedConcatenation, arrayBuffer->data(), strlen(expectedConcatenation)));
}

TEST_F(SharedBufferTest, tryCreateArrayBufferLargeSegments)
{
    Vector<char> vector0(0x4000, 'a');
    Vector<char> vector1(0x4000, 'b');
    Vector<char> vector2(0x4000, 'c');

    RefPtr<SharedBuffer> sharedBuffer = SharedBuffer::create(WTFMove(vector0));
    sharedBuffer->append(WTFMove(vector1));
    sharedBuffer->append(WTFMove(vector2));
    RefPtr<ArrayBuffer> arrayBuffer = sharedBuffer->tryCreateArrayBuffer();
    ASSERT_EQ(0x4000U + 0x4000U + 0x4000U, arrayBuffer->byteLength());
    int position = 0;
    for (int i = 0; i < 0x4000; ++i) {
        EXPECT_EQ('a', static_cast<char*>(arrayBuffer->data())[position]);
        ++position;
    }
    for (int i = 0; i < 0x4000; ++i) {
        EXPECT_EQ('b', static_cast<char*>(arrayBuffer->data())[position]);
        ++position;
    }
    for (int i = 0; i < 0x4000; ++i) {
        EXPECT_EQ('c', static_cast<char*>(arrayBuffer->data())[position]);
        ++position;
    }
}

TEST_F(SharedBufferTest, copy)
{
    char testData[] = "Habitasse integer eros tincidunt a scelerisque! Enim elit? Scelerisque magnis,"
    "et montes ultrices tristique a! Pid. Velit turpis, dapibus integer rhoncus sociis amet facilisis,"
    "adipiscing pulvinar nascetur magnis tempor sit pulvinar, massa urna enim porttitor sociis sociis proin enim?"
    "Lectus, platea dolor, integer a. A habitasse hac nunc, nunc, nec placerat vut in sit nunc nec, sed. Sociis,"
    "vut! Hac, velit rhoncus facilisis. Rhoncus et, enim, sed et in tristique nunc montes,"
    "natoque nunc sagittis elementum parturient placerat dolor integer? Pulvinar,"
    "magnis dignissim porttitor ac pulvinar mid tempor. A risus sed mid! Magnis elit duis urna,"
    "cras massa, magna duis. Vut magnis pid a! Penatibus aliquet porttitor nunc, adipiscing massa odio lundium,"
    "risus elementum ac turpis massa pellentesque parturient augue. Purus amet turpis pid aliquam?"
    "Dolor est tincidunt? Dolor? Dignissim porttitor sit in aliquam! Tincidunt, non nunc, rhoncus dictumst!"
    "Porta augue etiam. Cursus augue nunc lacus scelerisque. Rhoncus lectus, integer hac, nec pulvinar augue massa,"
    "integer amet nisi facilisis? A! A, enim velit pulvinar elit in non scelerisque in et ultricies amet est!"
    "in porttitor montes lorem et, hac aliquet pellentesque a sed? Augue mid purus ridiculus vel dapibus,"
    "sagittis sed, tortor auctor nascetur rhoncus nec, rhoncus, magna integer. Sit eu massa vut?"
    "Porta augue porttitor elementum, enim, rhoncus pulvinar duis integer scelerisque rhoncus natoque,"
    "mattis dignissim massa ac pulvinar urna, nunc ut. Sagittis, aliquet penatibus proin lorem, pulvinar lectus,"
    "augue proin! Ac, arcu quis. Placerat habitasse, ridiculus ridiculus.";
    unsigned length = strlen(testData);
    RefPtr<SharedBuffer> sharedBuffer = SharedBuffer::create(testData, length);
    sharedBuffer->append(testData, length);
    sharedBuffer->append(testData, length);
    sharedBuffer->append(testData, length);
    // sharedBuffer must contain data more than segmentSize (= 0x1000) to check copy().
    ASSERT_EQ(length * 4, sharedBuffer->size());
    RefPtr<SharedBuffer> clone = sharedBuffer->copy();
    ASSERT_EQ(length * 4, clone->size());
    ASSERT_EQ(0, memcmp(clone->data(), sharedBuffer->data(), clone->size()));
    clone->append(testData, length);
    ASSERT_EQ(length * 5, clone->size());
}

static void checkBuffer(const char* buffer, size_t bufferLength, const char* expected)
{
    // expected is null terminated, buffer is not.
    size_t length = strlen(expected);
    EXPECT_EQ(length, bufferLength);
    for (size_t i = 0; i < length; ++i)
        EXPECT_EQ(buffer[i], expected[i]);
}

TEST_F(SharedBufferTest, getSomeData)
{
    Vector<char> s1 = {'a', 'b', 'c', 'd'};
    Vector<char> s2 = {'e', 'f', 'g', 'h'};
    Vector<char> s3 = {'i', 'j', 'k', 'l'};
    
    auto buffer = SharedBuffer::create();
    buffer->append(WTFMove(s1));
    buffer->append(WTFMove(s2));
    buffer->append(WTFMove(s3));
    
    auto abcd = buffer->getSomeData(0);
    auto gh = buffer->getSomeData(6);
    auto h = buffer->getSomeData(7);
    auto ijkl = buffer->getSomeData(8);
    auto kl = buffer->getSomeData(10);
    auto abcdefghijkl = buffer->data();
    auto ghijkl = buffer->getSomeData(6);
    auto l = buffer->getSomeData(11);
    checkBuffer(abcd.data(), abcd.size(), "abcd");
    checkBuffer(gh.data(), gh.size(), "gh");
    checkBuffer(h.data(), h.size(), "h");
    checkBuffer(ijkl.data(), ijkl.size(), "ijkl");
    checkBuffer(kl.data(), kl.size(), "kl");
    checkBuffer(abcdefghijkl, buffer->size(), "abcdefghijkl");
    checkBuffer(ghijkl.data(), ghijkl.size(), "ghijkl");
    checkBuffer(l.data(), l.size(), "l");
}

TEST_F(SharedBufferTest, isEqualTo)
{
    auto makeBuffer = [] (Vector<Vector<char>>&& contents) {
        auto buffer = SharedBuffer::create();
        for (auto& content : contents)
            buffer->append(WTFMove(content));
        return buffer;
    };
    auto buffer1 = makeBuffer({{'a', 'b', 'c', 'd'}});
    EXPECT_EQ(buffer1, buffer1);

    buffer1->append(Vector<char>({'a', 'b', 'c', 'd'}));
    EXPECT_EQ(buffer1, makeBuffer({{'a', 'b', 'c', 'd', 'a', 'b', 'c', 'd'}}));
    EXPECT_EQ(makeBuffer({{'a'}, {'b', 'c'}, {'d'}}), makeBuffer({{'a', 'b'}, {'c', 'd'}}));
    EXPECT_NE(makeBuffer({{'a', 'b'}}), makeBuffer({{'a', 'b', 'c'}}));
    EXPECT_NE(makeBuffer({{'a', 'b'}}), makeBuffer({{'b', 'c'}}));
    EXPECT_NE(makeBuffer({{'a'}, {'b'}}), makeBuffer({{'a'}, {'a'}}));
}

}
