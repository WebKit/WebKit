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
#include "GPUProcessWakeupMessageArguments.h"
#include "IPCSemaphore.h"
#include "ImageBufferBackendHandle.h"
#include "MessageReceiver.h"
#include "MessageSender.h"
#include "RemoteRenderingBackendState.h"
#include "RemoteResourceCache.h"
#include "RenderingBackendIdentifier.h"
#include "RenderingUpdateID.h"
#include "ScopedRenderingResourcesRequest.h"
#include <WebCore/ColorSpace.h>
#include <WebCore/DisplayList.h>
#include <WebCore/DisplayListItems.h>
#include <WebCore/DisplayListReplayer.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

namespace DisplayList {
class DisplayList;
class Item;
}

class DestinationColorSpace;
class FloatSize;
class NativeImage;

enum class RenderingMode : bool;

}

namespace WebKit {

class DisplayListReaderHandle;
class GPUConnectionToWebProcess;
struct RemoteRenderingBackendCreationParameters;

class RemoteRenderingBackend
    : private IPC::MessageSender
    , public IPC::Connection::WorkQueueMessageReceiver
    , public WebCore::DisplayList::ItemBufferReadingClient {
public:

    static Ref<RemoteRenderingBackend> create(GPUConnectionToWebProcess&, RemoteRenderingBackendCreationParameters&&);
    virtual ~RemoteRenderingBackend();
    void stopListeningForIPC();

    RemoteResourceCache& remoteResourceCache() { return m_remoteResourceCache; }

    // Rendering operations.
    bool applyMediaItem(WebCore::DisplayList::ItemHandle, WebCore::GraphicsContext&);

    // Messages to be sent.
    void didCreateImageBufferBackend(ImageBufferBackendHandle, WebCore::RenderingResourceIdentifier);
    void didFlush(WebCore::DisplayList::FlushIdentifier, WebCore::RenderingResourceIdentifier);

    void setNextItemBufferToRead(WebCore::DisplayList::ItemBufferIdentifier, WebCore::RenderingResourceIdentifier destination);

    void didCreateMaskImageBuffer(WebCore::ImageBuffer&);
    void didResetMaskImageBuffer();

    void populateGetPixelBufferSharedMemory(std::optional<WebCore::PixelBuffer>&&);

    bool allowsExitUnderMemoryPressure() const;

    // Runs Function in RemoteRenderingBackend task queue.
    void dispatch(Function<void()>&&);

    RemoteRenderingBackendState lastKnownState() const;

private:
    RemoteRenderingBackend(GPUConnectionToWebProcess&, RemoteRenderingBackendCreationParameters&&);
    void startListeningForIPC();

    std::optional<WebCore::DisplayList::ItemHandle> WARN_UNUSED_RETURN decodeItem(const uint8_t* data, size_t length, WebCore::DisplayList::ItemType, uint8_t* handleLocation) override;

    template<typename T>
    std::optional<WebCore::DisplayList::ItemHandle> WARN_UNUSED_RETURN decodeAndCreate(const uint8_t* data, size_t length, uint8_t* handleLocation)
    {
        if (auto item = IPC::Decoder::decodeSingleObject<T>(data, length)) {
            // FIXME: WebKit2 should not need to know that the first 8 bytes at the handle are reserved for the type.
            // Need to figure out a way to keep this knowledge within display list code in WebCore.
            new (handleLocation + sizeof(uint64_t)) T(WTFMove(*item));
            return {{ handleLocation }};
        }
        return std::nullopt;
    }

    WebCore::DisplayList::ReplayResult submit(const WebCore::DisplayList::DisplayList&, WebCore::ImageBuffer& destination);
    RefPtr<WebCore::ImageBuffer> nextDestinationImageBufferAfterApplyingDisplayLists(WebCore::ImageBuffer& initialDestination, size_t initialOffset, DisplayListReaderHandle&, GPUProcessWakeupReason);

    std::optional<SharedMemory::IPCHandle> updateSharedMemoryForGetPixelBufferHelper(size_t byteCount);
    void updateRenderingResourceRequest();

    void updateLastKnownState(RemoteRenderingBackendState);

    // IPC::MessageSender.
    IPC::Connection* messageSenderConnection() const override;
    uint64_t messageSenderDestinationID() const override;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) override;

