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
#include "RemoteNativeImageIdentifier.h"
#include "RemoteSerializedImageBufferIdentifier.h"
#include "ThreadSafeObjectHeap.h"
#include <WebCore/ImageBuffer.h>
#include <WebCore/ImageBufferResourceLimits.h>
#include <WebCore/NativeImage.h>
#include <WebCore/ProcessIdentity.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <WebCore/ShareableBitmap.h>
#include <wtf/CompletionHandler.h>
#include <wtf/FastMalloc.h>
#include <wtf/HashMap.h>
#include <wtf/Ref.h>
#include <wtf/Seconds.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>

#if USE(IOSURFACE)
#include <WebCore/IOSurfacePool.h>
#endif

namespace WebKit {

// Timeout for waiting on a resource to be published in the shared cache from another work queue.
constexpr Seconds defaultRemoteSharedResourceCacheTimeout = 15_s;

// Per Web Content process budget for decoded thumbnail strips.
constexpr size_t thumbnailStripCacheLimit = 32 * MB;

// A decoded thumbnail strip, kept so that repeated scrubbing over the same
// media element does not re-decode the same image once per tile.
struct CachedThumbnailStrip {
    WebCore::IntSize tileSize;
    uint32_t tileCount { 0 };
    Vector<Ref<WebCore::NativeImage>> tiles;
    size_t byteCount { 0 };

    template<typename Encoder> void encode(Encoder& encoder) const
    {
        encoder << tileSize;
        encoder << tileCount;
        encoder << byteCount;
    }

    template<typename Decoder> static std::optional<CachedThumbnailStrip> decode(Decoder& decoder)
    {
        std::optional<WebCore::IntSize> tileSize;
        decoder >> tileSize;
        if (!tileSize)
            return std::nullopt;

        std::optional<uint32_t> tileCount;
        decoder >> tileCount;
        if (!tileCount)
            return std::nullopt;

        std::optional<size_t> byteCount;
        decoder >> byteCount;
        if (!byteCount)
            return std::nullopt;

        return CachedThumbnailStrip { *tileSize, *tileCount, { }, *byteCount };
    }
};

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

    bool addSerializedImageBuffer(RemoteSerializedImageBufferIdentifier, Ref<WebCore::ImageBuffer>);
    RefPtr<WebCore::ImageBuffer> takeSerializedImageBuffer(RemoteSerializedImageBufferIdentifier);

    bool addNativeImage(RemoteNativeImageReference, Ref<WebCore::NativeImage>);
    RefPtr<WebCore::NativeImage> readNativeImage(RemoteNativeImageReadReference&&, Seconds timeout);

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    const WebCore::ProcessIdentity& resourceOwner() const LIFETIME_BOUND { return m_resourceOwner; }
#if HAVE(IOSURFACE)
    WebCore::IOSurfacePool& ioSurfacePool() const { return m_ioSurfacePool; }
#endif

    void NODELETE didCreateImageBuffer(WebCore::RenderingPurpose, WebCore::RenderingMode);
    void NODELETE didReleaseImageBuffer(WebCore::RenderingPurpose, WebCore::RenderingMode);
    bool NODELETE reachedAcceleratedImageBufferLimit(WebCore::RenderingPurpose) const;
    bool NODELETE reachedImageBufferForCanvasLimit() const;
    WebCore::ImageBufferResourceLimits NODELETE getResourceLimitsForTesting() const;

    void lowMemoryHandler();

private:
    RemoteSharedResourceCache(GPUConnectionToWebProcess&);

    // Messages
    void releaseSerializedImageBuffer(RemoteSerializedImageBufferIdentifier);
    void releaseNativeImage(RemoteNativeImageWriteReference&&);
    void cacheEncodedThumbnailStrip(WebCore::RenderingResourceIdentifier, WebCore::IntSize tileSize, uint32_t tileCount, Vector<uint8_t>&& encodedData);
    void takeThumbnailTile(WebCore::RenderingResourceIdentifier, uint32_t tileIndex, CompletionHandler<void(std::optional<WebCore::ShareableBitmap::Handle>)>&&);

    void releaseUnusedThumbnailStrips();

    IPC::ThreadSafeObjectHeap<RemoteSerializedImageBufferIdentifier, RefPtr<WebCore::ImageBuffer>> m_serializedImageBuffers;
    IPC::ThreadSafeObjectHeap<WebCore::RenderingResourceIdentifier, RefPtr<WebCore::NativeImage>> m_nativeImages;
    HashMap<WebCore::RenderingResourceIdentifier, CachedThumbnailStrip*> m_thumbnailStrips;
    size_t m_thumbnailStripBytes { 0 };

    WebCore::ProcessIdentity m_resourceOwner;
#if HAVE(IOSURFACE)
    const Ref<WebCore::IOSurfacePool> m_ioSurfacePool;
#endif
    std::atomic<size_t> m_acceleratedImageBufferForCanvasCount;
    std::atomic<size_t> m_imageBufferForCanvasCount;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
