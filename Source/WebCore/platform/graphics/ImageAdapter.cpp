/*
 * Copyright (C) 2024 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ImageAdapter.h"

#include "BitmapImage.h"

namespace WebCore {

#if !PLATFORM(COCOA) && !PLATFORM(GTK) && !PLATFORM(WIN)
Ref<Image> ImageAdapter::loadPlatformResource(const char* resource)
{
    WTFLogAlways("WARNING: trying to load platform resource '%s'", resource);
    return BitmapImage::create();
}

void ImageAdapter::invalidate()
{
}
#endif // !PLATFORM(COCOA) && !PLATFORM(GTK) && !PLATFORM(WIN)

RefPtr<NativeImage> ImageAdapter::nativeImageOfSize(const IntSize& size)
{
    unsigned count = image().frameCount();

    for (unsigned i = 0; i < count; ++i) {
        auto nativeImage = image().nativeImageAtIndexCacheIfNeeded(i);
        if (nativeImage && nativeImage->size() == size)
            return nativeImage;
    }

    // Fallback to the first frame image if we can't find the right size
    return image().nativeImageAtIndex(0);
}

Vector<Ref<NativeImage>> ImageAdapter::allNativeImages()
{
    Vector<Ref<NativeImage>> nativeImages;
    unsigned count = image().frameCount();

    for (unsigned i = 0; i < count; ++i) {
        if (auto nativeImage = image().nativeImageAtIndexCacheIfNeeded(i))
            nativeImages.append(nativeImage.releaseNonNull());
    }

    return nativeImages;
}

} // namespace WebCore
