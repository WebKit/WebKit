/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
 * AND ANY EXPRESS OOR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
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

TEST_F(FragmentedSharedBufferTest, createWithContentsOfMissingFile)
{
    auto buffer = SharedBuffer::createWithContentsOfFile(String("not_existing_file"_s));
    ASSERT_NULL(buffer);
}

TEST_F(FragmentedSharedBufferTest, createWithContentsOfExistingFile)
{
    auto buffer = SharedBuffer::createWithContentsOfFile(tempFilePath());
    ASSERT_NOT_NULL(buffer);
    EXPECT_TRUE(buffer->size() == strlen(FragmentedSharedBufferTest::testData()));
    EXPECT_TRUE(String::fromLatin1(FragmentedSharedBufferTest::testData()) == String(buffer->makeContiguous()->data(), buffer->size()));
}

TEST_F(FragmentedSharedBufferTest, createWithContentsOfExistingEmptyFile)
{
    auto buffer = SharedBuffer::createWithContentsOfFile(tempEmptyFilePath());
    ASSERT_NOT_NULL(buffer);
    EXPECT_TRUE(buffer->isContiguous());
    EXPECT_TRUE(buffer->isEmpty());
}

TEST_F(FragmentedSharedBufferTest, copyBufferCreatedWithContentsOfExistingFile)
{
    auto buffer = SharedBuffer::createWithContentsOfFile(tempFilePath());
    ASSERT_NOT_NULL(buffer);
    EXPECT_TRUE(buffer->isContiguous());
    auto copy = buffer->copy();
    EXPECT_GT(buffer->size(), 0U);
    EXPECT_TRUE(buffer->size() == copy->size());
    EXPECT_TRUE(!memcmp(buffer->data(), copy->makeContiguous()->data(), buffer->size()));
}

TEST_F(FragmentedSharedBufferTest, appendBufferCreatedWithContentsOfExistingFile)
{
    RefPtr<FragmentedSharedBuffer> buffer = SharedBuffer::createWithContentsOfFile(tempFilePath());
    ASSERT_NOT_NULL(buffer);
    SharedBufferBuilder builder;
    builder.append(*buffer);
    builder.append("a", 1);
    EXPECT_TRUE(builder.size() == (strlen(FragmentedSharedBufferTest::testData()) + 1));
    EXPECT_TRUE(!memcmp(builder.get()->makeContiguous()->data(), FragmentedSharedBufferTest::testData(), strlen(FragmentedSharedBufferTest::testData())));
    EXPECT_EQ('a', builder.get()->makeContiguous()->data()[strlen(FragmentedSharedBufferTest::testData())]);
}

TEST_F(FragmentedSharedBufferTest, tryCreateArrayBuffer)
{
    char testData0[] = "Hello";
    char testData1[] = "World";
    char testData2[] = "Goodbye";
    SharedBufferBuilder builder(std::in_place, testData0, strlen(testData0));
    builder.append(testData1, strlen(testData1));
    builder.append(testData2, strlen(testData2));
    RefPtr<ArrayBuffer> arrayBuffer = builder.get()->tryCreateArrayBuffer();
    char expectedConcatenation[] = "HelloWorldGoodbye";
    ASSERT_EQ(strlen(expectedConcatenation), arrayBuffer->byteLength());
    EXPECT_EQ(0, memcmp(expectedConcatenation, arrayBuffer->data(), strlen(expectedConcatenation)));
}

TEST_F(FragmentedSharedBufferTest, tryCreateArrayBufferLargeSegments)
{
    Vector<uint8_t> vector0(0x4000, 'a');
    Vector<uint8_t> vector1(0x4000, 'b');
    Vector<uint8_t> vector2(0x4000, 'c');

    SharedBufferBuilder builder(std::in_place, WTFMove(vector0));
    builder.append(WTFMove(vector1));
    builder.append(WTFMove(vector2));
    RefPtr<ArrayBuffer> arrayBuffer = builder.get()->tryCreateArrayBuffer();
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

TEST_F(FragmentedSharedBufferTest, copy)
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
    SharedBufferBuilder builder1(std::in_place, testData, length);
    builder1.append(testData, length);
    builder1.append(testData, length);
    builder1.append(testData, length);
    // sharedBuffer must contain data more than segmentSize (= 0x1000) to check copy().
    EXPECT_EQ(length * 4, builder1.size());
    RefPtr<FragmentedSharedBuffer> clone = builder1.copy();
    EXPECT_EQ(length * 4, clone->size());
    EXPECT_EQ(0, memcmp(clone->makeContiguous()->data(), builder1.get()->makeContiguous()->data(), clone->size()));

    SharedBufferBuilder builder2;
    builder2.append(*clone);
    builder2.append(testData, length);
    EXPECT_EQ(length * 5, builder2.size());
    auto buffer = builder2.take();
    EXPECT_EQ(length * 5, buffer->size());
    // builder is now empty.
    EXPECT_EQ(0U, builder2.size());
    EXPECT_TRUE(builder2.isNull());
    EXPECT_TRUE(builder2.isEmpty());
}

