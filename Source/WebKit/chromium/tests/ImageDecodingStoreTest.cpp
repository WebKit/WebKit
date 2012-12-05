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

#include "ImageDecodingStore.h"

#include "ImageFrameGenerator.h"
#include "SharedBuffer.h"
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

class ImageDecodingStoreTest : public ::testing::Test {
public:
    virtual void SetUp()
    {
        ImageDecodingStore::initializeOnce();
        m_data = SharedBuffer::create();
        m_generator = ImageFrameGenerator::create(SkISize::Make(100, 100), m_data, true);
    }

    virtual void TearDown()
    {
        ImageDecodingStore::shutdown();
    }

protected:
    PassOwnPtr<ScaledImageFragment> createCompleteImage(const SkISize& size)
    {
        SkBitmap bitmap;
        bitmap.setConfig(SkBitmap::kARGB_8888_Config, size.width(), size.height());
        bitmap.allocPixels();
        return ScaledImageFragment::create(size, bitmap, true);
    }

    void insertCache(const SkISize& size)
    {
        const ScaledImageFragment* image = ImageDecodingStore::instance()->insertAndLockCache(
            m_generator.get(), createCompleteImage(size));
        unlockCache(image);
    }

    const ScaledImageFragment* lockCache(const SkISize& size)
    {
        return ImageDecodingStore::instance()->lockCompleteCache(m_generator.get(), size);
    }

    void unlockCache(const ScaledImageFragment* cachedImage)
    {
        ImageDecodingStore::instance()->unlockCache(m_generator.get(), cachedImage);
    }

    void evictOneCache()
    {
        size_t memoryUsageInBytes = ImageDecodingStore::instance()->memoryUsageInBytes();
        if (memoryUsageInBytes)
            ImageDecodingStore::instance()->setCacheLimitInBytes(memoryUsageInBytes - 1);
        else
            ImageDecodingStore::instance()->setCacheLimitInBytes(0);
    }

    bool isCacheAlive(const SkISize& size)
    {
        const ScaledImageFragment* cachedImage = lockCache(size);
        if (!cachedImage)
            return false;
        ImageDecodingStore::instance()->unlockCache(m_generator.get(), cachedImage);
        return true;
    }

    RefPtr<SharedBuffer> m_data;
    RefPtr<ImageFrameGenerator> m_generator;
};

TEST_F(ImageDecodingStoreTest, evictOneCache)
{
    insertCache(SkISize::Make(1, 1));
    insertCache(SkISize::Make(2, 2));
    insertCache(SkISize::Make(3, 3));
    EXPECT_EQ(3u, ImageDecodingStore::instance()->cacheEntries());

    evictOneCache();
    EXPECT_EQ(2u, ImageDecodingStore::instance()->cacheEntries());

    evictOneCache();
    EXPECT_EQ(1u, ImageDecodingStore::instance()->cacheEntries());
}

TEST_F(ImageDecodingStoreTest, pruneOrderIsLeastRecentlyUsed)
{
    insertCache(SkISize::Make(1, 1));
    insertCache(SkISize::Make(2, 2));
    insertCache(SkISize::Make(3, 3));
    insertCache(SkISize::Make(4, 4));
    insertCache(SkISize::Make(5, 5));
    EXPECT_EQ(5u, ImageDecodingStore::instance()->cacheEntries());

    // Use cache in the order 3, 2, 4, 1, 5.
    EXPECT_TRUE(isCacheAlive(SkISize::Make(3, 3)));
    EXPECT_TRUE(isCacheAlive(SkISize::Make(2, 2)));
    EXPECT_TRUE(isCacheAlive(SkISize::Make(4, 4)));
    EXPECT_TRUE(isCacheAlive(SkISize::Make(1, 1)));
    EXPECT_TRUE(isCacheAlive(SkISize::Make(5, 5)));

    // Evict 3.
    evictOneCache();
    EXPECT_FALSE(isCacheAlive(SkISize::Make(3, 3)));
    EXPECT_EQ(4u, ImageDecodingStore::instance()->cacheEntries());

    // Evict 2.
    evictOneCache();
    EXPECT_FALSE(isCacheAlive(SkISize::Make(2, 2)));
    EXPECT_EQ(3u, ImageDecodingStore::instance()->cacheEntries());

    // Evict 4.
    evictOneCache();
    EXPECT_FALSE(isCacheAlive(SkISize::Make(4, 4)));
    EXPECT_EQ(2u, ImageDecodingStore::instance()->cacheEntries());

    // Evict 1.
    evictOneCache();
    EXPECT_FALSE(isCacheAlive(SkISize::Make(1, 1)));
    EXPECT_EQ(1u, ImageDecodingStore::instance()->cacheEntries());

    // Evict 5.
    evictOneCache();
    EXPECT_FALSE(isCacheAlive(SkISize::Make(5, 5)));
    EXPECT_EQ(0u, ImageDecodingStore::instance()->cacheEntries());
}

TEST_F(ImageDecodingStoreTest, pruneCausedByInsertion)
{
    ImageDecodingStore::instance()->setCacheLimitInBytes(100);

    // Insert 100 entries.
    // Cache entries stored should increase and eventually decrease to 1.
    insertCache(SkISize::Make(1, 1));
    insertCache(SkISize::Make(2, 2));
    insertCache(SkISize::Make(3, 3));
    EXPECT_EQ(3u, ImageDecodingStore::instance()->cacheEntries());

    for (int i = 4; i <= 100; ++i)
        insertCache(SkISize::Make(i, i));

    EXPECT_EQ(1u, ImageDecodingStore::instance()->cacheEntries());
    for (int i = 1; i <= 99; ++i)
        EXPECT_FALSE(isCacheAlive(SkISize::Make(i, i)));
    EXPECT_TRUE(isCacheAlive(SkISize::Make(100, 100)));
}

TEST_F(ImageDecodingStoreTest, cacheInUseNotEvicted)
{
    insertCache(SkISize::Make(1, 1));
    insertCache(SkISize::Make(2, 2));
    insertCache(SkISize::Make(3, 3));
    EXPECT_EQ(3u, ImageDecodingStore::instance()->cacheEntries());

    const ScaledImageFragment* cachedImage = lockCache(SkISize::Make(1, 1));
    EXPECT_TRUE(cachedImage);

    // Cache 2 is evicted because cache 1 is in use.
    evictOneCache();
    EXPECT_TRUE(isCacheAlive(SkISize::Make(1, 1)));
    EXPECT_FALSE(isCacheAlive(SkISize::Make(2, 2)));
    EXPECT_TRUE(isCacheAlive(SkISize::Make(3, 3)));

    EXPECT_EQ(2u, ImageDecodingStore::instance()->cacheEntries());
    unlockCache(cachedImage);
}

TEST_F(ImageDecodingStoreTest, destroyImageFrameGenerator)
{
    insertCache(SkISize::Make(1, 1));
    insertCache(SkISize::Make(2, 2));
    insertCache(SkISize::Make(3, 3));
    EXPECT_EQ(3u, ImageDecodingStore::instance()->cacheEntries());

    m_generator.clear();
    EXPECT_FALSE(ImageDecodingStore::instance()->cacheEntries());
}

} // namespace