    // Messages to be received.
    void createImageBuffer(const WebCore::FloatSize& logicalSize, WebCore::RenderingMode, float resolutionScale, const WebCore::DestinationColorSpace&, WebCore::PixelFormat, WebCore::RenderingResourceIdentifier);
    void wakeUpAndApplyDisplayList(const GPUProcessWakeupMessageArguments&);
    void updateSharedMemoryForGetPixelBuffer(uint32_t byteCount, CompletionHandler<void(const SharedMemory::IPCHandle&)>&&);
    void semaphoreForGetPixelBuffer(CompletionHandler<void(const IPC::Semaphore&)>&&);
    void updateSharedMemoryAndSemaphoreForGetPixelBuffer(uint32_t byteCount, CompletionHandler<void(const SharedMemory::IPCHandle&, const IPC::Semaphore&)>&&);
    void destroyGetPixelBufferSharedMemory();
    void getDataURLForImageBuffer(const String& mimeType, std::optional<double> quality, WebCore::PreserveResolution, WebCore::RenderingResourceIdentifier, CompletionHandler<void(String&&)>&&);
    void getDataForImageBuffer(const String& mimeType, std::optional<double> quality, WebCore::RenderingResourceIdentifier, CompletionHandler<void(Vector<uint8_t>&&)>&&);
    void getShareableBitmapForImageBuffer(WebCore::RenderingResourceIdentifier, WebCore::PreserveResolution, CompletionHandler<void(ShareableBitmap::Handle&&)>&&);
    void cacheNativeImage(const ShareableBitmap::Handle&, WebCore::RenderingResourceIdentifier);
    void cacheFont(Ref<WebCore::Font>&&);
    void deleteAllFonts();
    void releaseRemoteResource(WebCore::RenderingResourceIdentifier, uint64_t useCount);
    void finalizeRenderingUpdate(RenderingUpdateID);
    void didCreateSharedDisplayListHandle(WebCore::DisplayList::ItemBufferIdentifier, const SharedMemory::IPCHandle&, WebCore::RenderingResourceIdentifier destinationBufferIdentifier);

    class ReplayerDelegate : public WebCore::DisplayList::Replayer::Delegate {
    public:
        ReplayerDelegate(WebCore::ImageBuffer&, RemoteRenderingBackend&);

    private:
        bool apply(WebCore::DisplayList::ItemHandle, WebCore::GraphicsContext&) final;
        void didCreateMaskImageBuffer(WebCore::ImageBuffer&) final;
        void didResetMaskImageBuffer() final;
        void recordResourceUse(WebCore::RenderingResourceIdentifier) final;

        WebCore::ImageBuffer& m_destination;
        RemoteRenderingBackend& m_remoteRenderingBackend;
    };

    struct PendingWakeupInformation {
        GPUProcessWakeupMessageArguments arguments;
        std::optional<WebCore::RenderingResourceIdentifier> missingCachedResourceIdentifier;
        RemoteRenderingBackendState state { RemoteRenderingBackendState::Initialized };

        bool shouldPerformWakeup(WebCore::RenderingResourceIdentifier identifier) const
        {
            return arguments.destinationImageBufferIdentifier == identifier
                || missingCachedResourceIdentifier == identifier;
        }

        bool shouldPerformWakeup(WebCore::DisplayList::ItemBufferIdentifier identifier) const
        {
            return arguments.itemBufferIdentifier == identifier;
        }
    };

    Ref<WorkQueue> m_workQueue;
    RemoteResourceCache m_remoteResourceCache;
    Ref<GPUConnectionToWebProcess> m_gpuConnectionToWebProcess;
    RenderingBackendIdentifier m_renderingBackendIdentifier;
    HashMap<WebCore::DisplayList::ItemBufferIdentifier, RefPtr<DisplayListReaderHandle>> m_sharedDisplayListHandles;
    std::optional<PendingWakeupInformation> m_pendingWakeupInfo;
    IPC::Semaphore m_resumeDisplayListSemaphore;
    IPC::Semaphore m_getPixelBufferSemaphore;
    RefPtr<SharedMemory> m_getPixelBufferSharedMemory;
    ScopedRenderingResourcesRequest m_renderingResourcesRequest;
    RefPtr<WebCore::ImageBuffer> m_currentMaskImageBuffer;
    Atomic<RemoteRenderingBackendState> m_lastKnownState { RemoteRenderingBackendState::Initialized };
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
