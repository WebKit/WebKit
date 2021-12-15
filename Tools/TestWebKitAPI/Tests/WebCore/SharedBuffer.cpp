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
#include <JavaScriptCore/ArrayBuffer.h>
#include <WebCore/SharedBuffer.h>
#if ENABLE(MHTML)
#include <WebCore/SharedBufferChunkReader.h>
#endif
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
    Vector<uint8_t> vector0(0x4000, 'a');
    Vector<uint8_t> vector1(0x4000, 'b');
    Vector<uint8_t> vector2(0x4000, 'c');

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

static void checkBuffer(const uint8_t* buffer, size_t bufferLength, const char* expected)
{
    // expected is null terminated, buffer is not.
    size_t length = strlen(expected);
    EXPECT_EQ(length, bufferLength);
    for (size_t i = 0; i < length; ++i)
        EXPECT_EQ(buffer[i], expected[i]);
}

TEST_F(SharedBufferTest, getSomeData)
{
    Vector<uint8_t> s1 = {'a', 'b', 'c', 'd'};
    Vector<uint8_t> s2 = {'e', 'f', 'g', 'h'};
    Vector<uint8_t> s3 = {'i', 'j', 'k', 'l'};
    
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
    auto makeBuffer = [] (Vector<Vector<uint8_t>>&& contents) {
        auto buffer = SharedBuffer::create();
        for (auto& content : contents)
            buffer->append(WTFMove(content));
        return buffer;
    };
    auto buffer1 = makeBuffer({{'a', 'b', 'c', 'd'}});
    EXPECT_EQ(buffer1.get(), buffer1.get());

    buffer1->append(Vector<uint8_t>({'a', 'b', 'c', 'd'}));
    EXPECT_EQ(buffer1.get(), makeBuffer({{'a', 'b', 'c', 'd', 'a', 'b', 'c', 'd'}}).get());
    EXPECT_EQ(makeBuffer({{'a'}, {'b', 'c'}, {'d'}}).get(), makeBuffer({{'a', 'b'}, {'c', 'd'}}).get());
    EXPECT_NE(makeBuffer({{'a', 'b'}}).get(), makeBuffer({{'a', 'b', 'c'}}).get());
    EXPECT_NE(makeBuffer({{'a', 'b'}}).get(), makeBuffer({{'b', 'c'}}).get());
    EXPECT_NE(makeBuffer({{'a'}, {'b'}}).get(), makeBuffer({{'a'}, {'a'}}).get());
}

TEST_F(SharedBufferTest, toHexString)
{
    Vector<uint8_t> t1 = {0x11, 0x5, 0x12};
    auto buffer = SharedBuffer::create();
    buffer->append(WTFMove(t1));
    String result = buffer->toHexString();
    EXPECT_EQ(result, "110512");
    buffer->clear();
    EXPECT_EQ(buffer->toHexString(), "");
}

TEST_F(SharedBufferTest, read)
{
    const char* const simpleText = "This is a simple test.";

    auto check = [](SharedBuffer& sharedBuffer) {
        Vector<uint8_t> data = sharedBuffer.read(4, 3);
        EXPECT_EQ(data.size(), 3u);
        EXPECT_EQ(String(data.data(), 3), " is");

        data = sharedBuffer.read(4, 1000);
        EXPECT_EQ(data.size(), 18u);

        EXPECT_EQ(String(data.data(), 18), " is a simple test.");
    };
    auto sharedBuffer = SharedBuffer::create(simpleText, strlen(simpleText));
    check(sharedBuffer);

    sharedBuffer = SharedBuffer::create();
    for (size_t i = 0; i < strlen(simpleText); i++)
        sharedBuffer->append(&simpleText[i], 1);
    check(sharedBuffer);

    sharedBuffer = SharedBuffer::create();
    for (size_t i = 0; i < strlen(simpleText); i += 2)
        sharedBuffer->append(&simpleText[i], 2);
    EXPECT_EQ(sharedBuffer->size(), strlen(simpleText));
    check(sharedBuffer);
}

#if ENABLE(MHTML)
// SharedBufferChunkReader unit-tests -------------------------------------
template< typename T, size_t N >
constexpr size_t arraysize( const T (&)[N] ) { return N; }

