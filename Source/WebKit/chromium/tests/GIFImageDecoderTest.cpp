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

TEST(GIFImageDecoderTest, decodeTwoFrames)
{
    OwnPtr<GIFImageDecoder> decoder(adoptPtr(new GIFImageDecoder(ImageSource::AlphaNotPremultiplied, ImageSource::GammaAndColorProfileApplied)));

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
    OwnPtr<GIFImageDecoder> decoder(adoptPtr(new GIFImageDecoder(ImageSource::AlphaNotPremultiplied, ImageSource::GammaAndColorProfileApplied)));

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
    OwnPtr<GIFImageDecoder> decoder(adoptPtr(new GIFImageDecoder(ImageSource::AlphaNotPremultiplied, ImageSource::GammaAndColorProfileApplied)));

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
    OwnPtr<GIFImageDecoder> decoder(adoptPtr(new GIFImageDecoder(ImageSource::AlphaNotPremultiplied, ImageSource::GammaAndColorProfileApplied)));

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

// Second frame in the file is broken but test that first frame can be decoded.
TEST(GIFImageDecoderTest, brokenSecondFrame)
{
    OwnPtr<GIFImageDecoder> decoder(adoptPtr(new GIFImageDecoder(ImageSource::AlphaNotPremultiplied, ImageSource::GammaAndColorProfileApplied)));

    RefPtr<SharedBuffer> data = readFile("/Source/WebKit/chromium/tests/data/broken.gif");
    ASSERT_TRUE(data.get());
    decoder->setData(data.get(), true);

    EXPECT_EQ(1u, decoder->frameCount());

    ImageFrame* frame = decoder->frameBufferAtIndex(0);
    EXPECT_EQ(ImageFrame::FrameComplete, frame->status());
    EXPECT_EQ(16, frame->getSkBitmap().width());
    EXPECT_EQ(16, frame->getSkBitmap().height());

    frame = decoder->frameBufferAtIndex(1);
    EXPECT_FALSE(frame);
    EXPECT_EQ(cAnimationLoopOnce, decoder->repetitionCount());
}

#endif

} // namespace
