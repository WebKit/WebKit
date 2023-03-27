/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "CacheableDecodedImage.h"

#include "PurgeableCachedDecodedImage.h"

namespace WebKit {

std::unique_ptr<CacheableDecodedImage> CacheableDecodedImage::create(RefPtr<ShareableBitmap>&& bitmap)
{
    if (!bitmap)
        return nullptr;

    auto platformImage = bitmap->createPlatformImage(DontCopyBackingStore, ShouldInterpolate::Yes);
    if (!platformImage)
        return nullptr;

    return makeUnique<CacheableDecodedImage>(bitmap.releaseNonNull(), WTFMove(platformImage));
}

CacheableDecodedImage::CacheableDecodedImage(Ref<ShareableBitmap>&& bitmap, WebCore::PlatformImagePtr&& platformImage)
    : DecodedImage(DecodedImageType::Cacheable)
    , m_bitmap(WTFMove(bitmap))
    , m_platformImage(WTFMove(platformImage))
{
}

CacheableDecodedImage::~CacheableDecodedImage() = default;

WebCore::PlatformImagePtr CacheableDecodedImage::platformImage() const
{
    return m_platformImage;
}

std::unique_ptr<CachedDecodedImage> CacheableDecodedImage::createCachedDecodedImage() const
{
#if ENABLE(PURGEABLE_DECODED_IMAGE_DATA_CACHE)
    return PurgeableCachedDecodedImage::create(m_bitmap.get());
#else
    return nullptr;
#endif
}

} // namespace WebKit