static void readAllChunks(std::vector<String>* chunks, SharedBuffer& buffer, const String& separator = "\r\n", bool includeSeparator = false)
{
    SharedBufferChunkReader chunkReader(&buffer, separator.utf8().data());
    String chunk = chunkReader.nextChunkAsUTF8StringWithLatin1Fallback(includeSeparator);
    while (!chunk.isNull()) {
        chunks->push_back(chunk);
        chunk = chunkReader.nextChunkAsUTF8StringWithLatin1Fallback(includeSeparator);
    }
}

static bool checkChunks(const std::vector<String>& chunks, const char* const expectedChunks[], size_t expectedSize)
{
    if (chunks.size() != expectedSize) {
        EXPECT_EQ(chunks.size(), expectedSize);
        return false;
    }

    for (size_t i = 0; i < chunks.size(); ++i) {
        if (chunks[i] != expectedChunks[i])
            return false;
    }
    return true;
}

static void checkDataInRange(const Vector<uint8_t>& data, size_t start, size_t length)
{
    ASSERT_EQ(data.size(), length);
    for (size_t i = 0; i < length; ++i)
        ASSERT_EQ(data[i], static_cast<uint8_t>(start + i));
}

TEST_F(SharedBufferChunkReaderTest, includeSeparator)
{
    auto check = [](SharedBuffer& sharedBuffer) {
        SharedBufferChunkReader chunkReader(&sharedBuffer, "\x10\x11\x12");
        Vector<uint8_t> out;
        EXPECT_TRUE(chunkReader.nextChunk(out));
        checkDataInRange(out, 0, 16);

        EXPECT_TRUE(chunkReader.nextChunk(out));
        checkDataInRange(out, 19, 237);

        EXPECT_FALSE(chunkReader.nextChunk(out));
    };
    uint8_t data[256];
    for (size_t i = 0; i < 256; ++i)
        data[i] = i;

    auto sharedBuffer = SharedBuffer::create(data, 256);
    check(sharedBuffer);
    sharedBuffer = SharedBuffer::create();
    for (size_t i = 0; i < 256; ++i) {
        char c = i;
        sharedBuffer->append(&c, 1);
    }
    check(sharedBuffer);
}

TEST_F(SharedBufferChunkReaderTest, peekData)
{
    const char* const simpleText = "This is a simple test.";

    auto check = [](SharedBuffer& sharedBuffer) {
        SharedBufferChunkReader chunkReader(&sharedBuffer, "is");

        String chunk = chunkReader.nextChunkAsUTF8StringWithLatin1Fallback();
        EXPECT_EQ(chunk, "Th");

        Vector<uint8_t> data;
        size_t read = chunkReader.peek(data, 3);
        EXPECT_EQ(read, 3u);

        EXPECT_EQ(String(data.data(), 3), " is");

        read = chunkReader.peek(data, 1000);
        EXPECT_EQ(read, 18u);

        EXPECT_EQ(String(data.data(), 18), " is a simple test.");

        // Ensure the cursor has not changed.
        chunk = chunkReader.nextChunkAsUTF8StringWithLatin1Fallback();
        EXPECT_EQ(chunk, " ");

        chunk = chunkReader.nextChunkAsUTF8StringWithLatin1Fallback();
        EXPECT_EQ(chunk, " a simple test.");

        chunk = chunkReader.nextChunkAsUTF8StringWithLatin1Fallback();
        EXPECT_TRUE(chunk.isNull());
    };
    auto sharedBuffer = SharedBuffer::create(simpleText, strlen(simpleText));
    check(sharedBuffer);

    sharedBuffer = SharedBuffer::create();
    for (size_t i = 0; i < strlen(simpleText); i++)
        sharedBuffer->append(&simpleText[i], 1);
    check(sharedBuffer);

    sharedBuffer = SharedBuffer::create();
    for (size_t i = 0; i < strlen(simpleText); i += 2)
        sharedBuffer->append(&simpleText[i], 2);
    EXPECT_EQ(sharedBuffer->size(), strlen(simpleText));
    check(sharedBuffer);
}

