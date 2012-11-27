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

#include "ImageFrameGenerator.h"

#include "ImageDecodingStore.h"
#include "MockImageDecoder.h"
#include "SharedBuffer.h"
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

class ImageFrameGeneratorTest;

// Helper methods to generate standard sizes.
SkISize fullSize() { return SkISize::Make(100, 100); }
SkISize scaledSize() { return SkISize::Make(50, 50); }

class ImageFrameGeneratorTest;

class MockImageDecoderFactory : public ImageDecoderFactory {
public:
    static PassOwnPtr<MockImageDecoderFactory> create(ImageFrameGeneratorTest* test)
    {
        return adoptPtr(new MockImageDecoderFactory(test));
    }

    virtual PassOwnPtr<ImageDecoder> create();

private:
    MockImageDecoderFactory(ImageFrameGeneratorTest* test)
        : m_test(test)
    {
    }

    ImageFrameGeneratorTest* m_test;
};

class ImageFrameGeneratorTest : public ::testing::Test, public MockImageDecoderClient {
public:
    virtual void SetUp()
    {
        ImageDecodingStore::initializeOnce();
        m_data = SharedBuffer::create();
        m_generator = ImageFrameGenerator::create(fullSize(), m_data, true);
        m_generator->setImageDecoderFactoryForTesting(MockImageDecoderFactory::create(this));
        m_frameBufferRequestCount = 0;
        m_frameStatus = ImageFrame::FrameEmpty;
    }

    virtual void TearDown()
    {
        ImageDecodingStore::shutdown();
    }

    virtual void decoderBeingDestroyed()
    {
    }

    virtual void frameBufferRequested()
    {
        ++m_frameBufferRequestCount;
    }

    virtual ImageFrame::FrameStatus frameStatus()
    {
        return m_frameStatus;
    }

protected:
    PassOwnPtr<ScaledImageFragment> createCompleteImage(const SkISize& size)
    {
        SkBitmap bitmap;
        bitmap.setConfig(SkBitmap::kARGB_8888_Config, size.width(), size.height());
        bitmap.allocPixels();
        return ScaledImageFragment::create(size, bitmap, true);
    }

    void setFrameStatus(ImageFrame::FrameStatus status)  { m_frameStatus = status; }

    RefPtr<SharedBuffer> m_data;
    RefPtr<ImageFrameGenerator> m_generator;
    int m_frameBufferRequestCount;
    ImageFrame::FrameStatus m_frameStatus;
};

PassOwnPtr<ImageDecoder> MockImageDecoderFactory::create()
{
    MockImageDecoder* decoder = new MockImageDecoder(m_test);
    decoder->setSize(fullSize().width(), fullSize().height());
    return adoptPtr(decoder);
}

TEST_F(ImageFrameGeneratorTest, cacheHit)
{
    const ScaledImageFragment* fullImage = ImageDecodingStore::instance()->insertAndLockCache(
        m_generator.get(), createCompleteImage(fullSize()));
    EXPECT_EQ(fullSize(), fullImage->scaledSize());
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), fullImage);

    const ScaledImageFragment* tempImage = m_generator->decodeAndScale(fullSize());
    EXPECT_EQ(fullImage, tempImage);
    EXPECT_EQ(fullSize(), tempImage->scaledSize());
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(0, m_frameBufferRequestCount);
}

TEST_F(ImageFrameGeneratorTest, cacheMissWithScale)
{
    const ScaledImageFragment* fullImage = ImageDecodingStore::instance()->insertAndLockCache(
        m_generator.get(), createCompleteImage(fullSize()));
    EXPECT_EQ(fullSize(), fullImage->scaledSize());
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), fullImage);

    // Cache miss because of scaled size not found.
    const ScaledImageFragment* scaledImage = m_generator->decodeAndScale(scaledSize());
    EXPECT_NE(fullImage, scaledImage);
    EXPECT_EQ(scaledSize(), scaledImage->scaledSize());
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), scaledImage);

    // Cache hit.
    const ScaledImageFragment* tempImage = m_generator->decodeAndScale(scaledSize());
    EXPECT_EQ(scaledImage, tempImage);
    EXPECT_EQ(scaledSize(), tempImage->scaledSize());
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(0, m_frameBufferRequestCount);
}

TEST_F(ImageFrameGeneratorTest, cacheMissWithDecodeAndScale)
{
    setFrameStatus(ImageFrame::FrameComplete);

    // Cache miss.
    const ScaledImageFragment* scaledImage = m_generator->decodeAndScale(scaledSize());
    EXPECT_EQ(1, m_frameBufferRequestCount);
    EXPECT_EQ(scaledSize(), scaledImage->scaledSize());
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), scaledImage);

    // Cache hit.
    const ScaledImageFragment* fullImage = m_generator->decodeAndScale(fullSize());
    EXPECT_NE(scaledImage, fullImage);
    EXPECT_EQ(fullSize(), fullImage->scaledSize());
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), fullImage);

    // Cache hit.
    const ScaledImageFragment* tempImage = m_generator->decodeAndScale(scaledSize());
    EXPECT_EQ(scaledImage, tempImage);
    EXPECT_EQ(scaledSize(), tempImage->scaledSize());
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(1, m_frameBufferRequestCount);
}

// Test that incomplete frames are not cached.
TEST_F(ImageFrameGeneratorTest, cacheMissWithIncompleteDecode)
{
    setFrameStatus(ImageFrame::FramePartial);

    const ScaledImageFragment* tempImage = m_generator->decodeAndScale(scaledSize());
    EXPECT_EQ(1, m_frameBufferRequestCount);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);

    tempImage = m_generator->decodeAndScale(fullSize());
    EXPECT_EQ(2, m_frameBufferRequestCount);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);

    tempImage = m_generator->decodeAndScale(scaledSize());
    EXPECT_EQ(3, m_frameBufferRequestCount);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
}

} // namespace
