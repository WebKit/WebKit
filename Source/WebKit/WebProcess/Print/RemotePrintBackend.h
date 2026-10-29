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

#include "RemoteDisplayListIdentifier.h"
#include "RemoteDisplayListRecorderIdentifier.h"
#include "RemoteDrawingBackend.h"
#include "RemoteGradientIdentifier.h"
#include "RemotePathImplIdentifier.h"
#include "RemoteRenderingBackendIdentifier.h"
#include "RemoteResourceCache.h"
#include "RemoteSnapshotIdentifier.h"
#include "RemoteSnapshotRecorderIdentifier.h"
#include "ScopedActiveMessageReceiveQueue.h"
#include "SharedPreferencesForWebProcess.h"
#include "StreamConnectionWorkQueue.h"
#include "StreamServerConnection.h"
#include <WebCore/FontBase.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/ProcessIdentity.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <WebCore/ShareableBitmap.h>
#include <wtf/HashMap.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {
class Filter;
class FloatSize;
class Gradient;
class PathImpl;
struct FontCustomPlatformSerializedData;
struct FontPlatformDataAttributes;
}

namespace WebKit {

class PrintConnectionToWebProcess;
class PrintPDFDocument;
class RemoteDisplayListRecorder;
class RemoteSnapshotRecorder;
class RemoteSnapshotRegistry;

// In the print process, records the frames of print snapshots that a web process draws. Unlike a
// RemoteRenderingBackend, it has no image buffers and sends nothing back to the web process, since a
// print snapshot combines frames from several sites.
class RemotePrintBackend final : public IPC::StreamServerConnection::Client, public RemoteDrawingBackend {
    WTF_MAKE_TZONE_ALLOCATED(RemotePrintBackend);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RemotePrintBackend);
public:
    static Ref<RemotePrintBackend> create(PrintConnectionToWebProcess&, RemoteRenderingBackendIdentifier, Ref<IPC::StreamServerConnection>&&);
    ~RemotePrintBackend();
    void stopListeningForIPC();

    // RemoteDrawingBackend.
    void ref() const final { IPC::StreamServerConnection::Client::ref(); }
    void deref() const final { IPC::StreamServerConnection::Client::deref(); }
    IPC::StreamServerConnection& streamConnection() const final { return m_streamConnection.get(); }
    IPC::StreamConnectionWorkQueue& workQueue() const final { return m_workQueue; }
    RemoteResourceCache& remoteResourceCache() LIFETIME_BOUND final { return m_remoteResourceCache; }
    RefPtr<WebCore::ImageBuffer> imageBuffer(WebCore::RenderingResourceIdentifier) final { return nullptr; }
    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const final { return m_sharedPreferencesForWebProcess; }
    std::unique_ptr<WebCore::ImageBufferAllocator> createFilterImageBufferAllocator() const final;
    Ref<WebCore::ControlFactory> createControlFactory() const final;
#if ENABLE(VIDEO)
    std::unique_ptr<SharedVideoFrameReader> createSharedVideoFrameReader() const final;
#endif

    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder&) final;

private:
    RemotePrintBackend(PrintConnectionToWebProcess&, RemoteRenderingBackendIdentifier, Ref<IPC::StreamServerConnection>&&);
    void startListeningForIPC();
    void workQueueInitialize();
    void workQueueUninitialize();

    // IPC::StreamServerConnection::Client.
    void didReceiveInvalidMessage(IPC::StreamServerConnection&, IPC::MessageName, const Vector<uint32_t>&) final;

    // Messages.
    void createSnapshot(RemoteSnapshotIdentifier, WebCore::FrameIdentifier rootFrameIdentifier, const WebCore::FloatSize&, CompletionHandler<void()>&&);
    void createSnapshotRecorder(RemoteSnapshotRecorderIdentifier, RemoteSnapshotIdentifier);
    void sinkSnapshotRecorderIntoSnapshotFrame(RemoteSnapshotRecorderIdentifier, WebCore::FrameIdentifier, CompletionHandler<void(bool)>&&);
    void createDisplayListRecorder(RemoteDisplayListRecorderIdentifier);
    void sinkDisplayListRecorderIntoDisplayList(RemoteDisplayListRecorderIdentifier, RemoteDisplayListIdentifier);
    void releaseDisplayList(RemoteDisplayListIdentifier);
    void cacheNativeImage(WebCore::ShareableBitmap::Handle&&, WebCore::RenderingResourceIdentifier);
    void releaseNativeImage(WebCore::RenderingResourceIdentifier);
    void cachePathImpl(Ref<WebCore::PathImpl>&&, RemotePathImplIdentifier);
    void releasePathImpl(RemotePathImplIdentifier);
    void cacheFont(const WebCore::FontInternalAttributes&, WebCore::FontPlatformDataAttributes, std::optional<WebCore::RenderingResourceIdentifier>);
    void releaseFont(WebCore::RenderingResourceIdentifier);
    void cacheFontCustomPlatformData(WebCore::FontCustomPlatformSerializedData&&);
    void releaseFontCustomPlatformData(WebCore::RenderingResourceIdentifier);
    void cacheGradient(Ref<WebCore::Gradient>&&, RemoteGradientIdentifier);
    void releaseGradient(RemoteGradientIdentifier);
    void cacheFilter(Ref<WebCore::Filter>&&);
    void releaseFilter(WebCore::RenderingResourceIdentifier);
    void cachePDFDocument(WebCore::RenderingResourceIdentifier, WebCore::SharedMemory::Handle&&);
    void releasePDFDocument(WebCore::RenderingResourceIdentifier);
    void drawPDFPage(RemoteSnapshotRecorderIdentifier, WebCore::RenderingResourceIdentifier documentIdentifier, uint32_t pageIndex, const WebCore::FloatRect& source, const WebCore::FloatRect& destination, bool includeLinks);
    void releaseMemory();

    bool shouldUseLockdownFontParser() const;

    const Ref<IPC::StreamConnectionWorkQueue> m_workQueue;
    const Ref<IPC::StreamServerConnection> m_streamConnection;
    const RemoteRenderingBackendIdentifier m_identifier;
    const WebCore::ProcessIdentifier m_webProcessIdentifier;
    // Taken on the main thread at construction and only read on the work queue.
    const SharedPreferencesForWebProcess m_sharedPreferencesForWebProcess;
    const bool m_isLockdownModeEnabled;
    const Ref<RemoteSnapshotRegistry> m_snapshotRegistry;
    RemoteResourceCache m_remoteResourceCache;
    HashMap<RemoteDisplayListRecorderIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteDisplayListRecorder>> m_remoteDisplayListRecorders WTF_GUARDED_BY_CAPABILITY(workQueue());
    HashMap<RemoteSnapshotRecorderIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteSnapshotRecorder>> m_remoteSnapshotRecorders WTF_GUARDED_BY_CAPABILITY(workQueue());
    // Null for data that is not a PDF document, whose pages draw nothing.
    HashMap<WebCore::RenderingResourceIdentifier, RefPtr<PrintPDFDocument>> m_pdfDocuments WTF_GUARDED_BY_CAPABILITY(workQueue());
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && PLATFORM(COCOA)
