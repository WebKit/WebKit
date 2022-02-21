/*
 * Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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
#include "IPCSemaphore.h"
#include "ImageBufferBackendHandle.h"
#include "MessageReceiver.h"
#include "MessageSender.h"
#include "QualifiedRenderingResourceIdentifier.h"
#include "RemoteDisplayListRecorder.h"
#include "RemoteResourceCache.h"
#include "RenderingBackendIdentifier.h"
#include "RenderingUpdateID.h"
#include "ScopedRenderingResourcesRequest.h"
#include "StreamConnectionWorkQueue.h"
#include "StreamMessageReceiver.h"
#include "StreamServerConnection.h"
#include <WebCore/ColorSpace.h>
#include <WebCore/DisplayList.h>
#include <WebCore/DisplayListItems.h>
#include <WebCore/DisplayListReplayer.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/ProcessIdentity.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class DestinationColorSpace;
class FloatSize;
class NativeImage;

enum class RenderingMode : bool;

}

namespace IPC {
class FilterReference;
}

namespace WebKit {

class GPUConnectionToWebProcess;
class RemoteDisplayListRecorder;
struct BufferIdentifierSet;
struct RemoteRenderingBackendCreationParameters;

class RemoteRenderingBackend : private IPC::MessageSender, public IPC::StreamMessageReceiver {
public:
    static Ref<RemoteRenderingBackend> create(GPUConnectionToWebProcess&, RemoteRenderingBackendCreationParameters&&, IPC::StreamConnectionBuffer&&);
    virtual ~RemoteRenderingBackend();
    void stopListeningForIPC();

    RemoteResourceCache& remoteResourceCache() { return m_remoteResourceCache; }

    void didReceiveStreamMessage(IPC::StreamServerConnectionBase&, IPC::Decoder&) final;

    // Messages to be sent.
    void didCreateImageBufferBackend(ImageBufferBackendHandle, QualifiedRenderingResourceIdentifier, RemoteDisplayListRecorder&);
    void didFlush(WebCore::GraphicsContextFlushIdentifier, QualifiedRenderingResourceIdentifier);

    bool allowsExitUnderMemoryPressure() const;

    // Runs Function in RemoteRenderingBackend task queue.
    void dispatch(Function<void()>&&);

    IPC::StreamServerConnection& streamConnection() const { return m_streamConnection.get(); }
    void performWithMediaPlayerOnMainThread(WebCore::MediaPlayerIdentifier, Function<void(WebCore::MediaPlayer&)>&&);

private:
    RemoteRenderingBackend(GPUConnectionToWebProcess&, RemoteRenderingBackendCreationParameters&&, IPC::StreamConnectionBuffer&&);
    void startListeningForIPC();

    std::optional<SharedMemory::IPCHandle> updateSharedMemoryForGetPixelBufferHelper(size_t byteCount);
    void updateRenderingResourceRequest();

    // IPC::MessageSender.
    IPC::Connection* messageSenderConnection() const override;
    uint64_t messageSenderDestinationID() const override;

    // Messages to be received.
    void createImageBuffer(const WebCore::FloatSize& logicalSize, WebCore::RenderingMode, float resolutionScale, const WebCore::DestinationColorSpace&, WebCore::PixelFormat, WebCore::RenderingResourceIdentifier);
    void getPixelBufferForImageBuffer(WebCore::RenderingResourceIdentifier, WebCore::PixelBufferFormat&&, WebCore::IntRect&& srcRect, CompletionHandler<void()>&&);
    void getPixelBufferForImageBufferWithNewMemory(WebCore::RenderingResourceIdentifier, SharedMemory::IPCHandle&&, WebCore::PixelBufferFormat&& destinationFormat, WebCore::IntRect&& srcRect, CompletionHandler<void()>&&);
    void destroyGetPixelBufferSharedMemory();
    void putPixelBufferForImageBuffer(WebCore::RenderingResourceIdentifier, WebCore::PixelBuffer&&, WebCore::IntRect&& srcRect, WebCore::IntPoint&& destPoint, WebCore::AlphaPremultiplication destFormat);
    void getDataURLForImageBuffer(const String& mimeType, std::optional<double> quality, WebCore::PreserveResolution, WebCore::RenderingResourceIdentifier, CompletionHandler<void(String&&)>&&);
    void getDataForImageBuffer(const String& mimeType, std::optional<double> quality, WebCore::RenderingResourceIdentifier, CompletionHandler<void(Vector<uint8_t>&&)>&&);
    void getShareableBitmapForImageBuffer(WebCore::RenderingResourceIdentifier, WebCore::PreserveResolution, CompletionHandler<void(ShareableBitmap::Handle&&)>&&);
    void getFilteredImageForImageBuffer(WebCore::RenderingResourceIdentifier, IPC::FilterReference&&, CompletionHandler<void(ShareableBitmap::Handle&&)>&&);
    void cacheNativeImage(const ShareableBitmap::Handle&, WebCore::RenderingResourceIdentifier);
    void cacheFont(Ref<WebCore::Font>&&);
    void deleteAllFonts();
    void releaseRemoteResource(WebCore::RenderingResourceIdentifier);
    void finalizeRenderingUpdate(RenderingUpdateID);
    void markSurfacesVolatile(const Vector<WebCore::RenderingResourceIdentifier>&, CompletionHandler<void(const Vector<WebCore::RenderingResourceIdentifier>& inUseBufferIdentifiers)>&&);

    void markSurfaceNonVolatile(WebCore::RenderingResourceIdentifier, CompletionHandler<void(std::optional<ImageBufferBackendHandle>, bool bufferWasEmpty)>&&);
    void swapToValidFrontBuffer(const BufferIdentifierSet& bufferSet, CompletionHandler<void(const BufferIdentifierSet& swappedBufferSet, std::optional<ImageBufferBackendHandle>&& frontBufferHandle, bool frontBufferWasEmpty)>&&);

    // Received messages translated to use QualifiedRenderingResourceIdentifier.
    void createImageBufferWithQualifiedIdentifier(const WebCore::FloatSize& logicalSize, WebCore::RenderingMode, float resolutionScale, const WebCore::DestinationColorSpace&, WebCore::PixelFormat, QualifiedRenderingResourceIdentifier);
    void getDataURLForImageBufferWithQualifiedIdentifier(const String& mimeType, std::optional<double> quality, WebCore::PreserveResolution, QualifiedRenderingResourceIdentifier, CompletionHandler<void(String&&)>&&);
    void getDataForImageBufferWithQualifiedIdentifier(const String& mimeType, std::optional<double> quality, QualifiedRenderingResourceIdentifier, CompletionHandler<void(Vector<uint8_t>&&)>&&);
    void getShareableBitmapForImageBufferWithQualifiedIdentifier(QualifiedRenderingResourceIdentifier, WebCore::PreserveResolution, CompletionHandler<void(ShareableBitmap::Handle&&)>&&);
    void cacheNativeImageWithQualifiedIdentifier(const ShareableBitmap::Handle&, QualifiedRenderingResourceIdentifier);
    void didCreateSharedDisplayListHandleWithQualifiedIdentifier(WebCore::DisplayList::ItemBufferIdentifier, const SharedMemory::IPCHandle&, QualifiedRenderingResourceIdentifier destinationBufferIdentifier);
    void releaseRemoteResourceWithQualifiedIdentifier(QualifiedRenderingResourceIdentifier);
    void cacheFontWithQualifiedIdentifier(Ref<WebCore::Font>&&, QualifiedRenderingResourceIdentifier);

    Ref<IPC::StreamConnectionWorkQueue> m_workQueue;
    Ref<IPC::StreamServerConnection> m_streamConnection;
    RemoteResourceCache m_remoteResourceCache;
    Ref<GPUConnectionToWebProcess> m_gpuConnectionToWebProcess;
    WebCore::ProcessIdentity m_resourceOwner;
    RenderingBackendIdentifier m_renderingBackendIdentifier;
    RefPtr<SharedMemory> m_getPixelBufferSharedMemory;
    ScopedRenderingResourcesRequest m_renderingResourcesRequest;

    Lock m_remoteDisplayListsLock;
    bool m_canRegisterRemoteDisplayLists WTF_GUARDED_BY_LOCK(m_remoteDisplayListsLock) { false };
    HashMap<QualifiedRenderingResourceIdentifier, Ref<RemoteDisplayListRecorder>> m_remoteDisplayLists WTF_GUARDED_BY_LOCK(m_remoteDisplayListsLock);
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
