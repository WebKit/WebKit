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
#include <wtf/Threading.h>

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
        m_decodersDestroyed = 0;
        m_frameBufferRequestCount = 0;
        m_frameBufferLockCount = 0;
        m_frameBufferUnlockCount = 0;
        m_frameStatus = ImageFrame::FrameEmpty;
    }

    virtual void TearDown()
    {
        ImageDecodingStore::shutdown();
    }

    virtual void decoderBeingDestroyed()
    {
        ++m_decodersDestroyed;
    }

    virtual void frameBufferRequested()
    {
        ++m_frameBufferRequestCount;
    }

    virtual void frameBuffersUnlocked()
    {
        ++m_frameBufferUnlockCount;
    }

    virtual void frameBuffersLocked()
    {
        ++m_frameBufferLockCount;
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

    void addNewData()
    {
        m_data->append("g", 1);
        m_generator->setData(m_data, false);
    }

    void setFrameStatus(ImageFrame::FrameStatus status)  { m_frameStatus = status; }

    RefPtr<SharedBuffer> m_data;
    RefPtr<ImageFrameGenerator> m_generator;
    int m_decodersDestroyed;
    int m_frameBufferRequestCount;
    int m_frameBufferLockCount;
    int m_frameBufferUnlockCount;
    ImageFrame::FrameStatus m_frameStatus;
};

PassOwnPtr<ImageDecoder> MockImageDecoderFactory::create()
{
    OwnPtr<MockImageDecoder> decoder = MockImageDecoder::create(m_test);
    decoder->setSize(fullSize().width(), fullSize().height());
    decoder->setFrameHasAlpha(false);
    return decoder.release();
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
    EXPECT_TRUE(m_generator->hasAlpha());
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(0, m_frameBufferRequestCount);
    EXPECT_EQ(0, m_frameBufferLockCount);
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
    EXPECT_TRUE(m_generator->hasAlpha());
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), scaledImage);

    // Cache hit.
    const ScaledImageFragment* tempImage = m_generator->decodeAndScale(scaledSize());
    EXPECT_EQ(scaledImage, tempImage);
    EXPECT_EQ(scaledSize(), tempImage->scaledSize());
    EXPECT_TRUE(m_generator->hasAlpha());
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(0, m_frameBufferRequestCount);
    EXPECT_EQ(0, m_frameBufferLockCount);
    EXPECT_EQ(0, m_frameBufferUnlockCount);
}

TEST_F(ImageFrameGeneratorTest, cacheMissWithDecodeAndScale)
{
    setFrameStatus(ImageFrame::FrameComplete);

    // Cache miss.
    const ScaledImageFragment* scaledImage = m_generator->decodeAndScale(scaledSize());
    EXPECT_EQ(1, m_frameBufferRequestCount);
    EXPECT_EQ(0, m_frameBufferLockCount);
    EXPECT_EQ(0, m_frameBufferUnlockCount);
    EXPECT_EQ(scaledSize(), scaledImage->scaledSize());
    EXPECT_FALSE(m_generator->hasAlpha());
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), scaledImage);
    EXPECT_EQ(1, m_decodersDestroyed);

    // Cache hit.
    const ScaledImageFragment* fullImage = m_generator->decodeAndScale(fullSize());
    EXPECT_NE(scaledImage, fullImage);
    EXPECT_EQ(fullSize(), fullImage->scaledSize());
    EXPECT_FALSE(m_generator->hasAlpha());
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), fullImage);

    // Cache hit.
    const ScaledImageFragment* tempImage = m_generator->decodeAndScale(scaledSize());
    EXPECT_EQ(scaledImage, tempImage);
    EXPECT_EQ(scaledSize(), tempImage->scaledSize());
    EXPECT_FALSE(m_generator->hasAlpha());
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(1, m_frameBufferRequestCount);
    EXPECT_EQ(0, m_frameBufferLockCount);
    EXPECT_EQ(0, m_frameBufferUnlockCount);
}

