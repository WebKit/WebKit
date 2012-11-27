/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ImageDecodingStore.h"

#include "ImageFrameGenerator.h"
#include "ScaledImageFragment.h"
#include "SharedBuffer.h"

namespace WebCore {

namespace {

ImageDecodingStore* s_instance = 0;

static void setInstance(ImageDecodingStore* imageDecodingStore)
{
    delete s_instance;
    s_instance = imageDecodingStore;
}

} // namespace

ImageDecodingStore::ImageDecodingStore()
{
}

ImageDecodingStore::~ImageDecodingStore()
{
}

ImageDecodingStore* ImageDecodingStore::instance()
{
    return s_instance;
}

void ImageDecodingStore::initializeOnce()
{
    setInstance(ImageDecodingStore::create().leakPtr());
}

void ImageDecodingStore::shutdown()
{
    setInstance(0);
}

const ScaledImageFragment* ImageDecodingStore::lockCompleteCache(const ImageFrameGenerator* generator, const SkISize& scaledSize)
{
    CacheEntry* cacheEntry = 0;
    {
        MutexLocker lock(m_mutex);
        CacheMap::iterator iter = m_cacheMap.find(std::make_pair(generator, scaledSize));
        if (iter == m_cacheMap.end())
            return 0;
        cacheEntry = iter->value;
        if (!cacheEntry->cachedImage->isComplete())
            return 0;

        // Increment use count such that it doesn't get evicted.
        ++cacheEntry->useCount;
    }
    cacheEntry->cachedImage->bitmap().lockPixels();
    return cacheEntry->cachedImage.get();
}

const ScaledImageFragment* ImageDecodingStore::lockIncompleteCache(const ImageFrameGenerator* generator, const SkISize& scaledSize)
{
    // TODO: Implement.
    return 0;
}

void ImageDecodingStore::unlockCache(const ImageFrameGenerator* generator, const ScaledImageFragment* cachedImage)
{
    cachedImage->bitmap().unlockPixels();
    if (!cachedImage->isComplete()) {
        // Delete the image if it is incomplete. It was never stored in cache.
        delete cachedImage;
        return;
    }

    MutexLocker lock(m_mutex);
    CacheMap::iterator iter = m_cacheMap.find(std::make_pair(generator, cachedImage->scaledSize()));
    ASSERT(iter != m_cacheMap.end());

    CacheEntry* cacheEntry = iter->value;
    --cacheEntry->useCount;
    ASSERT(cacheEntry->useCount >= 0);
}

const ScaledImageFragment* ImageDecodingStore::insertAndLockCache(const ImageFrameGenerator* generator, PassOwnPtr<ScaledImageFragment> image)
{
    // Prune old cache entries to give space for the new one.
    prune();

    // Lock the underlying SkBitmap to prevent it from being purged.
    image->bitmap().lockPixels();

    if (!image->isComplete()) {
        // Incomplete image is not stored in the cache and deleted after use.
        // See unlockCache().
        // TODO: We should allow incomplete images to be stored together with
        // the corresponding ImageDecoder.
        return image.leakPtr();
    }

    ScaledImageFragment* cachedImage = image.get();
    OwnPtr<CacheEntry> newCacheEntry = CacheEntry::createAndUse(image);

    CacheIdentifier key = std::make_pair(generator, cachedImage->scaledSize());
    MutexLocker lock(m_mutex);
    ASSERT(!m_cacheMap.contains(key));

    // m_cacheMap is used for indexing and quick lookup of a cached image.
    // m_cacheEntries is used to support LRU operations to reorder cache
    // entries quickly. It also owns cached images.
    m_cacheMap.add(key, newCacheEntry.get());
    m_cacheEntries.append(newCacheEntry.release());
    return cachedImage;
}

void ImageDecodingStore::prune()
{
    // TODO: Implement.
}

} // namespace WebCore
