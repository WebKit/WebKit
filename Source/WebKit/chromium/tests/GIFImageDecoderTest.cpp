/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "GIFImageDecoder.h"

#include "FileSystem.h"
#include "SharedBuffer.h"
#include <gtest/gtest.h>
#include <public/Platform.h>
#include <public/WebData.h>
#include <public/WebSize.h>
#include <public/WebUnitTestSupport.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/StringHasher.h>
#include <wtf/Vector.h>

using namespace WebCore;
using namespace WebKit;

namespace {

#if !OS(ANDROID)

static PassRefPtr<SharedBuffer> readFile(const char* fileName)
{
    String filePath = Platform::current()->unitTestSupport()->webKitRootDir();
    filePath.append(fileName);

    long long fileSize;
    if (!getFileSize(filePath, fileSize))
        return 0;

    PlatformFileHandle handle = openFile(filePath, OpenForRead);
    int fileLength = static_cast<int>(fileSize);
    Vector<char> buffer(fileLength);
    readFromFile(handle, buffer.data(), fileLength);
    closeFile(handle);
    return SharedBuffer::adoptVector(buffer);
}

static PassOwnPtr<GIFImageDecoder> createDecoder()
{
    return adoptPtr(new GIFImageDecoder(ImageSource::AlphaNotPremultiplied, ImageSource::GammaAndColorProfileApplied));
}

static unsigned hashSkBitmap(const SkBitmap& bitmap)
{
    return StringHasher::hashMemory(bitmap.getPixels(), bitmap.getSize());
}

TEST(GIFImageDecoderTest, decodeTwoFrames)
{
    OwnPtr<GIFImageDecoder> decoder(createDecoder());

    RefPtr<SharedBuffer> data = readFile("/LayoutTests/fast/images/resources/animated.gif");
    ASSERT_TRUE(data.get());
    decoder->setData(data.get(), true);
    EXPECT_EQ(cAnimationLoopOnce, decoder->repetitionCount());

    ImageFrame* frame = decoder->frameBufferAtIndex(0);
    EXPECT_EQ(ImageFrame::FrameComplete, frame->status());
    EXPECT_EQ(16, frame->getSkBitmap().width());
    EXPECT_EQ(16, frame->getSkBitmap().height());

    frame = decoder->frameBufferAtIndex(1);
    EXPECT_EQ(ImageFrame::FrameComplete, frame->status());
    EXPECT_EQ(16, frame->getSkBitmap().width());
    EXPECT_EQ(16, frame->getSkBitmap().height());

    EXPECT_EQ(2u, decoder->frameCount());
    EXPECT_EQ(cAnimationLoopInfinite, decoder->repetitionCount());
}

TEST(GIFImageDecoderTest, parseAndDecode)
{
    OwnPtr<GIFImageDecoder> decoder(createDecoder());

    RefPtr<SharedBuffer> data = readFile("/LayoutTests/fast/images/resources/animated.gif");
    ASSERT_TRUE(data.get());
    decoder->setData(data.get(), true);
    EXPECT_EQ(cAnimationLoopOnce, decoder->repetitionCount());

    // This call will parse the entire file.
    EXPECT_EQ(2u, decoder->frameCount());

    ImageFrame* frame = decoder->frameBufferAtIndex(0);
    EXPECT_EQ(ImageFrame::FrameComplete, frame->status());
    EXPECT_EQ(16, frame->getSkBitmap().width());
    EXPECT_EQ(16, frame->getSkBitmap().height());

    frame = decoder->frameBufferAtIndex(1);
    EXPECT_EQ(ImageFrame::FrameComplete, frame->status());
    EXPECT_EQ(16, frame->getSkBitmap().width());
    EXPECT_EQ(16, frame->getSkBitmap().height());
    EXPECT_EQ(cAnimationLoopInfinite, decoder->repetitionCount());
}

TEST(GIFImageDecoderTest, parseByteByByte)
{
    OwnPtr<GIFImageDecoder> decoder(createDecoder());

    RefPtr<SharedBuffer> data = readFile("/LayoutTests/fast/images/resources/animated.gif");
    ASSERT_TRUE(data.get());

    size_t frameCount = 0;

    // Pass data to decoder byte by byte.
    for (unsigned length = 1; length <= data->size(); ++length) {
        RefPtr<SharedBuffer> tempData = SharedBuffer::create(data->data(), length);
        decoder->setData(tempData.get(), length == data->size());

        EXPECT_LE(frameCount, decoder->frameCount());
        frameCount = decoder->frameCount();
    }

    EXPECT_EQ(2u, decoder->frameCount());

    decoder->frameBufferAtIndex(0);
    decoder->frameBufferAtIndex(1);
    EXPECT_EQ(cAnimationLoopInfinite, decoder->repetitionCount());
}

TEST(GIFImageDecoderTest, parseAndDecodeByteByByte)
{
    OwnPtr<GIFImageDecoder> decoder(createDecoder());

    RefPtr<SharedBuffer> data = readFile("/LayoutTests/fast/images/resources/animated-gif-with-offsets.gif");
    ASSERT_TRUE(data.get());

    size_t frameCount = 0;
    size_t framesDecoded = 0;

    // Pass data to decoder byte by byte.
    for (unsigned length = 1; length <= data->size(); ++length) {
        RefPtr<SharedBuffer> tempData = SharedBuffer::create(data->data(), length);
        decoder->setData(tempData.get(), length == data->size());

        EXPECT_LE(frameCount, decoder->frameCount());
        frameCount = decoder->frameCount();

        ImageFrame* frame = decoder->frameBufferAtIndex(frameCount - 1);
        if (frame && frame->status() == ImageFrame::FrameComplete && framesDecoded < frameCount)
            ++framesDecoded;
    }

    EXPECT_EQ(5u, decoder->frameCount());
    EXPECT_EQ(5u, framesDecoded);
    EXPECT_EQ(cAnimationLoopInfinite, decoder->repetitionCount());
}

TEST(GIFImageDecoderTest, brokenSecondFrame)
{
    OwnPtr<GIFImageDecoder> decoder(createDecoder());

    RefPtr<SharedBuffer> data = readFile("/Source/WebKit/chromium/tests/data/broken.gif");
    ASSERT_TRUE(data.get());
    decoder->setData(data.get(), true);

    EXPECT_EQ(0u, decoder->frameCount());
    ImageFrame* frame = decoder->frameBufferAtIndex(0);
    EXPECT_FALSE(frame);
    EXPECT_EQ(cAnimationLoopOnce, decoder->repetitionCount());
}

TEST(GIFImageDecoderTest, progressiveDecode)
{
    RefPtr<SharedBuffer> fullData = readFile("/Source/WebKit/chromium/tests/data/radient.gif");
    ASSERT_TRUE(fullData.get());
    const size_t fullLength = fullData->size();

    OwnPtr<GIFImageDecoder> decoder;
    ImageFrame* frame;

    Vector<unsigned> truncatedHashes;
    Vector<unsigned> progressiveHashes;

    // Compute hashes when the file is truncated.
    const size_t increment = 1;
    for (size_t i = 1; i <= fullLength; i += increment) {
        decoder = createDecoder();
        RefPtr<SharedBuffer> data = SharedBuffer::create(fullData->data(), i);
        decoder->setData(data.get(), i == fullLength);
        frame = decoder->frameBufferAtIndex(0);
        if (!frame) {
            truncatedHashes.append(0);
            continue;
        }
        truncatedHashes.append(hashSkBitmap(frame->getSkBitmap()));
    }

    // Compute hashes when the file is progressively decoded.
    decoder = createDecoder();
    for (size_t i = 1; i <= fullLength; i += increment) {
        RefPtr<SharedBuffer> data = SharedBuffer::create(fullData->data(), i);
        decoder->setData(data.get(), i == fullLength);
        frame = decoder->frameBufferAtIndex(0);
        if (!frame) {
            progressiveHashes.append(0);
            continue;
        }
        progressiveHashes.append(hashSkBitmap(frame->getSkBitmap()));
    }

    bool match = true;
    for (size_t i = 0; i < truncatedHashes.size(); ++i) {
        if (truncatedHashes[i] != progressiveHashes[i]) {
            match = false;
            break;
        }
    }
    EXPECT_TRUE(match);
}

#endif

} // namespace
