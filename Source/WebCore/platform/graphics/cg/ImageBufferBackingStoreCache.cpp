/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "ImageBufferBackingStoreCache.h"

#if USE(IOSURFACE_CANVAS_BACKING_STORE)
#include <CoreGraphics/CoreGraphics.h>
#include <IOSurface/IOSurface.h>

static const double purgeInterval = 5;

namespace WebCore {

static RetainPtr<IOSurfaceRef> createIOSurface(const IntSize& size)
{
    unsigned pixelFormat = 'BGRA';
    unsigned bytesPerElement = 4;
    int width = size.width();
    int height = size.height();

    unsigned long bytesPerRow = IOSurfaceAlignProperty(kIOSurfaceBytesPerRow, size.width() * bytesPerElement);
    if (!bytesPerRow)
        return 0;

    unsigned long allocSize = IOSurfaceAlignProperty(kIOSurfaceAllocSize, size.height() * bytesPerRow);
    if (!allocSize)
        return 0;

    const int kNumCreationParameters = 6;
    const void* keys[kNumCreationParameters];
    const void* values[kNumCreationParameters];
    keys[0] = kIOSurfaceWidth;
    values[0] = CFNumberCreate(0, kCFNumberIntType, &width);
    keys[1] = kIOSurfaceHeight;
    values[1] = CFNumberCreate(0, kCFNumberIntType, &height);
    keys[2] = kIOSurfacePixelFormat;
    values[2] = CFNumberCreate(0, kCFNumberIntType, &pixelFormat);
    keys[3] = kIOSurfaceBytesPerElement;
    values[3] = CFNumberCreate(0, kCFNumberIntType, &bytesPerElement);
    keys[4] = kIOSurfaceBytesPerRow;
    values[4] = CFNumberCreate(0, kCFNumberLongType, &bytesPerRow);
    keys[5] = kIOSurfaceAllocSize;
    values[5] = CFNumberCreate(0, kCFNumberLongType, &allocSize);

    RetainPtr<CFDictionaryRef> dict = adoptCF(CFDictionaryCreate(0, keys, values, kNumCreationParameters, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    for (unsigned i = 0; i < kNumCreationParameters; i++)
        CFRelease(values[i]);

    return adoptCF(IOSurfaceCreate(dict.get()));
}

ImageBufferBackingStoreCache::ImageBufferBackingStoreCache()
    : m_purgeTimer(this, &ImageBufferBackingStoreCache::timerFired, purgeInterval)
    , m_pixelsCached(0)
    {
    }

ImageBufferBackingStoreCache& ImageBufferBackingStoreCache::get()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(ImageBufferBackingStoreCache, cache, ());
    return cache;
}

bool ImageBufferBackingStoreCache::isAcceptableSurface(const IOSurfaceAndContextWithCreationParams& info, const IntSize& requestedSize, CGColorSpaceRef colorSpace, bool needExactSize) const
{
    IOSurfaceRef surface = info.surface.get();
    IntSize actualSize(IOSurfaceGetWidth(surface), IOSurfaceGetHeight(surface));
    if (!CFEqual(info.colorSpace.get(), colorSpace))
        return false;
    if (needExactSize && actualSize != requestedSize)
        return false;
    if (actualSize.width() < requestedSize.width() || actualSize.height() < requestedSize.height())
        return false;
    return true;
}

void ImageBufferBackingStoreCache::insertIntoCache(IOSurfaceAndContextWithCreationParams&& info)
{
    IOSurfaceRef surface = info.surface.get();
    IntSize surfaceSize(IOSurfaceGetWidth(surface), IOSurfaceGetHeight(surface));

    auto toAdd = new IOSurfaceAndContextWithCreationParams(info);
    auto insertedTuple = m_cachedSurfaces.add(convertSizeToKey(surfaceSize), InfoLinkedList());
    insertedTuple.iterator->value.append(toAdd);
    
    m_pixelsCached += surfaceSize.area();
}

auto ImageBufferBackingStoreCache::takeFromCache(CachedSurfaceMap::iterator iter, IOSurfaceAndContextWithCreationParams* info) -> IOSurfaceAndContextWithCreationParams
{
    ASSERT(info);
    ASSERT(iter != m_cachedSurfaces.end());

    IOSurfaceRef surface = info->surface.get();
    m_pixelsCached -= IOSurfaceGetWidth(surface) * IOSurfaceGetHeight(surface);

    iter->value.remove(info);
    if (iter->value.isEmpty())
        m_cachedSurfaces.remove(iter);
    IOSurfaceAndContextWithCreationParams result = std::move(*info);
    delete info;
    return result;
}

bool ImageBufferBackingStoreCache::tryTakeFromCache(const IntSize& size, CGColorSpaceRef colorSpace, bool needExactSize, IOSurfaceAndContextWithCreationParams& outInfo)
{
    CachedSurfaceMap::iterator i = m_cachedSurfaces.find(convertSizeToKey(size));
    if (i == m_cachedSurfaces.end())
        return nullptr;
    InfoLinkedList& ll = i->value;
    for (auto info = ll.head(); info; info = info->next()) {
        if (isAcceptableSurface(*info, size, colorSpace, needExactSize)) {
            outInfo = takeFromCache(i, info);
            return true;
        }
    }
    return false;
}

ImageBufferBackingStoreCache::IOSurfaceAndContext ImageBufferBackingStoreCache::getOrAllocate(IntSize size, CGColorSpaceRef colorSpace, bool needExactSize)
{
    IOSurfaceAndContextWithCreationParams foundInfo;
    if (tryTakeFromCache(size, colorSpace, needExactSize, foundInfo)) {
        IOSurfaceRef surface = foundInfo.surface.get();
        CGContextRef context = foundInfo.context.get();
        CGContextSaveGState(context);
        auto activeInserted = m_activeSurfaces.add(surface, std::move(foundInfo));
        ASSERT(activeInserted.isNewEntry);
        return activeInserted.iterator->value;
    }

    RetainPtr<IOSurfaceRef> surface = createIOSurface(size);
    if (!surface.get())
        return IOSurfaceAndContext();

    RetainPtr<CGContextRef> context = adoptCF(wkIOSurfaceContextCreate(surface.get(), size.width(), size.height(), colorSpace));
    if (!context.get())
        return IOSurfaceAndContext();
    CGContextSaveGState(context.get());

    auto insertedTuple = m_activeSurfaces.add(surface, IOSurfaceAndContextWithCreationParams(surface.get(), context.get(), colorSpace));
    ASSERT(insertedTuple.isNewEntry);

    return insertedTuple.iterator->value;
}

void ImageBufferBackingStoreCache::deallocate(IOSurfaceRef surface)
{
    ActiveSurfaceMap::iterator lookup = m_activeSurfaces.find(surface);
    ASSERT(lookup != m_activeSurfaces.end());
    
    auto info = std::move(lookup->value);
    m_activeSurfaces.remove(lookup);
    
    IOSurfaceRef ioSurface = info.surface.get();
    CGContextRef context = info.context.get();
    IntSize surfaceSize(IOSurfaceGetWidth(ioSurface), IOSurfaceGetHeight(ioSurface));
    int surfaceArea = surfaceSize.area();

    static const int kMaxPixelsCached = 1024 * 1024 * 64; // 256MB
    if (surfaceArea > kMaxPixelsCached)
        return;

    // Evict
    auto bucket = m_cachedSurfaces.find(convertSizeToKey(surfaceSize));
    if (bucket != m_cachedSurfaces.end()) {
        for (int itemsInBucket = bucket->value.size();
            itemsInBucket > 0 && m_pixelsCached + surfaceArea > kMaxPixelsCached;
            --itemsInBucket)
            takeFromCache(bucket, bucket->value.head());
    }
    while (m_pixelsCached + surfaceArea > kMaxPixelsCached) {
        CachedSurfaceMap::iterator iter = m_cachedSurfaces.begin();
        takeFromCache(iter, iter->value.head());
    }

    CGContextRestoreGState(context);
    // Clear opportunistically so CG has more time to carry it out.
    CGContextClearRect(context, CGRectMake(0, 0, surfaceSize.width(), surfaceSize.height()));
#if !PLATFORM(IOS) && __MAC_OS_X_VERSION_MIN_REQUIRED < 1090
    CGContextFlush(context);
#endif

    insertIntoCache(std::move(info));

    schedulePurgeTimer();
}

void ImageBufferBackingStoreCache::timerFired(DeferrableOneShotTimer<ImageBufferBackingStoreCache>&)
{
    while (!m_cachedSurfaces.isEmpty()) {
        CachedSurfaceMap::iterator iter = m_cachedSurfaces.begin();
        takeFromCache(iter, iter->value.head());
    }
}

void ImageBufferBackingStoreCache::schedulePurgeTimer()
{
    m_purgeTimer.restart();
}

}
#endif // IOSURFACE_CANVAS_BACKING_STORE
