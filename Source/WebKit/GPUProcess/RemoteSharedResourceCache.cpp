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

#if ENABLE(GPU_PROCESS)
#include "RemoteSharedResourceCache.h"
#include <mutex>
#include <wtf/NeverDestroyed.h>

#if HAVE(IOSURFACE)
#include <WebCore/IOSurfacePool.h>
#endif

namespace WebKit {
using namespace WebCore;

// Per GPU process limit of accelerated image buffers.
// These consume limited global OS resources.
constexpr size_t acceleratedImageBufferGlobalLimit = 30000;

// Per WebContent process limit of accelerated image buffers.
constexpr size_t acceleratedImageBufferLimit = 5000;

// Per WebContent process limit of accelerated image buffers for 2d context.
// It is most common to leak 2d contexts.
constexpr size_t acceleratedImageBuffer2DContextLimit = 1000;

constexpr Seconds defaultRemoteSharedResourceCacheTimeout = 15_s;

// Per GPU process count of current accelerated image buffers.
static const Ref<ResourceCounter>& globalAcceleratedImageBufferCounter()
{
    static LazyNeverDestroyed<Ref<ResourceCounter>> instance;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        instance.construct(*new ResourceCounter);
    });
    return instance;
}

Ref<RemoteSharedResourceCache> RemoteSharedResourceCache::create(GPUConnectionToWebProcess& gpuConnectionToWebProcess)
{
    return adoptRef(*new RemoteSharedResourceCache(gpuConnectionToWebProcess));
}

RemoteSharedResourceCache::RemoteSharedResourceCache(GPUConnectionToWebProcess& gpuConnectionToWebProcess)
    : m_acceleratedImageBufferCounter(adoptRef(*new ResourceCounter))
    , m_resourceOwner(gpuConnectionToWebProcess.webProcessIdentity())
#if HAVE(IOSURFACE)
    , m_ioSurfacePool(IOSurfacePool::create())
#endif
{
}

RemoteSharedResourceCache::~RemoteSharedResourceCache() = default;

void RemoteSharedResourceCache::addSerializedImageBuffer(RenderingResourceIdentifier identifier, Ref<ImageBuffer> imageBuffer)
{
    m_serializedImageBuffers.add({ identifier, 0 }, WTFMove(imageBuffer));
}

RefPtr<ImageBuffer> RemoteSharedResourceCache::takeSerializedImageBuffer(RenderingResourceIdentifier identifier)
{
    return m_serializedImageBuffers.take({ { identifier, 0 }, 0 }, defaultRemoteSharedResourceCacheTimeout);
}

void RemoteSharedResourceCache::releaseSerializedImageBuffer(WebCore::RenderingResourceIdentifier identifier)
{
    m_serializedImageBuffers.remove({ { identifier, 0 }, 0 });
}

void RemoteSharedResourceCache::lowMemoryHandler()
{
#if HAVE(IOSURFACE)
    m_ioSurfacePool->discardAllSurfaces();
#endif
}

Ref<ResourceCounter> RemoteSharedResourceCache::acceleratedImageBufferCounter() const
{
    return m_acceleratedImageBufferCounter;
}

Ref<ResourceCounter> RemoteSharedResourceCache::globalAcceleratedImageBufferCounter() const
{
    return WebKit::globalAcceleratedImageBufferCounter();
}

WebCore::RenderingMode RemoteSharedResourceCache::adjustAcceleratedImageBufferRenderingMode(RenderingPurpose renderingPurpose) const
{
    // These are naturally racy, but the limits are heuristic in nature.
    size_t globalCount = WebKit::globalAcceleratedImageBufferCounter()->count();
    if (globalCount >= acceleratedImageBufferGlobalLimit)
        return RenderingMode::Unaccelerated;
    size_t count = m_acceleratedImageBufferCounter->count();
    if (count >= acceleratedImageBufferLimit)
        return RenderingMode::Unaccelerated;
    if (renderingPurpose == RenderingPurpose::Canvas && count >= acceleratedImageBuffer2DContextLimit)
        return RenderingMode::Unaccelerated;
    return RenderingMode::Accelerated;
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
