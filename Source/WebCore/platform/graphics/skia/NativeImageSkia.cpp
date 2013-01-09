/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
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

#include "skia/ext/image_operations.h"

#include "NativeImageSkia.h"
#include "GraphicsContext3D.h"
#include "MemoryInstrumentationSkia.h"
#include "PlatformInstrumentation.h"
#include "PlatformMemoryInstrumentation.h"
#include "SkPixelRef.h"
#include "SkiaUtils.h"

#if PLATFORM(CHROMIUM)
#include "DeferredImageDecoder.h"
#include "TraceEvent.h"
#endif

namespace WebCore {

NativeImageSkia::NativeImageSkia()
    : m_resolutionScale(1),
      m_resizeRequests(0)
{
}

NativeImageSkia::NativeImageSkia(const SkBitmap& other, float resolutionScale)
    : m_image(other),
      m_resolutionScale(resolutionScale),
      m_resizeRequests(0)
{
}

NativeImageSkia::~NativeImageSkia()
{
}

int NativeImageSkia::decodedSize() const
{
    return m_image.getSize() + m_resizedImage.getSize();
}

bool NativeImageSkia::hasResizedBitmap(const SkISize& scaledImageSize, const SkIRect& scaledImageSubset) const
{
    bool imageScaleEqual = m_cachedImageInfo.scaledImageSize == scaledImageSize;
    bool scaledImageSubsetAvailable = m_cachedImageInfo.scaledImageSubset.contains(scaledImageSubset);
    return imageScaleEqual && scaledImageSubsetAvailable && !m_resizedImage.empty();
}

SkBitmap NativeImageSkia::resizedBitmap(const SkISize& scaledImageSize, const SkIRect& scaledImageSubset) const
{
#if PLATFORM(CHROMIUM)
    if (DeferredImageDecoder::isLazyDecoded(m_image))
        return DeferredImageDecoder::createResizedLazyDecodingBitmap(m_image, scaledImageSize, scaledImageSubset);
#endif

    if (!hasResizedBitmap(scaledImageSize, scaledImageSubset)) {
        bool shouldCache = isDataComplete()
            && shouldCacheResampling(scaledImageSize, scaledImageSubset);

        PlatformInstrumentation::willResizeImage(shouldCache);
        SkBitmap resizedImage = skia::ImageOperations::Resize(m_image, skia::ImageOperations::RESIZE_LANCZOS3, scaledImageSize.width(), scaledImageSize.height(), scaledImageSubset);
        resizedImage.setImmutable();
        PlatformInstrumentation::didResizeImage();

        if (!shouldCache)
            return resizedImage;

        m_resizedImage = resizedImage;
    }

    SkBitmap resizedSubset;
    SkIRect resizedSubsetRect = m_cachedImageInfo.rectInSubset(scaledImageSubset);
    m_resizedImage.extractSubset(&resizedSubset, resizedSubsetRect);
    return resizedSubset;
}

bool NativeImageSkia::shouldCacheResampling(const SkISize& scaledImageSize, const SkIRect& scaledImageSubset) const
{
    // Check whether the requested dimensions match previous request.
    bool matchesPreviousRequest = m_cachedImageInfo.isEqual(scaledImageSize, scaledImageSubset);
    if (matchesPreviousRequest)
        ++m_resizeRequests;
    else {
        m_cachedImageInfo.set(scaledImageSize, scaledImageSubset);
        m_resizeRequests = 0;
        // Reset m_resizedImage now, because we don't distinguish
        // between the last requested resize info and m_resizedImage's
        // resize info.
        m_resizedImage.reset();
    }

    // We can not cache incomplete frames. This might be a good optimization in
    // the future, were we know how much of the frame has been decoded, so when
    // we incrementally draw more of the image, we only have to resample the
    // parts that are changed.
    if (!isDataComplete())
        return false;

    // If the destination bitmap is excessively large, we'll never allow caching.
    static const unsigned long long kLargeBitmapSize = 4096ULL * 4096ULL;
    unsigned long long fullSize = static_cast<unsigned long long>(scaledImageSize.width()) * static_cast<unsigned long long>(scaledImageSize.height());
    unsigned long long fragmentSize = static_cast<unsigned long long>(scaledImageSubset.width()) * static_cast<unsigned long long>(scaledImageSubset.height());

    if (fragmentSize > kLargeBitmapSize)
        return false;

    // If the destination bitmap is small, we'll always allow caching, since
    // there is not very much penalty for computing it and it may come in handy.
    static const unsigned kSmallBitmapSize = 4096;
    if (fragmentSize <= kSmallBitmapSize)
        return true;

    // If "too many" requests have been made for this bitmap, we assume that
    // many more will be made as well, and we'll go ahead and cache it.
    static const int kManyRequestThreshold = 4;
    if (m_resizeRequests >= kManyRequestThreshold)
        return true;

    // If more than 1/4 of the resized image is requested, it's worth caching.
    return fragmentSize > fullSize / 4;
}

NativeImageSkia::CachedImageInfo::CachedImageInfo()
{
    scaledImageSize.setEmpty();
    scaledImageSubset.setEmpty();
}

bool NativeImageSkia::CachedImageInfo::isEqual(const SkISize& otherScaledImageSize, const SkIRect& otherScaledImageSubset) const
{
    return scaledImageSize == otherScaledImageSize && scaledImageSubset == otherScaledImageSubset;
}

void NativeImageSkia::CachedImageInfo::set(const SkISize& otherScaledImageSize, const SkIRect& otherScaledImageSubset)
{
    scaledImageSize = otherScaledImageSize;
    scaledImageSubset = otherScaledImageSubset;
}

SkIRect NativeImageSkia::CachedImageInfo::rectInSubset(const SkIRect& otherScaledImageSubset)
{
    if (!scaledImageSubset.contains(otherScaledImageSubset))
        return SkIRect::MakeEmpty();
    SkIRect subsetRect = otherScaledImageSubset;
    subsetRect.offset(-scaledImageSubset.x(), -scaledImageSubset.y());
    return subsetRect;
}

void NativeImageSkia::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this);
    info.addMember(m_image);
    info.addMember(m_resizedImage);
    info.addMember(m_cachedImageInfo);
}

void reportMemoryUsage(const NativeImageSkia* image, MemoryObjectInfo* memoryObjectInfo)
{
    image->reportMemoryUsage(memoryObjectInfo);
}

} // namespace WebCore