TEST_F(SharedBufferChunkReaderTest, readAllChunksInMultiSegment)
{
    const char* const simpleText = "This is the most ridiculous history there is.";
    auto check = [](SharedBuffer& sharedBuffer) {
        std::vector<String> chunks;
        const char* const expectedChunks1WithoutSeparator[] = { "Th", "s ", "s the most r", "d", "culous h", "story there ", "s." };
        readAllChunks(&chunks, sharedBuffer, "i");
        EXPECT_TRUE(checkChunks(chunks, expectedChunks1WithoutSeparator, arraysize(expectedChunks1WithoutSeparator)));

        chunks.clear();
        const char* const expectedChunks1WithSeparator[] = { "Thi", "s i", "s the most ri", "di", "culous hi", "story there i", "s." };
        readAllChunks(&chunks, sharedBuffer, "i", true);
        EXPECT_TRUE(checkChunks(chunks, expectedChunks1WithSeparator, arraysize(expectedChunks1WithSeparator)));

        chunks.clear();
        const char* const expectedChunks2WithoutSeparator[] = { "Th", " ", " the most ridiculous h", "tory there ", "." };
        readAllChunks(&chunks, sharedBuffer, "is");
        EXPECT_TRUE(checkChunks(chunks, expectedChunks2WithoutSeparator, arraysize(expectedChunks2WithoutSeparator)));

        chunks.clear();
        const char* const expectedChunks2WithSeparator[] = { "This", " is", " the most ridiculous his", "tory there is", "." };
        readAllChunks(&chunks, sharedBuffer, "is", true);
        EXPECT_TRUE(checkChunks(chunks, expectedChunks2WithSeparator, arraysize(expectedChunks2WithSeparator)));

        chunks.clear();
        const char* const expectedChunks3WithoutSeparator[] = { "This is the most ridiculous h", "ory there is." };
        readAllChunks(&chunks, sharedBuffer, "ist");
        EXPECT_TRUE(checkChunks(chunks, expectedChunks3WithoutSeparator, arraysize(expectedChunks3WithoutSeparator)));

        chunks.clear();
        const char* const expectedChunks3WithSeparator[] = { "This is the most ridiculous hist", "ory there is." };
        readAllChunks(&chunks, sharedBuffer, "ist", true);
        EXPECT_TRUE(checkChunks(chunks, expectedChunks3WithSeparator, arraysize(expectedChunks3WithSeparator)));
    };
    auto sharedBuffer = SharedBuffer::create(simpleText, strlen(simpleText));
    check(sharedBuffer);
    sharedBuffer = SharedBuffer::create();
    for (size_t i = 0; i < strlen(simpleText); i++)
        sharedBuffer->append(&simpleText[i], 1);
    EXPECT_EQ(sharedBuffer->size(), strlen(simpleText));
    check(sharedBuffer);

    sharedBuffer = SharedBuffer::create();
    for (size_t i = 0; i < strlen(simpleText); i += 5)
        sharedBuffer->append(&simpleText[i], 5);
    EXPECT_EQ(sharedBuffer->size(), strlen(simpleText));
    check(sharedBuffer);
}

