/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "RemoteDisplayListIdentifier.h"
#include "RemoteGradientIdentifier.h"
#include "RemotePathImplIdentifier.h"
#include "RemoteSnapshotIdentifier.h"
#include <WebCore/FrameIdentifier.h>
#include <WebCore/RenderingMode.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <optional>
#include <wtf/AbstractCanMakeCheckedPtr.h>
#include <wtf/AbstractRefCountedAndCanMakeWeakPtr.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/UniqueRef.h>

namespace IPC {
class StreamClientConnection;
}

namespace WebCore {
class ColorSpace;
class Filter;
class FloatRect;
class FloatSize;
class Gradient;
class ImageBuffer;
class PathImpl;
class ShareableBitmap;
class ShareableBitmapHandle;
struct FontCustomPlatformData;
struct FontInternalAttributes;
struct FontPlatformDataAttributes;
struct ImageBufferFormat;

namespace DisplayList {
class DisplayList;
}
}

namespace WebKit {

class RemoteImageBufferProxy;
class RemoteNativeImageProxy;
class RemoteResourceCacheProxy;
class RemoteSnapshotRecorderProxy;

// The backend proxy that RemoteGraphicsContextProxy and RemoteResourceCacheProxy record for: the
// stream the drawing goes to, the resources sent ahead of it, and the image buffers that contexts
// create while drawing.
class RemoteDrawingBackendProxy : public AbstractRefCountedAndCanMakeWeakPtr<RemoteDrawingBackendProxy>, public AbstractCanMakeCheckedPtr {
public:
    virtual ~RemoteDrawingBackendProxy() = default;

    virtual RefPtr<IPC::StreamClientConnection> connection() = 0;
    virtual void didBecomeUnresponsive() = 0;
    virtual RemoteResourceCacheProxy& remoteResourceCacheProxy() const = 0;
    // Whether the backend is in the GPU process, so that it can use the native images and video
    // frames that the GPU process shares, and IOSurfaces.
    virtual bool canUseGPUProcessResources() const = 0;

    // The remote image buffer of this backend for the image buffer, if there is one.
    virtual RefPtr<RemoteImageBufferProxy> cachedImageBuffer(const WebCore::ImageBuffer&) const = 0;
    // For the image buffers that contexts create while drawing.
    virtual RefPtr<WebCore::ImageBuffer> createImageBufferForContext(const WebCore::FloatSize&, WebCore::RenderingMode, WebCore::RenderingPurpose, float resolutionScale, const WebCore::ColorSpace&, WebCore::ImageBufferFormat) = 0;

    virtual void cacheNativeImage(WebCore::ShareableBitmapHandle&&, WebCore::RenderingResourceIdentifier) = 0;
    virtual void releaseNativeImage(WebCore::RenderingResourceIdentifier) = 0;
    // Only for the RemoteNativeImageProxies in this backend's resource cache.
    virtual RefPtr<WebCore::ShareableBitmap> nativeImageBitmap(const RemoteNativeImageProxy&) = 0;
    virtual void cachePathImpl(Ref<WebCore::PathImpl>&&, RemotePathImplIdentifier) = 0;
    virtual void releasePathImpl(RemotePathImplIdentifier) = 0;
    virtual void cacheFont(const WebCore::FontInternalAttributes&, const WebCore::FontPlatformDataAttributes&, std::optional<WebCore::RenderingResourceIdentifier>) = 0;
    virtual void releaseFont(WebCore::RenderingResourceIdentifier) = 0;
    virtual void cacheFontCustomPlatformData(Ref<const WebCore::FontCustomPlatformData>&&) = 0;
    virtual void releaseFontCustomPlatformData(WebCore::RenderingResourceIdentifier) = 0;
    virtual void cacheGradient(Ref<WebCore::Gradient>&&, RemoteGradientIdentifier) = 0;
    virtual void releaseGradient(RemoteGradientIdentifier) = 0;
    virtual void cacheFilter(Ref<WebCore::Filter>&&) = 0;
    virtual void releaseFilter(WebCore::RenderingResourceIdentifier) = 0;
    virtual void cacheDisplayList(RemoteDisplayListIdentifier, const WebCore::DisplayList::DisplayList&) = 0;
    virtual void releaseDisplayList(RemoteDisplayListIdentifier) = 0;

    // Remote snapshots, which the backend's process assembles.
    virtual void createSnapshot(RemoteSnapshotIdentifier, WebCore::FrameIdentifier rootFrameIdentifier, const WebCore::FloatSize&) = 0;
    virtual UniqueRef<RemoteSnapshotRecorderProxy> createSnapshotRecorder(const WebCore::FloatRect& initialClip, RemoteSnapshotIdentifier, WebCore::RenderingMode) = 0;
    virtual void sinkSnapshotRecorderIntoSnapshotFrame(UniqueRef<RemoteSnapshotRecorderProxy>&&, WebCore::FrameIdentifier, CompletionHandler<void(bool)>&&) = 0;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
