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

#include "RemoteSharedResourceCacheMessages.h"
#include <wtf/MainThread.h>
#include <wtf/TZoneMallocInlines.h>

#if HAVE(IOSURFACE)
#include <WebCore/IOSurfacePool.h>
#endif

namespace WebKit {
using namespace WebCore;

// Per GPU process limit of image buffers for canvas. These consume limited process-wide resources.
constexpr size_t globalImageBufferForCanvasLimit = 200000;

// Per Web Content process limit of accelerated image buffers for canvas. Prevents GPU resource exhaustion affecting other Web Content processes.
constexpr size_t acceleratedImageBufferForCanvasLimit = 5000;

// Per Web Content process limit of image buffers for canvas. Prevents IPC-related resource exhaustion affecting other Web Content processes.
constexpr size_t imageBufferForCanvasLimit = 50000;

static std::atomic<size_t> globalAcceleratedImageBufferCount = 0;
static std::atomic<size_t> globalImageBufferForCanvasCount = 0;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteSharedResourceCache);

Ref<RemoteSharedResourceCache> RemoteSharedResourceCache::create(IPC::Connection& connection, const ProcessIdentity& resourceOwner)
{
    Ref cache = adoptRef(*new RemoteSharedResourceCache(connection, resourceOwner));
    connection.addWorkQueueMessageReceiver(Messages::RemoteSharedResourceCache::messageReceiverName(), cache->workQueue(), cache);
    return cache;
}

RemoteSharedResourceCache::RemoteSharedResourceCache(IPC::Connection& connection, const ProcessIdentity& resourceOwner)
    : m_connection(connection)
    , m_workQueue(WorkQueue::create("org.webkit.RemoteSharedResourceCache"_s, WorkQueue::QOS::UserInteractive))
    , m_resourceOwner(resourceOwner)
#if HAVE(IOSURFACE)
    , m_ioSurfacePool(IOSurfacePool::create())
#endif
{
}

RemoteSharedResourceCache::~RemoteSharedResourceCache()
{
    ASSERT(m_isClosed);
}

void RemoteSharedResourceCache::close()
{
    assertIsMainThread();
    if (m_isClosed)
        return;
    m_isClosed = true;
    protect(m_connection)->removeWorkQueueMessageReceiver(Messages::RemoteSharedResourceCache::messageReceiverName());
}

bool RemoteSharedResourceCache::addSerializedImageBuffer(RemoteSerializedImageBufferIdentifier identifier, Ref<ImageBuffer> imageBuffer)
{
    didCreateImageBuffer(imageBuffer->renderingPurpose(), imageBuffer->renderingMode());
    return m_serializedImageBuffers.add({ identifier, 0 }, WTF::move(imageBuffer));
}

RefPtr<ImageBuffer> RemoteSharedResourceCache::takeSerializedImageBuffer(RemoteSerializedImageBufferIdentifier identifier)
{
    RefPtr imageBuffer = m_serializedImageBuffers.take({ { identifier, 0 }, 0 }, defaultRemoteSharedResourceCacheTimeout);
    if (imageBuffer)
        didReleaseImageBuffer(imageBuffer->renderingPurpose(), imageBuffer->renderingMode());
    return imageBuffer;
}

bool RemoteSharedResourceCache::addNativeImage(RemoteNativeImageReference reference, Ref<NativeImage> image)
{
    return m_nativeImages.add(reference, WTF::move(image));
}

bool RemoteSharedResourceCache::addNativeImage(RemoteNativeImageReference reference, RefPtr<NativeImage>&& image)
{
    return m_nativeImages.add(reference, WTF::move(image));
}

RefPtr<NativeImage> RemoteSharedResourceCache::readNativeImage(RemoteNativeImageReadReference&& read, Seconds timeout)
{
    return m_nativeImages.read(WTF::move(read), timeout);
}

std::optional<RefPtr<NativeImage>> RemoteSharedResourceCache::readNativeImage(IPC::StreamServerConnection& connection, RemoteNativeImageReadReference&& read)
{
    return m_nativeImages.read(connection, WTF::move(read));
}

std::optional<ShareableBitmap::Handle> RemoteSharedResourceCache::createBitmapHandle(const NativeImage& image)
{
    RefPtr<ShareableBitmap> bitmap;
#if USE(CG)
    bitmap = ShareableBitmap::createFromImagePixels(image);
#endif
    if (!bitmap)
        bitmap = ShareableBitmap::createFromImageDraw(image, ColorSpace { image.colorSpace() });
    if (!bitmap)
        return std::nullopt;
    auto handle = bitmap->createHandle();
    if (!handle)
        return std::nullopt;
    if (m_resourceOwner)
        handle->setOwnershipOfMemory(m_resourceOwner, MemoryLedger::Graphics);
    auto platformImage = bitmap->createPlatformImage();
    if (!platformImage) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }
    image.replacePlatformImage(WTF::move(platformImage));
    return handle;
}

void RemoteSharedResourceCache::releaseNativeImage(RemoteNativeImageWriteReference&& write)
{
    assertIsCurrent(workQueue());
    m_nativeImages.remove(WTF::move(write));
}

void RemoteSharedResourceCache::releaseSerializedImageBuffer(RemoteSerializedImageBufferIdentifier identifier)
{
    assertIsCurrent(workQueue());
    // Must wait due to using current accounting mechanism.
    takeSerializedImageBuffer(identifier);
}

void RemoteSharedResourceCache::getNativeImagePixels(RemoteNativeImageReadReference&& read, CompletionHandler<void(std::optional<ShareableBitmap::Handle>&&)>&& completionHandler)
{
    assertIsCurrent(workQueue());
    // The producers publish the image from their own work queues, and none of them waits for this queue.
    RefPtr image = readNativeImage(WTF::move(read), defaultRemoteSharedResourceCacheTimeout);
    if (!image) {
        completionHandler(std::nullopt);
        return;
    }
    completionHandler(createBitmapHandle(*image));
}

void RemoteSharedResourceCache::lowMemoryHandler()
{
#if HAVE(IOSURFACE)
    Ref { m_ioSurfacePool }->discardAllSurfaces();
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

bool RemoteSharedResourceCache::reachedAcceleratedImageBufferLimit(RenderingPurpose purpose, size_t globalAcceleratedImageBufferLimit) const
{
    return (purpose == RenderingPurpose::Canvas && m_acceleratedImageBufferForCanvasCount >= acceleratedImageBufferForCanvasLimit) || globalAcceleratedImageBufferCount >= globalAcceleratedImageBufferLimit;
}

bool RemoteSharedResourceCache::reachedImageBufferForCanvasLimit() const
{
    return m_imageBufferForCanvasCount >= imageBufferForCanvasLimit || globalImageBufferForCanvasCount >= globalImageBufferForCanvasLimit;
}

ImageBufferResourceLimits RemoteSharedResourceCache::getResourceLimitsForTesting(size_t globalAcceleratedImageBufferLimit) const
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
