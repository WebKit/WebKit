/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(GPU_PROCESS)

#include "GPUProcessConnection.h"
#include "GPUProcessWakeupMessageArguments.h"
#include "IPCSemaphore.h"
#include "ImageBufferBackendHandle.h"
#include "MessageReceiver.h"
#include "MessageSender.h"
#include "RemoteRenderingBackendCreationParameters.h"
#include "RemoteResourceCacheProxy.h"
#include "RenderingBackendIdentifier.h"
#include <WebCore/DisplayList.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <WebCore/Timer.h>
#include <wtf/Deque.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
namespace DisplayList {
class DisplayList;
class Item;
}
class FloatSize;
class ImageData;
enum class AlphaPremultiplication : uint8_t;
enum class DestinationColorSpace : uint8_t;
enum class RenderingMode : bool;
}

namespace WebKit {

class DisplayListWriterHandle;
class WebPage;

class RemoteRenderingBackendProxy
    : public IPC::MessageSender
    , private IPC::MessageReceiver
    , public GPUProcessConnection::Client {
public:
    static std::unique_ptr<RemoteRenderingBackendProxy> create(WebPage&);

    ~RemoteRenderingBackendProxy();

    using GPUProcessConnection::Client::weakPtrFactory;
    using WeakValueType = GPUProcessConnection::Client::WeakValueType;

    RemoteResourceCacheProxy& remoteResourceCacheProxy() { return m_remoteResourceCacheProxy; }
    WebCore::DisplayList::ItemBufferHandle createItemBuffer(size_t capacity, WebCore::RenderingResourceIdentifier destinationBufferIdentifier);

    void didAppendData(const WebCore::DisplayList::ItemBufferHandle&, size_t numberOfBytes, WebCore::DisplayList::DidChangeItemBuffer, WebCore::RenderingResourceIdentifier destinationImageBuffer);
    void willAppendItem(WebCore::RenderingResourceIdentifier);
    void sendDeferredWakeupMessageIfNeeded();

    SharedMemory* sharedMemoryForGetImageData(size_t dataSize, IPC::Timeout);
    bool waitForGetImageDataToComplete(IPC::Timeout);
    void destroyGetImageDataSharedMemory();

    // IPC::MessageSender.
    IPC::Connection* messageSenderConnection() const override;
    uint64_t messageSenderDestinationID() const override;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // Messages to be sent.
    RefPtr<WebCore::ImageBuffer> createImageBuffer(const WebCore::FloatSize&, WebCore::RenderingMode, float resolutionScale, WebCore::DestinationColorSpace, WebCore::PixelFormat);
    String getDataURLForImageBuffer(const String& mimeType, Optional<double> quality, WebCore::PreserveResolution, WebCore::RenderingResourceIdentifier);
    Vector<uint8_t> getDataForImageBuffer(const String& mimeType, Optional<double> quality, WebCore::RenderingResourceIdentifier);
    Vector<uint8_t> getBGRADataForImageBuffer(WebCore::RenderingResourceIdentifier);
    WebCore::DisplayList::FlushIdentifier flushDisplayListAndCommit(const WebCore::DisplayList::DisplayList&, WebCore::RenderingResourceIdentifier);
    RefPtr<ShareableBitmap> getShareableBitmap(WebCore::RenderingResourceIdentifier, WebCore::PreserveResolution);
    void cacheNativeImage(const ShareableBitmap::Handle&, WebCore::RenderingResourceIdentifier);
    void cacheFont(Ref<WebCore::Font>&&);
    void deleteAllFonts();
    void releaseRemoteResource(WebCore::RenderingResourceIdentifier);

    enum class DidReceiveBackendCreationResult : bool {
        ReceivedAnyResponse,
        TimeoutOrIPCFailure
    };
    DidReceiveBackendCreationResult waitForDidCreateImageBufferBackend();
    bool waitForDidFlush();

    RenderingBackendIdentifier renderingBackendIdentifier() const;

    RenderingBackendIdentifier ensureBackendCreated();

private:
    explicit RemoteRenderingBackendProxy(WebPage&);

    // GPUProcessConnection::Client
    void gpuProcessConnectionDidClose(GPUProcessConnection&) final;

    GPUProcessConnection& ensureGPUProcessConnection();

    // Messages to be received.
    void didCreateImageBufferBackend(ImageBufferBackendHandle, WebCore::RenderingResourceIdentifier);
    void didFlush(WebCore::DisplayList::FlushIdentifier, WebCore::RenderingResourceIdentifier);

    RefPtr<DisplayListWriterHandle> mostRecentlyUsedDisplayListHandle();
    RefPtr<DisplayListWriterHandle> findReusableDisplayListHandle(size_t capacity);

    void sendWakeupMessage(const GPUProcessWakeupMessageArguments&);

    RemoteRenderingBackendCreationParameters m_parameters;
    RemoteResourceCacheProxy m_remoteResourceCacheProxy { *this };
    HashMap<WebCore::DisplayList::ItemBufferIdentifier, RefPtr<DisplayListWriterHandle>> m_sharedDisplayListHandles;
    Deque<WebCore::DisplayList::ItemBufferIdentifier> m_identifiersOfReusableHandles;
    Optional<WebCore::RenderingResourceIdentifier> m_currentDestinationImageBufferIdentifier;
    Optional<GPUProcessWakeupMessageArguments> m_deferredWakeupMessageArguments;
    unsigned m_remainingItemsToAppendBeforeSendingWakeup { 0 };
    Optional<IPC::Semaphore> m_getImageDataSemaphore;
    RefPtr<SharedMemory> m_getImageDataSharedMemory;
    uint64_t m_getImageDataSharedMemoryLength { 0 };
    WebCore::Timer m_destroyGetImageDataSharedMemoryTimer { *this, &RemoteRenderingBackendProxy::destroyGetImageDataSharedMemory };
    WeakPtr<GPUProcessConnection> m_gpuProcessConnection;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