TEST_F(FragmentedSharedBufferTest, builder)
{
    char testData0[] = "Hello";
    SharedBufferBuilder builder1(std::in_place, testData0, strlen(testData0));
    EXPECT_FALSE(builder1.isNull());
    EXPECT_FALSE(builder1.isEmpty());
    auto copy = builder1.copy();
    auto buffer = builder1.take();
    EXPECT_TRUE(builder1.isNull());
    EXPECT_TRUE(builder1.isEmpty());
    EXPECT_NE(copy.ptr(), buffer.ptr()); // We have different FragmentedSharedBuffer
    EXPECT_EQ(copy.get(), buffer.get()); // But their content is identical

    SharedBufferBuilder builder2;
    EXPECT_TRUE(builder2.isNull());
    EXPECT_TRUE(builder2.isEmpty());
    builder2.append(testData0, strlen(testData0));
    EXPECT_FALSE(builder2.isNull());
    EXPECT_FALSE(builder2.isEmpty());
    builder2.reset();
    EXPECT_TRUE(builder2.isNull());
    EXPECT_TRUE(builder2.isEmpty());
    builder2.empty();
    EXPECT_FALSE(builder2.isNull());
    EXPECT_TRUE(builder2.isEmpty());
}

static void checkBufferWithLength(const uint8_t* buffer, size_t bufferLength, const char* expected, size_t length)
{
    ASSERT_EQ(length, bufferLength);
    for (size_t i = 0; i < length; ++i)
        EXPECT_EQ(buffer[i], expected[i]);
}

static void checkBuffer(const uint8_t* buffer, size_t bufferLength, const char* expected)
{
    // expected is null terminated, buffer is not.
    size_t length = strlen(expected);
    checkBufferWithLength(buffer, bufferLength, expected, length);
}

TEST_F(FragmentedSharedBufferTest, getSomeData)
{
    Vector<uint8_t> s1 = { 'a', 'b', 'c', 'd' };
    Vector<uint8_t> s2 = { 'e', 'f', 'g', 'h' };
    Vector<uint8_t> s3 = { 'i', 'j', 'k', 'l' };

    SharedBufferBuilder builder;
    builder.append(WTFMove(s1));
    builder.append(WTFMove(s2));
    builder.append(WTFMove(s3));
    auto buffer = builder.take();
    
    auto abcd = buffer->getSomeData(0);
    auto gh1 = buffer->getSomeData(6);
    auto h = buffer->getSomeData(7);
    auto ijkl = buffer->getSomeData(8);
    auto kl = buffer->getSomeData(10);
    auto contiguousBuffer = buffer->makeContiguous();
    auto abcdefghijkl = contiguousBuffer->data();
    auto gh2 = buffer->getSomeData(6);
    auto ghijkl = contiguousBuffer->getSomeData(6);
    auto l = buffer->getSomeData(11);
    checkBuffer(abcd.data(), abcd.size(), "abcd");
    checkBuffer(gh1.data(), gh1.size(), "gh");
    checkBuffer(h.data(), h.size(), "h");
    checkBuffer(ijkl.data(), ijkl.size(), "ijkl");
    checkBuffer(kl.data(), kl.size(), "kl");
    checkBuffer(abcdefghijkl, buffer->size(), "abcdefghijkl");
    checkBuffer(gh2.data(), gh2.size(), "gh");
    checkBuffer(ghijkl.data(), ghijkl.size(), "ghijkl");
    EXPECT_EQ(gh1.size(), gh2.size());
    checkBufferWithLength(gh1.data(), gh1.size(), gh2.dataAsCharPtr(), gh2.size());
    checkBuffer(l.data(), l.size(), "l");
}

