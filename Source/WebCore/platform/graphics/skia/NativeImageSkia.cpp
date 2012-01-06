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
#include "SkiaUtils.h"

#if PLATFORM(CHROMIUM)
#include "TraceEvent.h"
#endif

namespace WebCore {

NativeImageSkia::NativeImageSkia()
    : m_resizeRequests(0)
{
}

NativeImageSkia::NativeImageSkia(const SkBitmap& other)
    : m_image(other),
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

bool NativeImageSkia::hasResizedBitmap(const SkIRect& srcSubset, int destWidth, int destHeight) const
{
    return m_cachedImageInfo.isEqual(srcSubset, destWidth, destHeight) && !m_resizedImage.empty();
}

SkBitmap NativeImageSkia::resizedBitmap(const SkIRect& srcSubset,
                                        int destWidth,
                                        int destHeight,
                                        const SkIRect& destVisibleSubset) const
{
#if PLATFORM(CHROMIUM)
    TRACE_EVENT("NativeImageSkia::resizedBitmap", const_cast<NativeImageSkia*>(this), 0);
#endif
    if (!hasResizedBitmap(srcSubset, destWidth, destHeight)) {
        bool shouldCache = isDataComplete()
            && shouldCacheResampling(srcSubset, destWidth, destHeight, destVisibleSubset);

        SkBitmap subset;
        m_image.extractSubset(&subset, srcSubset);
        if (!shouldCache) {
#if PLATFORM(CHROMIUM)
            TRACE_EVENT("nonCachedResize", const_cast<NativeImageSkia*>(this), 0);
#endif
            // Just resize the visible subset and return it.
            SkBitmap resizedImage = skia::ImageOperations::Resize(subset, skia::ImageOperations::RESIZE_LANCZOS3, destWidth, destHeight, destVisibleSubset);
            resizedImage.setImmutable();
            return resizedImage;
        } else {
#if PLATFORM(CHROMIUM)
            TRACE_EVENT("cachedResize", const_cast<NativeImageSkia*>(this), 0);
#endif
            m_resizedImage = skia::ImageOperations::Resize(subset, skia::ImageOperations::RESIZE_LANCZOS3, destWidth, destHeight);
        }
        m_resizedImage.setImmutable();
    }

    SkBitmap visibleBitmap;
    m_resizedImage.extractSubset(&visibleBitmap, destVisibleSubset);
    return visibleBitmap;
}

bool NativeImageSkia::shouldCacheResampling(const SkIRect& srcSubset,
                                            int destWidth,
                                            int destHeight,
                                            const SkIRect& destVisibleSubset) const
{
    // Check whether the requested dimensions match previous request.
    bool matchesPreviousRequest = m_cachedImageInfo.isEqual(srcSubset, destWidth, destHeight);
    if (matchesPreviousRequest)
        ++m_resizeRequests;
    else {
        m_cachedImageInfo.set(srcSubset, destWidth, destHeight);
        m_resizeRequests = 0;
        // Reset m_resizedImage now, because we don't distinguish between the
        // last requested resize info and m_resizedImage's resize info.
        m_resizedImage.reset();
    }

    // We can not cache incomplete frames. This might be a good optimization in
    // the future, were we know how much of the frame has been decoded, so when
    // we incrementally draw more of the image, we only have to resample the
    // parts that are changed.
    if (!isDataComplete())
        return false;

    // If the destination bitmap is small, we'll always allow caching, since
    // there is not very much penalty for computing it and it may come in handy.
    static const int kSmallBitmapSize = 4096;
    if (destWidth * destHeight <= kSmallBitmapSize)
        return true;

    // If "too many" requests have been made for this bitmap, we assume that
    // many more will be made as well, and we'll go ahead and cache it.
    static const int kManyRequestThreshold = 4;
    if (m_resizeRequests >= kManyRequestThreshold)
        return true;

    // If more than 1/4 of the resized image is visible, it's worth caching.
    int destVisibleSize = destVisibleSubset.width() * destVisibleSubset.height();
    return (destVisibleSize > (destWidth * destHeight) / 4);
}

NativeImageSkia::CachedImageInfo::CachedImageInfo()
{
    srcSubset.setEmpty();
}

bool NativeImageSkia::CachedImageInfo::isEqual(const SkIRect& otherSrcSubset, int width, int height) const
{
    return srcSubset == otherSrcSubset
        && requestSize.width() == width
        && requestSize.height() == height;
}

void NativeImageSkia::CachedImageInfo::set(const SkIRect& otherSrcSubset, int width, int height)
{
    srcSubset = otherSrcSubset;
    requestSize.setWidth(width);
    requestSize.setHeight(height);
}

} // namespace WebCore
