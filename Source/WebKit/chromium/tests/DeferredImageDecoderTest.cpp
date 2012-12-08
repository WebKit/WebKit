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

#include "DeferredImageDecoder.h"

#include "ImageDecodingStore.h"
#include "MockImageDecoder.h"
#include "NativeImageSkia.h"
#include "SharedBuffer.h"
#include "SkCanvas.h"
#include "SkDevice.h"
#include "SkPicture.h"
#include <gtest/gtest.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Threading.h>

using namespace WebCore;

namespace {

// Raw data for a PNG file with 1x1 white pixels.
const unsigned char whitePNG[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00,
    0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01, 0x08, 0x02, 0x00, 0x00, 0x00, 0x90,
    0x77, 0x53, 0xde, 0x00, 0x00, 0x00, 0x01, 0x73, 0x52, 0x47,
    0x42, 0x00, 0xae, 0xce, 0x1c, 0xe9, 0x00, 0x00, 0x00, 0x09,
    0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0b, 0x13, 0x00, 0x00,
    0x0b, 0x13, 0x01, 0x00, 0x9a, 0x9c, 0x18, 0x00, 0x00, 0x00,
    0x0c, 0x49, 0x44, 0x41, 0x54, 0x08, 0xd7, 0x63, 0xf8, 0xff,
    0xff, 0x3f, 0x00, 0x05, 0xfe, 0x02, 0xfe, 0xdc, 0xcc, 0x59,
    0xe7, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae,
    0x42, 0x60, 0x82,
};

static SkCanvas* createRasterCanvas(int width, int height)
{
    SkAutoTUnref<SkDevice> device(new SkDevice(SkBitmap::kARGB_8888_Config, width, height));
    return new SkCanvas(device);
}

struct Rasterizer {
    SkCanvas* canvas;
    SkPicture* picture;
};

class DeferredImageDecoderTest : public ::testing::Test, public MockImageDecoderClient {
public:
    virtual void SetUp()
    {
        ImageDecodingStore::initializeOnce();
        DeferredImageDecoder::setEnabled(true);
        m_data = SharedBuffer::create(whitePNG, sizeof(whitePNG));
        OwnPtr<MockImageDecoder> decoder = MockImageDecoder::create(this);
        m_actualDecoder = decoder.get();
        m_actualDecoder->setSize(1, 1);
        m_lazyDecoder = DeferredImageDecoder::createForTesting(decoder.release());
        m_lazyDecoder->setData(m_data.get(), true);
        m_canvas.reset(createRasterCanvas(100, 100));
        m_frameBufferRequestCount = 0;
    }

    virtual void TearDown()
    {
        ImageDecodingStore::shutdown();
    }

    virtual void decoderBeingDestroyed()
    {
        m_actualDecoder = 0;
    }

    virtual void frameBufferRequested()
    {
        ++m_frameBufferRequestCount;
    }

