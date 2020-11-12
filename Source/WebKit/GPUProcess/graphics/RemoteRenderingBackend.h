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
#include "ImageBufferBackendHandle.h"
#include "ImageDataReference.h"
#include "MessageReceiver.h"
#include "MessageSender.h"
#include "RemoteResourceCache.h"
#include "RenderingBackendIdentifier.h"
#include <WebCore/ColorSpace.h>
#include <WebCore/DisplayList.h>
#include <WebCore/DisplayListItems.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
namespace DisplayList {
class DisplayList;
class Item;
}
class FloatSize;
class NativeImage;
enum class ColorSpace : uint8_t;
enum class RenderingMode : bool;
}

namespace WebKit {

class DisplayListReaderHandle;
class GPUConnectionToWebProcess;

class RemoteRenderingBackend
    : public IPC::MessageSender
    , private IPC::MessageReceiver
    , public WebCore::DisplayList::ItemBufferReadingClient {
public:
    static std::unique_ptr<RemoteRenderingBackend> create(GPUConnectionToWebProcess&, RenderingBackendIdentifier);
    virtual ~RemoteRenderingBackend();

    GPUConnectionToWebProcess* gpuConnectionToWebProcess() const;
    RemoteResourceCache& remoteResourceCache() { return m_remoteResourceCache; }

    // Rendering operations.
    bool applyMediaItem(WebCore::DisplayList::ItemHandle, WebCore::GraphicsContext&);

    // Messages to be sent.
    void imageBufferBackendWasCreated(const WebCore::FloatSize& logicalSize, const WebCore::IntSize& backendSize, float resolutionScale, WebCore::ColorSpace, WebCore::PixelFormat, ImageBufferBackendHandle, WebCore::RenderingResourceIdentifier);
    void flushDisplayListWasCommitted(WebCore::DisplayList::FlushIdentifier, WebCore::RenderingResourceIdentifier);

    void setNextItemBufferToRead(WebCore::DisplayList::ItemBufferIdentifier);

private:
    RemoteRenderingBackend(GPUConnectionToWebProcess&, RenderingBackendIdentifier);

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

    void applyDisplayListsFromHandle(WebCore::ImageBuffer& destination, DisplayListReaderHandle&, size_t offset);

    // IPC::MessageSender.
    IPC::Connection* messageSenderConnection() const override;
    uint64_t messageSenderDestinationID() const override;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) override;

    // Messages to be received.
    void createImageBuffer(const WebCore::FloatSize& logicalSize, WebCore::RenderingMode, float resolutionScale, WebCore::ColorSpace, WebCore::PixelFormat, WebCore::RenderingResourceIdentifier);
    void wakeUpAndApplyDisplayList(WebCore::DisplayList::ItemBufferIdentifier initialIdentifier, uint64_t initialOffset, WebCore::RenderingResourceIdentifier destinationBufferIdentifier);
    void getImageData(WebCore::AlphaPremultiplication outputFormat, WebCore::IntRect srcRect, WebCore::RenderingResourceIdentifier, CompletionHandler<void(IPC::ImageDataReference&&)>&&);
    void cacheNativeImage(Ref<WebCore::NativeImage>&&);
    void releaseRemoteResource(WebCore::RenderingResourceIdentifier);
    void didCreateSharedDisplayListHandle(WebCore::DisplayList::ItemBufferIdentifier, const SharedMemory::IPCHandle&, WebCore::RenderingResourceIdentifier destinationBufferIdentifier);

    RemoteResourceCache m_remoteResourceCache;
    WeakPtr<GPUConnectionToWebProcess> m_gpuConnectionToWebProcess;
    RenderingBackendIdentifier m_renderingBackendIdentifier;
    HashMap<WebCore::DisplayList::ItemBufferIdentifier, RefPtr<DisplayListReaderHandle>> m_sharedDisplayListHandles;
    WebCore::DisplayList::ItemBufferIdentifier m_nextItemBufferToRead;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