TEST_F(ImageFrameGeneratorTest, cacheMissWithIncompleteDecode)
{
    setFrameStatus(ImageFrame::FramePartial);

    const ScaledImageFragment* tempImage= m_generator->decodeAndScale(fullSize());
    EXPECT_FALSE(tempImage->isComplete());
    EXPECT_EQ(1, m_frameBufferRequestCount);
    EXPECT_EQ(0, m_frameBufferLockCount);
    EXPECT_EQ(1, m_frameBufferUnlockCount);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(1u, ImageDecodingStore::instance()->cacheEntries());

    addNewData();
    tempImage = m_generator->decodeAndScale(fullSize());
    EXPECT_FALSE(tempImage->isComplete());
    EXPECT_EQ(2, m_frameBufferRequestCount);
    EXPECT_EQ(1, m_frameBufferLockCount);
    EXPECT_EQ(2, m_frameBufferUnlockCount);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(1u, ImageDecodingStore::instance()->cacheEntries());
    EXPECT_EQ(0, m_decodersDestroyed);
}

TEST_F(ImageFrameGeneratorTest, cacheMissWithIncompleteDecodeNoNewData)
{
    setFrameStatus(ImageFrame::FramePartial);

    const ScaledImageFragment* tempImage= m_generator->decodeAndScale(fullSize());
    EXPECT_FALSE(tempImage->isComplete());
    EXPECT_EQ(1, m_frameBufferRequestCount);
    EXPECT_EQ(0, m_frameBufferLockCount);
    EXPECT_EQ(1, m_frameBufferUnlockCount);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(1u, ImageDecodingStore::instance()->cacheEntries());

    tempImage = m_generator->decodeAndScale(fullSize());
    EXPECT_FALSE(tempImage->isComplete());
    EXPECT_EQ(1, m_frameBufferRequestCount);
    EXPECT_EQ(0, m_frameBufferLockCount);
    EXPECT_EQ(1, m_frameBufferUnlockCount);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(1u, ImageDecodingStore::instance()->cacheEntries());
    EXPECT_EQ(0, m_decodersDestroyed);
}

TEST_F(ImageFrameGeneratorTest, cacheMissWithIncompleteDecodeAndScale)
{
    setFrameStatus(ImageFrame::FramePartial);

    const ScaledImageFragment* tempImage= m_generator->decodeAndScale(scaledSize());
    EXPECT_FALSE(tempImage->isComplete());
    EXPECT_EQ(1, m_frameBufferRequestCount);
    EXPECT_EQ(0, m_frameBufferLockCount);
    EXPECT_EQ(1, m_frameBufferUnlockCount);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(2u, ImageDecodingStore::instance()->cacheEntries());

    addNewData();
    tempImage = m_generator->decodeAndScale(scaledSize());
    EXPECT_FALSE(tempImage->isComplete());
    EXPECT_EQ(2, m_frameBufferRequestCount);
    EXPECT_EQ(1, m_frameBufferLockCount);
    EXPECT_EQ(2, m_frameBufferUnlockCount);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(2u, ImageDecodingStore::instance()->cacheEntries());
    EXPECT_EQ(0, m_decodersDestroyed);
}

TEST_F(ImageFrameGeneratorTest, incompleteDecodeBecomesComplete)
{
    setFrameStatus(ImageFrame::FramePartial);

    const ScaledImageFragment* tempImage = m_generator->decodeAndScale(fullSize());
    EXPECT_FALSE(tempImage->isComplete());
    EXPECT_EQ(1, m_frameBufferRequestCount);
    EXPECT_EQ(0, m_decodersDestroyed);
    EXPECT_EQ(0, m_frameBufferLockCount);
    EXPECT_EQ(1, m_frameBufferUnlockCount);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(1u, ImageDecodingStore::instance()->cacheEntries());

    setFrameStatus(ImageFrame::FrameComplete);
    addNewData();

    tempImage = m_generator->decodeAndScale(fullSize());
    EXPECT_TRUE(tempImage->isComplete());
    EXPECT_EQ(2, m_frameBufferRequestCount);
    EXPECT_EQ(1, m_decodersDestroyed);
    EXPECT_EQ(1, m_frameBufferLockCount);
    EXPECT_EQ(1, m_frameBufferUnlockCount);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(1u, ImageDecodingStore::instance()->cacheEntries());

    tempImage = m_generator->decodeAndScale(fullSize());
    EXPECT_TRUE(tempImage->isComplete());
    EXPECT_EQ(2, m_frameBufferRequestCount);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
}