    virtual ImageFrame::FrameStatus frameStatus()
    {
        return ImageFrame::FrameComplete;
    }

protected:
    // Don't own this but saves the pointer to query states.
    MockImageDecoder* m_actualDecoder;
    OwnPtr<DeferredImageDecoder> m_lazyDecoder;
    SkPicture m_picture;
    SkAutoTUnref<SkCanvas> m_canvas;
    int m_frameBufferRequestCount;
    RefPtr<SharedBuffer> m_data;
};

TEST_F(DeferredImageDecoderTest, drawIntoSkPicture)
{
    OwnPtr<NativeImageSkia> image(adoptPtr(m_lazyDecoder->frameBufferAtIndex(0)->asNewNativeImage()));
    EXPECT_EQ(1, image->bitmap().width());
    EXPECT_EQ(1, image->bitmap().height());
    EXPECT_FALSE(image->bitmap().isNull());
    EXPECT_TRUE(image->bitmap().isImmutable());

    SkCanvas* tempCanvas = m_picture.beginRecording(100, 100);
    tempCanvas->drawBitmap(image->bitmap(), 0, 0);
    m_picture.endRecording();
    EXPECT_EQ(0, m_frameBufferRequestCount);

    m_canvas->drawPicture(m_picture);
    EXPECT_EQ(0, m_frameBufferRequestCount);

    SkBitmap canvasBitmap;
    canvasBitmap.setConfig(SkBitmap::kARGB_8888_Config, 100, 100);
    ASSERT_TRUE(m_canvas->readPixels(&canvasBitmap, 0, 0));
    SkAutoLockPixels autoLock(canvasBitmap);
    EXPECT_EQ(SkColorSetARGB(255, 255, 255, 255), canvasBitmap.getColor(0, 0));
}

TEST_F(DeferredImageDecoderTest, drawScaledIntoSkPicture)
{
    OwnPtr<NativeImageSkia> image(adoptPtr(m_lazyDecoder->frameBufferAtIndex(0)->asNewNativeImage()));
    SkBitmap scaledBitmap = image->resizedBitmap(SkISize::Make(50, 51), SkIRect::MakeWH(50, 51));
    EXPECT_FALSE(scaledBitmap.isNull());
    EXPECT_TRUE(scaledBitmap.isImmutable());
    EXPECT_EQ(50, scaledBitmap.width());
    EXPECT_EQ(51, scaledBitmap.height());
    EXPECT_EQ(0, m_frameBufferRequestCount);

    SkCanvas* tempCanvas = m_picture.beginRecording(100, 100);
    tempCanvas->drawBitmap(scaledBitmap, 0, 0);
    m_picture.endRecording();
    EXPECT_EQ(0, m_frameBufferRequestCount);

    m_canvas->drawPicture(m_picture);
    EXPECT_EQ(0, m_frameBufferRequestCount);

    SkBitmap canvasBitmap;
    canvasBitmap.setConfig(SkBitmap::kARGB_8888_Config, 100, 100);
    ASSERT_TRUE(m_canvas->readPixels(&canvasBitmap, 0, 0));
    SkAutoLockPixels autoLock(canvasBitmap);
    EXPECT_EQ(SkColorSetARGB(255, 255, 255, 255), canvasBitmap.getColor(0, 0));
    EXPECT_EQ(SkColorSetARGB(255, 255, 255, 255), canvasBitmap.getColor(49, 50));
}

static void rasterizeMain(void* arg)
{
    Rasterizer* rasterizer = static_cast<Rasterizer*>(arg);
    rasterizer->canvas->drawPicture(*rasterizer->picture);
}

TEST_F(DeferredImageDecoderTest, decodeOnOtherThread)
{
    WTF::initializeThreading();

    OwnPtr<NativeImageSkia> image(adoptPtr(m_lazyDecoder->frameBufferAtIndex(0)->asNewNativeImage()));
    EXPECT_EQ(1, image->bitmap().width());
    EXPECT_EQ(1, image->bitmap().height());
    EXPECT_FALSE(image->bitmap().isNull());
    EXPECT_TRUE(image->bitmap().isImmutable());

    SkCanvas* tempCanvas = m_picture.beginRecording(100, 100);
    tempCanvas->drawBitmap(image->bitmap(), 0, 0);
    m_picture.endRecording();
    EXPECT_EQ(0, m_frameBufferRequestCount);

    // Create a thread to rasterize SkPicture.
    Rasterizer rasterizer;
    rasterizer.canvas = m_canvas;
    rasterizer.picture = &m_picture;
    ThreadIdentifier threadID = createThread(&rasterizeMain, &rasterizer, "RasterThread");
    waitForThreadCompletion(threadID);
    EXPECT_EQ(0, m_frameBufferRequestCount);

    SkBitmap canvasBitmap;
    canvasBitmap.setConfig(SkBitmap::kARGB_8888_Config, 100, 100);
    ASSERT_TRUE(m_canvas->readPixels(&canvasBitmap, 0, 0));
    SkAutoLockPixels autoLock(canvasBitmap);
    EXPECT_EQ(SkColorSetARGB(255, 255, 255, 255), canvasBitmap.getColor(0, 0));
}

} // namespace
