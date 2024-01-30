/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS)
#include "RemoteNativeImageBackendProxy.h"

#include "RemoteResourceCacheProxy.h"

namespace WebKit {
using namespace WebCore;

std::unique_ptr<RemoteNativeImageBackendProxy> RemoteNativeImageBackendProxy::create(NativeImage& image)
{
    RefPtr<ShareableBitmap> bitmap;
    PlatformImagePtr platformImage;
#if USE(CG)
    bitmap = ShareableBitmap::createFromImagePixels(image);
    if (bitmap)
        platformImage = bitmap->createPlatformImage(DontCopyBackingStore, ShouldInterpolate::Yes);
#endif

    // If we failed to create ShareableBitmap or PlatformImage, fall back to image-draw method.
    if (!platformImage) {
        bitmap = ShareableBitmap::createFromImageDraw(image);
        if (bitmap)
            platformImage = bitmap->createPlatformImage(DontCopyBackingStore, ShouldInterpolate::Yes);

        // If createGraphicsContext() failed because the image colorSpace is not
        // supported for output, fallback to SRGB.
        if (!platformImage) {
            bitmap = ShareableBitmap::createFromImageDraw(image, DestinationColorSpace::SRGB());
            if (bitmap)
                platformImage = bitmap->createPlatformImage(DontCopyBackingStore, ShouldInterpolate::Yes);
        }
    }
    if (!platformImage)
        return nullptr;
    return std::unique_ptr<RemoteNativeImageBackendProxy> { new RemoteNativeImageBackendProxy(bitmap.releaseNonNull(), WTFMove(platformImage)) };
}

RemoteNativeImageBackendProxy::RemoteNativeImageBackendProxy(Ref<ShareableBitmap> bitmap, PlatformImagePtr platformImage)
    : m_bitmap(WTFMove(bitmap))
    , m_platformBackend(WTFMove(platformImage))
{
}

RemoteNativeImageBackendProxy::~RemoteNativeImageBackendProxy() = default;

const PlatformImagePtr& RemoteNativeImageBackendProxy::platformImage() const
{
    return m_platformBackend.platformImage();
}

IntSize RemoteNativeImageBackendProxy::size() const
{
    return m_platformBackend.size();
}

bool RemoteNativeImageBackendProxy::hasAlpha() const
{
    return m_platformBackend.hasAlpha();
}

DestinationColorSpace RemoteNativeImageBackendProxy::colorSpace() const
{
    return m_platformBackend.colorSpace();
}

bool RemoteNativeImageBackendProxy::isRemoteNativeImageBackendProxy() const
{
    return true;
}

std::optional<ShareableBitmap::Handle> RemoteNativeImageBackendProxy::createHandle()
{
    return m_bitmap->createHandle();
}

}

#endif
