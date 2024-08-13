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

#include "GPUConnectionToWebProcess.h"
#include <wtf/TZoneMallocInlines.h>

#if HAVE(IOSURFACE)
#include <WebCore/IOSurfacePool.h>
#endif

namespace WebKit {
using namespace WebCore;

constexpr Seconds defaultRemoteSharedResourceCacheTimeout = 15_s;

// Per GPU process limit of accelerated image buffers. These consume limited global OS resources.
constexpr size_t globalAcceleratedImageBufferLimit = 10000;

// Per GPU process limit of image buffers for canvas. These consume limited process-wide resources.
constexpr size_t globalImageBufferForCanvasLimit = 200000;

// Per Web Content process limit of accelerated image buffers for canvas. Prevents GPU resource exhaustion affecting other Web Content processes.
constexpr size_t acceleratedImageBufferForCanvasLimit = 5000;

// Per Web Content process limit of image buffers for canvas. Prevents IPC-related resource exhaustion affecting other Web Content processes.
constexpr size_t imageBufferForCanvasLimit = 50000;

static std::atomic<size_t> globalAcceleratedImageBufferCount = 0;
static std::atomic<size_t> globalImageBufferForCanvasCount = 0;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteSharedResourceCache);

Ref<RemoteSharedResourceCache> RemoteSharedResourceCache::create(GPUConnectionToWebProcess& gpuConnectionToWebProcess)
{
    return adoptRef(*new RemoteSharedResourceCache(gpuConnectionToWebProcess));
}

RemoteSharedResourceCache::RemoteSharedResourceCache(GPUConnectionToWebProcess& gpuConnectionToWebProcess)
    : m_resourceOwner(gpuConnectionToWebProcess.webProcessIdentity())
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

void RemoteSharedResourceCache::didCreateImageBuffer(RenderingPurpose purpose, RenderingMode renderingMode)
{
    if (purpose == RenderingPurpose::Canvas) {
        if (renderingMode == RenderingMode::Accelerated)
            ++m_acceleratedImageBufferForCanvasCount;
        ++globalImageBufferForCanvasCount;
        ++m_imageBufferForCanvasCount;
    }
    if (renderingMode == RenderingMode::Accelerated)
        ++globalAcceleratedImageBufferCount;
}

void RemoteSharedResourceCache::didReleaseImageBuffer(RenderingPurpose purpose, RenderingMode renderingMode)
{
    if (purpose == RenderingPurpose::Canvas) {
        if (renderingMode == RenderingMode::Accelerated)
            --m_acceleratedImageBufferForCanvasCount;
        --globalImageBufferForCanvasCount;
        --m_imageBufferForCanvasCount;
    }
    if (renderingMode == RenderingMode::Accelerated)
        --globalAcceleratedImageBufferCount;
}

bool RemoteSharedResourceCache::reachedAcceleratedImageBufferLimit(RenderingPurpose purpose) const
{
    return (purpose == RenderingPurpose::Canvas && m_acceleratedImageBufferForCanvasCount >= acceleratedImageBufferForCanvasLimit) || globalAcceleratedImageBufferCount >= globalAcceleratedImageBufferLimit;
}

bool RemoteSharedResourceCache::reachedImageBufferForCanvasLimit() const
{
    return m_imageBufferForCanvasCount >= imageBufferForCanvasLimit || globalImageBufferForCanvasCount >= globalImageBufferForCanvasLimit;
}

ImageBufferResourceLimits RemoteSharedResourceCache::getResourceLimitsForTesting() const
{
    return {
        .acceleratedImageBufferForCanvasCount = m_acceleratedImageBufferForCanvasCount,
        .acceleratedImageBufferForCanvasLimit = acceleratedImageBufferForCanvasLimit,
        .globalAcceleratedImageBufferCount = globalAcceleratedImageBufferCount,
        .globalAcceleratedImageBufferLimit = globalAcceleratedImageBufferLimit,
        .globalImageBufferForCanvasCount = globalImageBufferForCanvasCount,
        .globalImageBufferForCanvasLimit = globalImageBufferForCanvasLimit,
        .imageBufferForCanvasCount = m_imageBufferForCanvasCount,
        .imageBufferForCanvasLimit = imageBufferForCanvasLimit,
    };
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
