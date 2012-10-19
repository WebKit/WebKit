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
#include "SkCanvas.h"
#include "SkDevice.h"
#include "SkPicture.h"
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

class DeferredImageDecoderTest : public ::testing::Test {
public:
    virtual void SetUp()
    {
        ImageDecodingStore::initializeOnMainThread();
        m_actualDecoder = new MockImageDecoder();
        m_actualDecoder->setSize(600, 613);
        m_lazyDecoder = DeferredImageDecoder::createForTesting(adoptPtr(m_actualDecoder));
        m_canvas.setDevice(new SkDevice(SkBitmap::kARGB_8888_Config, 100, 100))->unref();
    }

    virtual void TearDown()
    {
        ImageDecodingStore::shutdown();
    }

protected:
    // Don't own this but saves the pointer to query states.
    MockImageDecoder* m_actualDecoder;
    OwnPtr<DeferredImageDecoder> m_lazyDecoder;
    SkPicture m_picture;
    SkCanvas m_canvas;
};

TEST_F(DeferredImageDecoderTest, drawIntoSkPicture)
{
    OwnPtr<NativeImageSkia> image(adoptPtr(m_lazyDecoder->frameBufferAtIndex(0)->asNewNativeImage()));
    EXPECT_EQ(m_actualDecoder->size().width(), image->bitmap().width());
    EXPECT_EQ(m_actualDecoder->size().height(), image->bitmap().height());
    EXPECT_FALSE(image->bitmap().isNull());
    EXPECT_TRUE(image->bitmap().isImmutable());

    SkCanvas* tempCanvas = m_picture.beginRecording(100, 100);
    tempCanvas->drawBitmap(image->bitmap(), 0, 0);
    m_picture.endRecording();
    EXPECT_EQ(0, m_actualDecoder->frameBufferRequestCount());

    m_canvas.drawPicture(m_picture);
    EXPECT_EQ(1, m_actualDecoder->frameBufferRequestCount());
}

TEST_F(DeferredImageDecoderTest, drawScaledIntoSkPicture)
{
    OwnPtr<NativeImageSkia> image(adoptPtr(m_lazyDecoder->frameBufferAtIndex(0)->asNewNativeImage()));
    SkBitmap scaledBitmap = image->resizedBitmap(SkISize::Make(50, 51), SkIRect::MakeWH(50, 51));
    EXPECT_FALSE(scaledBitmap.isNull());
    EXPECT_TRUE(scaledBitmap.isImmutable());
    EXPECT_EQ(50, scaledBitmap.width());
    EXPECT_EQ(51, scaledBitmap.height());
    EXPECT_EQ(0, m_actualDecoder->frameBufferRequestCount());

    SkCanvas* tempCanvas = m_picture.beginRecording(100, 100);
    tempCanvas->drawBitmap(scaledBitmap, 0, 0);
    m_picture.endRecording();
    EXPECT_EQ(0, m_actualDecoder->frameBufferRequestCount());

    m_canvas.drawPicture(m_picture);
    EXPECT_EQ(1, m_actualDecoder->frameBufferRequestCount());
}

} // namespace