TEST_F(FragmentedSharedBufferTest, getContiguousData)
{
    Vector<uint8_t> s1 = { 'a', 'b', 'c', 'd' };
    Vector<uint8_t> s2 = { 'e', 'f', 'g', 'h' };
    Vector<uint8_t> s3 = { 'i', 'j', 'k', 'l' };

    SharedBufferBuilder builder;
    builder.append(WTFMove(s1));
    builder.append(WTFMove(s2));
    builder.append(WTFMove(s3));
    auto buffer = builder.take();

    auto abcd = buffer->getContiguousData(0, 4);
    auto bcdefghi = buffer->getContiguousData(1, 8);
    auto gh = buffer->getContiguousData(6, 2);
    auto ghij = buffer->getContiguousData(6, 4);
    auto h = buffer->getContiguousData(7, 1);
    auto ijk = buffer->getContiguousData(8, 3);
    auto kl = buffer->getContiguousData(10, 2);
    auto l = buffer->getContiguousData(11, 1);
    checkBuffer(abcd->data(), abcd->size(), "abcd");
    checkBuffer(bcdefghi->data(), bcdefghi->size(), "bcdefghi");
    checkBuffer(gh->data(), gh->size(), "gh");
    checkBuffer(ghij->data(), ghij->size(), "ghij");
    checkBuffer(h->data(), h->size(), "h");
    checkBuffer(ijk->data(), ijk->size(), "ijk");
    checkBuffer(kl->data(), kl->size(), "kl");
    checkBuffer(l->data(), l->size(), "l");
    auto fghijkl = buffer->getContiguousData(5, 20);
    EXPECT_EQ(fghijkl->size(), buffer->size() - 5);
    checkBuffer(fghijkl->data(), fghijkl->size(), "fghijkl");
    auto outBound = buffer->getContiguousData(30, 20);
    EXPECT_EQ(outBound->size(), 0u);
}

TEST_F(FragmentedSharedBufferTest, isEqualTo)
{
    auto makeBuffer = [] (Vector<Vector<uint8_t>>&& contents) {
        SharedBufferBuilder builder;
        for (auto& content : contents)
            builder.append(WTFMove(content));
        return builder.take();
    };
    auto buffer1 = makeBuffer({ { 'a', 'b', 'c', 'd' } });
    EXPECT_EQ(buffer1.get(), buffer1.get());

    SharedBufferBuilder builder(WTFMove(buffer1));
    builder.append(Vector<uint8_t>({ 'a', 'b', 'c', 'd' }));
    EXPECT_EQ(*builder.get(), makeBuffer({ { 'a', 'b', 'c', 'd', 'a', 'b', 'c', 'd' } }).get());

    EXPECT_EQ(makeBuffer({ { 'a' }, { 'b', 'c' }, { 'd' } }).get(), makeBuffer({ { 'a', 'b' }, { 'c', 'd' } }).get());
    EXPECT_NE(makeBuffer({ { 'a', 'b' } }).get(), makeBuffer({ { 'a', 'b', 'c' } }).get());
    EXPECT_NE(makeBuffer({ { 'a', 'b' } }).get(), makeBuffer({ { 'b', 'c' } }).get());
    EXPECT_NE(makeBuffer({ { 'a' }, { 'b' } }).get(), makeBuffer({ { 'a' }, { 'a' } }).get());
}

TEST_F(FragmentedSharedBufferTest, toHexString)
{
    Vector<uint8_t> t1 = {0x11, 0x5, 0x12};
    SharedBufferBuilder builder;
    builder.append(WTFMove(t1));
    auto buffer = builder.take();
    String result = buffer->toHexString();
    EXPECT_EQ(result, "110512"_s);
    builder.reset();
    buffer = builder.take();
    EXPECT_EQ(buffer->toHexString(), ""_s);
}

TEST_F(FragmentedSharedBufferTest, read)
{
    const char* const simpleText = "This is a simple test.";

    auto check = [](FragmentedSharedBuffer& sharedBuffer) {
        Vector<uint8_t> data = sharedBuffer.read(4, 3);
        EXPECT_EQ(data.size(), 3u);
        EXPECT_EQ(StringView(data.data(), 3), " is"_s);

        data = sharedBuffer.read(4, 1000);
        EXPECT_EQ(data.size(), 18u);

        EXPECT_EQ(StringView(data.data(), 18), " is a simple test."_s);
    };
    auto sharedBuffer = SharedBuffer::create(simpleText, strlen(simpleText));
    check(sharedBuffer);

    SharedBufferBuilder builder;
    for (size_t i = 0; i < strlen(simpleText); i++)
        builder.append(&simpleText[i], 1);
    check(builder.take());
    EXPECT_TRUE(builder.isNull() && !builder);
    EXPECT_EQ(builder.size(), 0u);

    for (size_t i = 0; i < strlen(simpleText); i += 2)
        builder.append(&simpleText[i], 2);
    EXPECT_EQ(builder.size(), strlen(simpleText));
    check(builder.take());
}