TEST_F(SharedBufferChunkReaderTest, changingIterator)
{
    {
        const char* const simpleText = "This is the most ridiculous history there is.";
        auto sharedBuffer = SharedBuffer::create(simpleText, strlen(simpleText));
        SharedBufferChunkReader chunkReader(sharedBuffer.ptr(), "is");
        String chunk = chunkReader.nextChunkAsUTF8StringWithLatin1Fallback();
        EXPECT_EQ(chunk, "Th");

        chunk = chunkReader.nextChunkAsUTF8StringWithLatin1Fallback();
        EXPECT_EQ(chunk, " ");

        chunkReader.setSeparator("he");
        chunk = chunkReader.nextChunkAsUTF8StringWithLatin1Fallback();
        EXPECT_EQ(chunk, " t");

        chunk = chunkReader.nextChunkAsUTF8StringWithLatin1Fallback();
        EXPECT_EQ(chunk, " most ridiculous history t");

        // Set a non existing separator.
        chunkReader.setSeparator("tchinta");
        chunk = chunkReader.nextChunkAsUTF8StringWithLatin1Fallback();
        EXPECT_EQ(chunk, "re is.");

        // We should be at the end of the string, so any subsequent call to nextChunk should return null.
        chunkReader.setSeparator(".");
        chunk = chunkReader.nextChunkAsUTF8StringWithLatin1Fallback(true);
        EXPECT_TRUE(chunk.isNull());
    }

    {
        const char* const simpleText = "dog";
        auto sharedBuffer = SharedBuffer::create(simpleText, strlen(simpleText));
        const char* const expectedChunksWithoutSeparator[] = { "" };
        std::vector<String> chunks;
        readAllChunks(&chunks, sharedBuffer, "dog");
        EXPECT_TRUE(checkChunks(chunks, expectedChunksWithoutSeparator, arraysize(expectedChunksWithoutSeparator)));

        chunks.clear();
        const char* const expectedChunksWithSeparator[] = { "dog" };
        readAllChunks(&chunks, sharedBuffer, "dog", true);
        EXPECT_TRUE(checkChunks(chunks, expectedChunksWithSeparator, arraysize(expectedChunksWithSeparator)));
    }

    // Ends with repeated separators.
    {
        const char* const simpleText = "Beaucoup de chats catcatcatcatcat";
        auto sharedBuffer = SharedBuffer::create(simpleText, strlen(simpleText));
        const char* const expectedChunksWithoutSeparator[] = { "Beaucoup de chats ", "", "", "", "" };
        std::vector<String> chunks;
        readAllChunks(&chunks, sharedBuffer, "cat");
        EXPECT_TRUE(checkChunks(chunks, expectedChunksWithoutSeparator, arraysize(expectedChunksWithoutSeparator)));

        chunks.clear();
        const char* const expectedChunksWithSeparator[] = { "Beaucoup de chats cat", "cat", "cat", "cat", "cat" };
        readAllChunks(&chunks, sharedBuffer, "cat", true);
        EXPECT_TRUE(checkChunks(chunks, expectedChunksWithSeparator, arraysize(expectedChunksWithSeparator)));
    }
    {
        const char* const simpleText = "This is a simple test.\r\nNothing special.\r\n";
        const char* const expectedChunks[] = { "This is a simple test.", "Nothing special." };
        auto sharedBuffer = SharedBuffer::create(simpleText, strlen(simpleText));
        std::vector<String> chunks;
        readAllChunks(&chunks, sharedBuffer);
        EXPECT_TRUE(checkChunks(chunks, expectedChunks, arraysize(expectedChunks)));
    }

    {
        const char* const simpleText = "This is a simple test.\r\nNothing special.";
        const char* const expectedChunks[] = { "This is a simple test.", "Nothing special." };
        auto sharedBuffer = SharedBuffer::create(simpleText, strlen(simpleText));

        std::vector<String> chunks;
        readAllChunks(&chunks, sharedBuffer);
        EXPECT_TRUE(checkChunks(chunks, expectedChunks, arraysize(expectedChunks)));
    }

    {
        const char* const simpleText = "Simple line with no EOL.";
        const char* const expectedChunks[] = { "Simple line with no EOL." };
        auto sharedBuffer = SharedBuffer::create(simpleText, strlen(simpleText));

        std::vector<String> chunks;
        readAllChunks(&chunks, sharedBuffer);
        EXPECT_TRUE(checkChunks(chunks, expectedChunks, arraysize(expectedChunks)));
    }

    {
        const char* const simpleText = "Line that has a EOL\r\nand then ends with a CR\r";
        const char* const expectedChunks[] = { "Line that has a EOL", "and then ends with a CR\r" };
        auto sharedBuffer = SharedBuffer::create(simpleText, strlen(simpleText));

        std::vector<String> chunks;
        readAllChunks(&chunks, sharedBuffer);
        EXPECT_TRUE(checkChunks(chunks, expectedChunks, arraysize(expectedChunks)));
    }

    {
        const char* const simpleText = "Repeated CRs should not cause probems\r\r\r\nShouln't they?";
        const char* const expectedChunks[] = { "Repeated CRs should not cause probems\r\r", "Shouln't they?" };
        auto sharedBuffer = SharedBuffer::create(simpleText, strlen(simpleText));

        std::vector<String> chunks;
        readAllChunks(&chunks, sharedBuffer);
        EXPECT_TRUE(checkChunks(chunks, expectedChunks, arraysize(expectedChunks)));
    }
    {
        const char* const simpleText = "EOL\r\n betwe\r\nen segments";
        const char* const expectedChunks[] = { "EOL", " betwe", "en segments" };
        auto sharedBuffer = SharedBuffer::create(simpleText, strlen(simpleText));

        std::vector<String> chunks;
        readAllChunks(&chunks, sharedBuffer);
        EXPECT_TRUE(checkChunks(chunks, expectedChunks, arraysize(expectedChunks)));
    }
}
#endif

}