TEST_F(ImageFrameGeneratorTest, incompleteDecodeAndScaleBecomesComplete)
{
    setFrameStatus(ImageFrame::FramePartial);

    const ScaledImageFragment* tempImage = m_generator->decodeAndScale(scaledSize());
    EXPECT_FALSE(tempImage->isComplete());
    EXPECT_EQ(1, m_frameBufferRequestCount);
    EXPECT_EQ(0, m_decodersDestroyed);
    EXPECT_EQ(0, m_frameBufferLockCount);
    EXPECT_EQ(1, m_frameBufferUnlockCount);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(2u, ImageDecodingStore::instance()->cacheEntries());

    setFrameStatus(ImageFrame::FrameComplete);
    addNewData();

    tempImage = m_generator->decodeAndScale(scaledSize());
    EXPECT_TRUE(tempImage->isComplete());
    EXPECT_EQ(2, m_frameBufferRequestCount);
    EXPECT_EQ(1, m_decodersDestroyed);
    EXPECT_EQ(1, m_frameBufferLockCount);
    EXPECT_EQ(1, m_frameBufferUnlockCount);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(2u, ImageDecodingStore::instance()->cacheEntries());

    tempImage = m_generator->decodeAndScale(scaledSize());
    EXPECT_TRUE(tempImage->isComplete());
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);

    tempImage = m_generator->decodeAndScale(fullSize());
    EXPECT_TRUE(tempImage->isComplete());
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);

    EXPECT_EQ(2, m_frameBufferRequestCount);
}

static void decodeThreadMain(void* arg)
{
    ImageFrameGenerator* generator = reinterpret_cast<ImageFrameGenerator*>(arg);
    const ScaledImageFragment* tempImage = generator->decodeAndScale(fullSize());
    ImageDecodingStore::instance()->unlockCache(generator, tempImage);
}

TEST_F(ImageFrameGeneratorTest, incompleteDecodeBecomesCompleteMultiThreaded)
{
    WTF::initializeThreading();
    setFrameStatus(ImageFrame::FramePartial);

    const ScaledImageFragment* tempImage = m_generator->decodeAndScale(fullSize());
    EXPECT_FALSE(tempImage->isComplete());
    EXPECT_EQ(1, m_frameBufferRequestCount);
    EXPECT_EQ(0, m_decodersDestroyed);
    EXPECT_EQ(0, m_frameBufferLockCount);
    EXPECT_EQ(1, m_frameBufferUnlockCount);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(1u, ImageDecodingStore::instance()->cacheEntries());

    // Frame can now be decoded completely.
    setFrameStatus(ImageFrame::FrameComplete);
    addNewData();
    ThreadIdentifier threadID = createThread(&decodeThreadMain, m_generator.get(), "DecodeThread");
    waitForThreadCompletion(threadID);

    EXPECT_EQ(2, m_frameBufferRequestCount);
    EXPECT_EQ(1, m_decodersDestroyed);
    EXPECT_EQ(1, m_frameBufferLockCount);
    EXPECT_EQ(1, m_frameBufferUnlockCount);
    EXPECT_EQ(1u, ImageDecodingStore::instance()->cacheEntries());

    tempImage = m_generator->decodeAndScale(fullSize());
    EXPECT_TRUE(tempImage->isComplete());
    EXPECT_EQ(2, m_frameBufferRequestCount);
    EXPECT_EQ(1, m_frameBufferLockCount);
    EXPECT_EQ(1, m_frameBufferUnlockCount);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
}

} // namespace
