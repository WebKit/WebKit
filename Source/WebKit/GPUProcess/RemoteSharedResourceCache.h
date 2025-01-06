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

#include "MessageReceiver.h"
#include "RemoteSerializedImageBufferIdentifier.h"
#include "ThreadSafeObjectHeap.h"
#include <WebCore/ImageBuffer.h>
#include <WebCore/ImageBufferResourceLimits.h>
#include <WebCore/ProcessIdentity.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <wtf/FastMalloc.h>
#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>

#if USE(IOSURFACE)
#include <WebCore/IOSurfacePool.h>
#endif

namespace WebKit {

class GPUConnectionToWebProcess;
// Class holding GPU process resources per Web Content process.
// Thread-safe.
class RemoteSharedResourceCache final : public ThreadSafeRefCounted<RemoteSharedResourceCache>, IPC::MessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(RemoteSharedResourceCache);
public:
    static Ref<RemoteSharedResourceCache> create(GPUConnectionToWebProcess&);
    virtual ~RemoteSharedResourceCache();

    void ref() const final { ThreadSafeRefCounted::ref(); }
    void deref() const final { ThreadSafeRefCounted::deref(); }

    void addSerializedImageBuffer(WebCore::RenderingResourceIdentifier, Ref<WebCore::ImageBuffer>);
    RefPtr<WebCore::ImageBuffer> takeSerializedImageBuffer(WebCore::RenderingResourceIdentifier);

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    const WebCore::ProcessIdentity& resourceOwner() const { return m_resourceOwner; }
#if HAVE(IOSURFACE)
    WebCore::IOSurfacePool& ioSurfacePool() const { return m_ioSurfacePool; }
#endif

    void didCreateImageBuffer(WebCore::RenderingPurpose, WebCore::RenderingMode);
    void didReleaseImageBuffer(WebCore::RenderingPurpose, WebCore::RenderingMode);
    bool reachedAcceleratedImageBufferLimit(WebCore::RenderingPurpose) const;
    bool reachedImageBufferForCanvasLimit() const;
    WebCore::ImageBufferResourceLimits getResourceLimitsForTesting() const;

    void lowMemoryHandler();

private:
    RemoteSharedResourceCache(GPUConnectionToWebProcess&);

    // Messages
    void releaseSerializedImageBuffer(WebCore::RenderingResourceIdentifier);

    IPC::ThreadSafeObjectHeap<RemoteSerializedImageBufferIdentifier, RefPtr<WebCore::ImageBuffer>> m_serializedImageBuffers;
    WebCore::ProcessIdentity m_resourceOwner;
#if HAVE(IOSURFACE)
    Ref<WebCore::IOSurfacePool> m_ioSurfacePool;
#endif
    std::atomic<size_t> m_acceleratedImageBufferForCanvasCount;
    std::atomic<size_t> m_imageBufferForCanvasCount;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