TEST_F(FragmentedSharedBufferTest, extractData)
{
    const char* const simpleText = "This is a simple test.";
    auto original = SharedBuffer::create(simpleText, strlen(simpleText));
    auto copy = original->copy();
    auto vector = copy->extractData();
    EXPECT_TRUE(copy->isEmpty());
    EXPECT_FALSE(original->isEmpty());
    ASSERT_TRUE(original->hasOneSegment());
    EXPECT_GT(original->begin()->segment->size(), 0u);
}

TEST_F(FragmentedSharedBufferTest, copyIsContiguous)
{
    EXPECT_TRUE(SharedBuffer::create()->copy()->isContiguous());
    EXPECT_FALSE(FragmentedSharedBuffer::create()->copy()->isContiguous());
    const char* const simpleText = "This is a simple test.";
    EXPECT_TRUE(SharedBuffer::create(simpleText, strlen(simpleText))->copy()->isContiguous());
    EXPECT_FALSE(FragmentedSharedBuffer::create(simpleText, strlen(simpleText))->copy()->isContiguous());
}

#if ENABLE(MHTML)
// SharedBufferChunkReader unit-tests -------------------------------------
template< typename T, size_t N >
constexpr size_t arraysize( const T (&)[N] ) { return N; }

static void readAllChunks(std::vector<String>* chunks, FragmentedSharedBuffer& buffer, const String& separator = "\r\n"_s, bool includeSeparator = false)
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
        if (chunks[i] != StringView::fromLatin1(expectedChunks[i]))
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
    auto check = [](FragmentedSharedBuffer& sharedBuffer) {
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

    SharedBufferBuilder builder(std::in_place, data, 256);
    check(builder.take());

    for (size_t i = 0; i < 256; ++i) {
        char c = i;
        builder.append(&c, 1);
    }
    check(builder.take());
}

TEST_F(SharedBufferChunkReaderTest, peekData)
{
    const char* const simpleText = "This is a simple test.";

    auto check = [](FragmentedSharedBuffer& sharedBuffer) {
        SharedBufferChunkReader chunkReader(&sharedBuffer, "is");

        String chunk = chunkReader.nextChunkAsUTF8StringWithLatin1Fallback();
        EXPECT_EQ(chunk, "Th"_s);

        Vector<uint8_t> data;
        size_t read = chunkReader.peek(data, 3);
        EXPECT_EQ(read, 3u);

        EXPECT_EQ(String(data.data(), 3), " is"_s);

        read = chunkReader.peek(data, 1000);
        EXPECT_EQ(read, 18u);

        EXPECT_EQ(String(data.data(), 18), " is a simple test."_s);

        // Ensure the cursor has not changed.
        chunk = chunkReader.nextChunkAsUTF8StringWithLatin1Fallback();
        EXPECT_EQ(chunk, " "_s);

        chunk = chunkReader.nextChunkAsUTF8StringWithLatin1Fallback();
        EXPECT_EQ(chunk, " a simple test."_s);

        chunk = chunkReader.nextChunkAsUTF8StringWithLatin1Fallback();
        EXPECT_TRUE(chunk.isNull());
    };
    auto sharedBuffer = SharedBuffer::create(simpleText, strlen(simpleText));
    check(sharedBuffer);

    SharedBufferBuilder builder(std::in_place, simpleText, strlen(simpleText));
    check(builder.take());

    for (size_t i = 0; i < strlen(simpleText); i++)
        builder.append(&simpleText[i], 1);
    check(builder.take());

    for (size_t i = 0; i < strlen(simpleText); i += 2)
        builder.append(&simpleText[i], 2);
    EXPECT_EQ(builder.size(), strlen(simpleText));
    check(builder.take());
}

