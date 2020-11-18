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
#include "ImageBufferBackendHandle.h"
#include "MessageReceiver.h"
#include "MessageSender.h"
#include "RemoteResourceCacheProxy.h"
#include "RenderingBackendIdentifier.h"
#include <WebCore/DisplayList.h>
#include <WebCore/RenderingResourceIdentifier.h>
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
enum class ColorSpace : uint8_t;
enum class RenderingMode : bool;
}

namespace WebKit {

class DisplayListWriterHandle;

class RemoteRenderingBackendProxy
    : public IPC::MessageSender
    , private IPC::MessageReceiver
    , public GPUProcessConnection::Client {
public:
    static std::unique_ptr<RemoteRenderingBackendProxy> create();

    ~RemoteRenderingBackendProxy();

    RemoteResourceCacheProxy& remoteResourceCacheProxy() { return m_remoteResourceCacheProxy; }
    WebCore::DisplayList::ItemBufferHandle createItemBuffer(size_t capacity, WebCore::RenderingResourceIdentifier destinationBufferIdentifier);

    // IPC::MessageSender.
    IPC::Connection* messageSenderConnection() const override;
    uint64_t messageSenderDestinationID() const override;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // Messages to be sent.
    RefPtr<WebCore::ImageBuffer> createImageBuffer(const WebCore::FloatSize&, WebCore::RenderingMode, float resolutionScale, WebCore::ColorSpace, WebCore::PixelFormat);
    RefPtr<WebCore::ImageData> getImageData(WebCore::AlphaPremultiplication outputFormat, const WebCore::IntRect& srcRect, WebCore::RenderingResourceIdentifier);
    void submitDisplayList(const WebCore::DisplayList::DisplayList&, WebCore::RenderingResourceIdentifier destinationBufferIdentifier);
    WebCore::DisplayList::FlushIdentifier flushDisplayListAndCommit(const WebCore::DisplayList::DisplayList&, WebCore::RenderingResourceIdentifier);
    void cacheNativeImage(WebCore::NativeImage&);
    void releaseRemoteResource(WebCore::RenderingResourceIdentifier);

    bool waitForImageBufferBackendWasCreated();
    bool waitForDidFlush();

private:
    RemoteRenderingBackendProxy();

    // GPUProcessConnection::Client
    void gpuProcessConnectionDidClose(GPUProcessConnection&) final;

    void connectToGPUProcess();
    void reestablishGPUProcessConnection();
    void updateReusableHandles();

    // Messages to be received.
    void imageBufferBackendWasCreated(const WebCore::FloatSize& logicalSize, const WebCore::IntSize& backendSize, float resolutionScale, WebCore::ColorSpace, WebCore::PixelFormat, ImageBufferBackendHandle, WebCore::RenderingResourceIdentifier);
    void didFlush(WebCore::DisplayList::FlushIdentifier, WebCore::RenderingResourceIdentifier);

    RemoteResourceCacheProxy m_remoteResourceCacheProxy { *this };
    HashMap<WebCore::DisplayList::ItemBufferIdentifier, RefPtr<DisplayListWriterHandle>> m_sharedDisplayListHandles;
    Deque<WebCore::DisplayList::ItemBufferIdentifier> m_identifiersOfReusableHandles;
    HashSet<WebCore::DisplayList::ItemBufferIdentifier> m_identifiersOfHandlesAvailableForWriting;
    RenderingBackendIdentifier m_renderingBackendIdentifier { RenderingBackendIdentifier::generate() };
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
