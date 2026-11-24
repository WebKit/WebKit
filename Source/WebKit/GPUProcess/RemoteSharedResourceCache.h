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

#pragma once

#if ENABLE(GPU_PROCESS)

#include "Connection.h"
#include "RemoteNativeImageIdentifier.h"
#include "RemoteSerializedImageBufferIdentifier.h"
#include "ThreadSafeObjectHeap.h"
#include "WorkQueueMessageReceiver.h"
#include <WebCore/ImageBuffer.h>
#include <WebCore/ImageBufferResourceLimits.h>
#include <WebCore/ProcessIdentity.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <WebCore/ShareableBitmap.h>
#include <wtf/FastMalloc.h>
#include <wtf/Ref.h>
#include <wtf/Seconds.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WorkQueue.h>

#if USE(IOSURFACE)
#include <WebCore/IOSurfacePool.h>
#endif

namespace WebKit {

// Timeout for waiting on a resource to be published in the shared cache from another work queue.
constexpr Seconds defaultRemoteSharedResourceCacheTimeout = 15_s;

// Class holding GPU process resources per Web Content process.
// Thread-safe. Receives its messages on its own work queue.
class RemoteSharedResourceCache final : public IPC::WorkQueueMessageReceiver<WTF::DestructionThread::Any> {
    WTF_MAKE_TZONE_ALLOCATED(RemoteSharedResourceCache);
public:
    static Ref<RemoteSharedResourceCache> create(IPC::Connection&, const WebCore::ProcessIdentity& resourceOwner);
    virtual ~RemoteSharedResourceCache();

    void ref() const final { IPC::WorkQueueMessageReceiver<WTF::DestructionThread::Any>::ref(); }
    void deref() const final { IPC::WorkQueueMessageReceiver<WTF::DestructionThread::Any>::deref(); }

    // Stops receiving messages. Called when the connection closes.
    void close();

    bool addSerializedImageBuffer(RemoteSerializedImageBufferIdentifier, Ref<WebCore::ImageBuffer>);
    RefPtr<WebCore::ImageBuffer> takeSerializedImageBuffer(RemoteSerializedImageBufferIdentifier);

    bool addNativeImage(RemoteNativeImageReference, Ref<WebCore::NativeImage>);
    // Null tells the readers that the image could not be made.
    bool addNativeImage(RemoteNativeImageReference, RefPtr<WebCore::NativeImage>&&);
    RefPtr<WebCore::NativeImage> readNativeImage(RemoteNativeImageReadReference&&, Seconds timeout);
    // For stream message handlers: std::nullopt means that the handler should defer the message.
    std::optional<RefPtr<WebCore::NativeImage>> readNativeImage(IPC::StreamServerConnection&, RemoteNativeImageReadReference&&);

    const WebCore::ProcessIdentity& resourceOwner() const LIFETIME_BOUND { return m_resourceOwner; }
#if HAVE(IOSURFACE)
    WebCore::IOSurfacePool& ioSurfacePool() const { return m_ioSurfacePool; }
#endif

    NODELETE void didCreateImageBuffer(WebCore::RenderingPurpose, WebCore::RenderingMode);
    NODELETE void didReleaseImageBuffer(WebCore::RenderingPurpose, WebCore::RenderingMode);
    NODELETE bool reachedAcceleratedImageBufferLimit(WebCore::RenderingPurpose, size_t globalAcceleratedImageBufferLimit) const;
    NODELETE bool reachedImageBufferForCanvasLimit() const;
    NODELETE WebCore::ImageBufferResourceLimits getResourceLimitsForTesting(size_t globalAcceleratedImageBufferLimit) const;

    void lowMemoryHandler();

private:
    RemoteSharedResourceCache(IPC::Connection&, const WebCore::ProcessIdentity& resourceOwner);

    WorkQueue& workQueue() const LIFETIME_BOUND { return m_workQueue; }

    // IPC::WorkQueueMessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // Messages
    void releaseSerializedImageBuffer(RemoteSerializedImageBufferIdentifier);
    void releaseNativeImage(RemoteNativeImageWriteReference&&);
    void getNativeImagePixels(RemoteNativeImageReadReference&&, CompletionHandler<void(std::optional<WebCore::ShareableBitmap::Handle>&&)>&&);

    // A bitmap with the pixels of `image`, owned by the Web process. Replaces the platform image of `image` with
    // one that uses the bitmap, so that the pixels are not held twice.
    std::optional<WebCore::ShareableBitmap::Handle> createBitmapHandle(const WebCore::NativeImage&);

    const Ref<IPC::Connection> m_connection;
    const Ref<WorkQueue> m_workQueue;
    bool m_isClosed { false };

    IPC::ThreadSafeObjectHeap<RemoteSerializedImageBufferIdentifier, RefPtr<WebCore::ImageBuffer>> m_serializedImageBuffers;
    IPC::ThreadSafeObjectHeap<WebCore::RenderingResourceIdentifier, RefPtr<WebCore::NativeImage>> m_nativeImages;

    WebCore::ProcessIdentity m_resourceOwner;
#if HAVE(IOSURFACE)
    const Ref<WebCore::IOSurfacePool> m_ioSurfacePool;
#endif
    std::atomic<size_t> m_acceleratedImageBufferForCanvasCount;
    std::atomic<size_t> m_imageBufferForCanvasCount;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