TEST_F(SharedBufferChunkReaderTest, readAllChunksInMultiSegment)
{
    const char* const simpleText = "This is the most ridiculous history there is.";
    auto check = [](FragmentedSharedBuffer& sharedBuffer) {
        std::vector<String> chunks;
        const char* const expectedChunks1WithoutSeparator[] = { "Th", "s ", "s the most r", "d", "culous h", "story there ", "s." };
        readAllChunks(&chunks, sharedBuffer, "i"_s);
        EXPECT_TRUE(checkChunks(chunks, expectedChunks1WithoutSeparator, arraysize(expectedChunks1WithoutSeparator)));

        chunks.clear();
        const char* const expectedChunks1WithSeparator[] = { "Thi", "s i", "s the most ri", "di", "culous hi", "story there i", "s." };
        readAllChunks(&chunks, sharedBuffer, "i"_s, true);
        EXPECT_TRUE(checkChunks(chunks, expectedChunks1WithSeparator, arraysize(expectedChunks1WithSeparator)));

        chunks.clear();
        const char* const expectedChunks2WithoutSeparator[] = { "Th", " ", " the most ridiculous h", "tory there ", "." };
        readAllChunks(&chunks, sharedBuffer, "is"_s);
        EXPECT_TRUE(checkChunks(chunks, expectedChunks2WithoutSeparator, arraysize(expectedChunks2WithoutSeparator)));

        chunks.clear();
        const char* const expectedChunks2WithSeparator[] = { "This", " is", " the most ridiculous his", "tory there is", "." };
        readAllChunks(&chunks, sharedBuffer, "is"_s, true);
        EXPECT_TRUE(checkChunks(chunks, expectedChunks2WithSeparator, arraysize(expectedChunks2WithSeparator)));

        chunks.clear();
        const char* const expectedChunks3WithoutSeparator[] = { "This is the most ridiculous h", "ory there is." };
        readAllChunks(&chunks, sharedBuffer, "ist"_s);
        EXPECT_TRUE(checkChunks(chunks, expectedChunks3WithoutSeparator, arraysize(expectedChunks3WithoutSeparator)));

        chunks.clear();
        const char* const expectedChunks3WithSeparator[] = { "This is the most ridiculous hist", "ory there is." };
        readAllChunks(&chunks, sharedBuffer, "ist"_s, true);
        EXPECT_TRUE(checkChunks(chunks, expectedChunks3WithSeparator, arraysize(expectedChunks3WithSeparator)));
    };
    auto sharedBuffer = SharedBuffer::create(simpleText, strlen(simpleText));
    check(sharedBuffer);

    SharedBufferBuilder builder(std::in_place, simpleText, strlen(simpleText));
    check(builder.take());

    for (size_t i = 0; i < strlen(simpleText); i++)
        builder.append(&simpleText[i], 1);
    EXPECT_EQ(builder.size(), strlen(simpleText));
    check(builder.take());

    for (size_t i = 0; i < strlen(simpleText); i += 5)
        builder.append(&simpleText[i], 5);
    EXPECT_EQ(builder.size(), strlen(simpleText));
    check(builder.take());
}

TEST_F(SharedBufferChunkReaderTest, changingIterator)
{
    {
        const char* const simpleText = "This is the most ridiculous history there is.";
        auto sharedBuffer = SharedBuffer::create(simpleText, strlen(simpleText));
        SharedBufferChunkReader chunkReader(sharedBuffer.ptr(), "is");
        String chunk = chunkReader.nextChunkAsUTF8StringWithLatin1Fallback();
        EXPECT_EQ(chunk, "Th"_s);

        chunk = chunkReader.nextChunkAsUTF8StringWithLatin1Fallback();
        EXPECT_EQ(chunk, " "_s);

        chunkReader.setSeparator("he");
        chunk = chunkReader.nextChunkAsUTF8StringWithLatin1Fallback();
        EXPECT_EQ(chunk, " t"_s);

        chunk = chunkReader.nextChunkAsUTF8StringWithLatin1Fallback();
        EXPECT_EQ(chunk, " most ridiculous history t"_s);

        // Set a non existing separator.
        chunkReader.setSeparator("tchinta");
        chunk = chunkReader.nextChunkAsUTF8StringWithLatin1Fallback();
        EXPECT_EQ(chunk, "re is."_s);

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
        readAllChunks(&chunks, sharedBuffer, "dog"_s);
        EXPECT_TRUE(checkChunks(chunks, expectedChunksWithoutSeparator, arraysize(expectedChunksWithoutSeparator)));

        chunks.clear();
        const char* const expectedChunksWithSeparator[] = { "dog" };
        readAllChunks(&chunks, sharedBuffer, "dog"_s, true);
        EXPECT_TRUE(checkChunks(chunks, expectedChunksWithSeparator, arraysize(expectedChunksWithSeparator)));
    }

    // Ends with repeated separators.
    {
        const char* const simpleText = "Beaucoup de chats catcatcatcatcat";
        auto sharedBuffer = SharedBuffer::create(simpleText, strlen(simpleText));
        const char* const expectedChunksWithoutSeparator[] = { "Beaucoup de chats ", "", "", "", "" };
        std::vector<String> chunks;
        readAllChunks(&chunks, sharedBuffer, "cat"_s);
        EXPECT_TRUE(checkChunks(chunks, expectedChunksWithoutSeparator, arraysize(expectedChunksWithoutSeparator)));

        chunks.clear();
        const char* const expectedChunksWithSeparator[] = { "Beaucoup de chats cat", "cat", "cat", "cat", "cat" };
        readAllChunks(&chunks, sharedBuffer, "cat"_s, true);
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
