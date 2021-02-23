/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "RemoteResourceCache.h"
#include "RenderingBackendIdentifier.h"
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
class FloatSize;
class NativeImage;
enum class DestinationColorSpace : uint8_t;
enum class RenderingMode : bool;
}

namespace WebKit {

class DisplayListReaderHandle;
class GPUConnectionToWebProcess;

class RemoteRenderingBackend
    : private IPC::MessageSender
    , public IPC::Connection::WorkQueueMessageReceiver
    , public WebCore::DisplayList::ItemBufferReadingClient {
public:

    static Ref<RemoteRenderingBackend> create(GPUConnectionToWebProcess&, RenderingBackendIdentifier, IPC::Semaphore&& resumeDisplayListSemaphore);
    virtual ~RemoteRenderingBackend();
    void stopListeningForIPC();

    RemoteResourceCache& remoteResourceCache() { return m_remoteResourceCache; }

    // Rendering operations.
    bool applyMediaItem(WebCore::DisplayList::ItemHandle, WebCore::GraphicsContext&);

    // Messages to be sent.
    void didCreateImageBufferBackend(ImageBufferBackendHandle, WebCore::RenderingResourceIdentifier);
    void didFlush(WebCore::DisplayList::FlushIdentifier, WebCore::RenderingResourceIdentifier);

    void setNextItemBufferToRead(WebCore::DisplayList::ItemBufferIdentifier, WebCore::RenderingResourceIdentifier destination);

    // Runs Function in RemoteRenderingBackend task queue.
    void dispatch(Function<void()>&&);

private:
    RemoteRenderingBackend(GPUConnectionToWebProcess&, RenderingBackendIdentifier, IPC::Semaphore&&);
    void startListeningForIPC();

    Optional<WebCore::DisplayList::ItemHandle> WARN_UNUSED_RETURN decodeItem(const uint8_t* data, size_t length, WebCore::DisplayList::ItemType, uint8_t* handleLocation) override;

    template<typename T>
    Optional<WebCore::DisplayList::ItemHandle> WARN_UNUSED_RETURN decodeAndCreate(const uint8_t* data, size_t length, uint8_t* handleLocation)
    {
        if (auto item = IPC::Decoder::decodeSingleObject<T>(data, length)) {
            // FIXME: WebKit2 should not need to know that the first 8 bytes at the handle are reserved for the type.
            // Need to figure out a way to keep this knowledge within display list code in WebCore.
            new (handleLocation + sizeof(uint64_t)) T(WTFMove(*item));
            return {{ handleLocation }};
        }
        return WTF::nullopt;
    }

    WebCore::DisplayList::ReplayResult submit(const WebCore::DisplayList::DisplayList&, WebCore::ImageBuffer& destination);
    RefPtr<WebCore::ImageBuffer> nextDestinationImageBufferAfterApplyingDisplayLists(WebCore::ImageBuffer& initialDestination, size_t initialOffset, DisplayListReaderHandle&, GPUProcessWakeupReason);

    // IPC::MessageSender.
    IPC::Connection* messageSenderConnection() const override;
    uint64_t messageSenderDestinationID() const override;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) override;

    // Messages to be received.
    void createImageBuffer(const WebCore::FloatSize& logicalSize, WebCore::RenderingMode, float resolutionScale, WebCore::DestinationColorSpace, WebCore::PixelFormat, WebCore::RenderingResourceIdentifier);
    void wakeUpAndApplyDisplayList(const GPUProcessWakeupMessageArguments&);
    void getImageData(WebCore::AlphaPremultiplication outputFormat, WebCore::IntRect srcRect, WebCore::RenderingResourceIdentifier, CompletionHandler<void(RefPtr<WebCore::ImageData>&&)>&&);
    void getDataURLForImageBuffer(const String& mimeType, Optional<double> quality, WebCore::PreserveResolution, WebCore::RenderingResourceIdentifier, CompletionHandler<void(String&&)>&&);
    void getDataForImageBuffer(const String& mimeType, Optional<double> quality, WebCore::RenderingResourceIdentifier, CompletionHandler<void(Vector<uint8_t>&&)>&&);
    void getBGRADataForImageBuffer(WebCore::RenderingResourceIdentifier, CompletionHandler<void(Vector<uint8_t>&&)>&&);
    void getShareableBitmapForImageBuffer(WebCore::RenderingResourceIdentifier, WebCore::PreserveResolution, CompletionHandler<void(ShareableBitmap::Handle&&)>&&);
    void cacheNativeImage(const ShareableBitmap::Handle&, WebCore::RenderingResourceIdentifier);
    void cacheFont(Ref<WebCore::Font>&&);
    void deleteAllFonts();
    void releaseRemoteResource(WebCore::RenderingResourceIdentifier);
    void didCreateSharedDisplayListHandle(WebCore::DisplayList::ItemBufferIdentifier, const SharedMemory::IPCHandle&, WebCore::RenderingResourceIdentifier destinationBufferIdentifier);

    struct PendingWakeupInformation {
        GPUProcessWakeupMessageArguments arguments;
        Optional<WebCore::RenderingResourceIdentifier> missingCachedResourceIdentifier;

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
    Optional<PendingWakeupInformation> m_pendingWakeupInfo;
    IPC::Semaphore m_resumeDisplayListSemaphore;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
