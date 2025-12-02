/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "RemoteNativeImageProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteResourceCacheProxy.h"
#include <WebCore/Color.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/ImageBuffer.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

static PlatformImagePtr placeholderPlatformImage()
{
    static NeverDestroyed<PlatformImagePtr> image = [] {
        // Currently we return a placeholder that does not match the NativeImage
        // size, colorspace, isAlpha properties.
        RefPtr buffer = ImageBuffer::create(FloatSize { 1, 1 }, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1, DestinationColorSpace::SRGB(), ImageBufferFormat { PixelFormat::BGRA8 });
        RELEASE_ASSERT(buffer);
        buffer->context().fillRect({ 0, 0, 1, 1 }, Color::black);
        RefPtr nativeImage = ImageBuffer::sinkIntoNativeImage(WTFMove(buffer));
        RELEASE_ASSERT(nativeImage);
        return nativeImage->platformImage();
    }();
    return image;
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteNativeImageProxy);

Ref<RemoteNativeImageProxy> RemoteNativeImageProxy::create(const IntSize& size, PlatformColorSpace&& colorSpace, bool hasAlpha, WeakRef<RemoteResourceCacheProxy>&& resourceCache)
{
    return adoptRef(*new RemoteNativeImageProxy(size, WTFMove(colorSpace), hasAlpha, WTFMove(resourceCache)));
}

RemoteNativeImageProxy::RemoteNativeImageProxy(const IntSize& size, PlatformColorSpace&& colorSpace, bool hasAlpha, WeakRef<RemoteResourceCacheProxy>&& resourceCache)
    : NativeImage(nullptr)
    , m_resourceCache(WTFMove(resourceCache))
    , m_size(size)
    , m_colorSpace(WTFMove(colorSpace))
    , m_hasAlpha(hasAlpha)
{
}

RemoteNativeImageProxy::~RemoteNativeImageProxy()
{
    if (CheckedPtr resourceCache = m_resourceCache.get())
        resourceCache->willDestroyRemoteNativeImageProxy(*this);
}

const PlatformImagePtr& RemoteNativeImageProxy::platformImage() const
{
    if (!m_platformImage) {
        if (CheckedPtr resourceCache = m_resourceCache.get())
            m_platformImage = resourceCache->platformImage(*this);
    }
    // The callers do not expect !platformImage().
    if (!m_platformImage)
        m_platformImage = placeholderPlatformImage();
    return m_platformImage;
}

IntSize RemoteNativeImageProxy::size() const
{
    return m_size;
}

bool RemoteNativeImageProxy::hasAlpha() const
{
    return m_hasAlpha;
}

DestinationColorSpace RemoteNativeImageProxy::colorSpace() const
{
    // FIXME: Images are not in destination color space, they are in any color space.
    return DestinationColorSpace { m_colorSpace };
}

}

#endif
