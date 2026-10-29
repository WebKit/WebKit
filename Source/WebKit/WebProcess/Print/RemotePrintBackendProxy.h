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

#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA)

#include "Connection.h"
#include "RemoteDrawingBackendProxy.h"
#include "RemoteRenderingBackendIdentifier.h"
#include "RemoteResourceCacheProxy.h"
#include "RemoteSnapshotIdentifier.h"
#include <WebCore/FrameIdentifier.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/UniqueRef.h>

namespace WebCore {
class FloatRect;
}

namespace WebKit {

class PrintProcessConnection;
class RemoteSnapshotRecorderProxy;

// In a web process, records frames of print snapshots into the print process. Separate from the
// page's RemoteRenderingBackendProxy, since canvases, WebGL and WebGPU stay in the GPU process. It
// is not recreated if the print process goes away: the print snapshots fail then.
class RemotePrintBackendProxy final : public RefCounted<RemotePrintBackendProxy>, public IPC::Connection::Client, public RemoteDrawingBackendProxy {
    WTF_MAKE_TZONE_ALLOCATED(RemotePrintBackendProxy);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RemotePrintBackendProxy);
public:
    static Ref<RemotePrintBackendProxy> create(PrintProcessConnection&);
    ~RemotePrintBackendProxy();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }
    USING_CAN_MAKE_WEAKPTR(IPC::MessageReceiver);
    OVERRIDE_ABSTRACT_CAN_MAKE_CHECKEDPTR(IPC::Connection::Client);

    // Called when the connection to the print process closes.
    void disconnect();

    void createSnapshot(RemoteSnapshotIdentifier, WebCore::FrameIdentifier rootFrameIdentifier, const WebCore::FloatSize&) final;
    UniqueRef<RemoteSnapshotRecorderProxy> createSnapshotRecorder(const WebCore::FloatRect& initialClip, RemoteSnapshotIdentifier, WebCore::RenderingMode) final;
    void sinkSnapshotRecorderIntoSnapshotFrame(UniqueRef<RemoteSnapshotRecorderProxy>&&, WebCore::FrameIdentifier, CompletionHandler<void(bool)>&&) final;
    bool canDrawPDFPages() const final { return true; }
    bool cachePDFDocument(WebCore::RenderingResourceIdentifier, const WebCore::FragmentedSharedBuffer&) final;
    void releasePDFDocument(WebCore::RenderingResourceIdentifier) final;
    void drawPDFPage(RemoteSnapshotRecorderIdentifier, WebCore::RenderingResourceIdentifier documentIdentifier, unsigned pageIndex, const WebCore::FloatRect& source, const WebCore::FloatRect& destination, bool includeLinks) final;
    void releaseMemory();

    // RemoteDrawingBackendProxy.
    RefPtr<IPC::StreamClientConnection> connection() final;
    void didBecomeUnresponsive() final;
    RemoteResourceCacheProxy& remoteResourceCacheProxy() const final { return m_remoteResourceCacheProxy; }
    bool canUseGPUProcessResources() const final { return false; }
    RefPtr<RemoteImageBufferProxy> cachedImageBuffer(const WebCore::ImageBuffer&) const final { return nullptr; }
    RefPtr<WebCore::ImageBuffer> createImageBufferForContext(const WebCore::FloatSize&, WebCore::RenderingMode, WebCore::RenderingPurpose, float resolutionScale, const WebCore::ColorSpace&, WebCore::ImageBufferFormat) final;
    void cacheNativeImage(WebCore::ShareableBitmapHandle&&, WebCore::RenderingResourceIdentifier) final;
    void releaseNativeImage(WebCore::RenderingResourceIdentifier) final;
    RefPtr<WebCore::ShareableBitmap> nativeImageBitmap(const RemoteNativeImageProxy&) final { return nullptr; }
    void cachePathImpl(Ref<WebCore::PathImpl>&&, RemotePathImplIdentifier) final;
    void releasePathImpl(RemotePathImplIdentifier) final;
    void cacheFont(const WebCore::FontInternalAttributes&, const WebCore::FontPlatformDataAttributes&, std::optional<WebCore::RenderingResourceIdentifier>) final;
    void releaseFont(WebCore::RenderingResourceIdentifier) final;
    void cacheFontCustomPlatformData(Ref<const WebCore::FontCustomPlatformData>&&) final;
    void releaseFontCustomPlatformData(WebCore::RenderingResourceIdentifier) final;
    void cacheGradient(Ref<WebCore::Gradient>&&, RemoteGradientIdentifier) final;
    void releaseGradient(RemoteGradientIdentifier) final;
    void cacheFilter(Ref<WebCore::Filter>&&) final;
    void releaseFilter(WebCore::RenderingResourceIdentifier) final;
    void cacheDisplayList(RemoteDisplayListIdentifier, const WebCore::DisplayList::DisplayList&) final;
    void releaseDisplayList(RemoteDisplayListIdentifier) final;

private:
    explicit RemotePrintBackendProxy(PrintProcessConnection&);

    void ensureBackendCreated();
    template<typename T> void send(T&& message);

    // IPC::Connection::Client
    void didClose(IPC::Connection&) final;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName, const Vector<uint32_t>& indicesOfObjectsFailingDecoding) final { }
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) final;

    // The connection the print backend is created and released on.
    RefPtr<IPC::Connection> m_printProcessConnection;
    RefPtr<IPC::StreamClientConnection> m_connection;
    const RemoteRenderingBackendIdentifier m_identifier { RemoteRenderingBackendIdentifier::generate() };
    const UniqueRef<RemoteResourceCacheProxy> m_remoteResourceCacheProxy;
    bool m_isResponsive { true };
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && PLATFORM(COCOA)
