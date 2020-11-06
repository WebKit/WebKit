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

#include "DisplayListFlushIdentifier.h"
#include "ImageBufferBackendHandle.h"
#include "MessageReceiver.h"
#include "MessageSender.h"
#include "RemoteResourceCacheProxy.h"
#include "RenderingBackendIdentifier.h"
#include <WebCore/DisplayList.h>
#include <WebCore/RenderingResourceIdentifier.h>
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

class RemoteRenderingBackendProxy
    : public IPC::MessageSender
    , private IPC::MessageReceiver
    , public CanMakeWeakPtr<RemoteRenderingBackendProxy> {
public:
    static std::unique_ptr<RemoteRenderingBackendProxy> create();

    ~RemoteRenderingBackendProxy();

    RemoteResourceCacheProxy& remoteResourceCacheProxy() { return m_remoteResourceCacheProxy; }
    WebCore::DisplayList::ItemBufferHandle createItemBuffer(size_t);

    // IPC::MessageSender.
    IPC::Connection* messageSenderConnection() const override;
    uint64_t messageSenderDestinationID() const override;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // Messages to be sent.
    RefPtr<WebCore::ImageBuffer> createImageBuffer(const WebCore::FloatSize&, WebCore::RenderingMode, float resolutionScale, WebCore::ColorSpace);
    RefPtr<WebCore::ImageData> getImageData(WebCore::AlphaPremultiplication outputFormat, const WebCore::IntRect& srcRect, WebCore::RenderingResourceIdentifier);
    void flushDisplayList(const WebCore::DisplayList::DisplayList&, WebCore::RenderingResourceIdentifier);
    DisplayListFlushIdentifier flushDisplayListAndCommit(const WebCore::DisplayList::DisplayList&, WebCore::RenderingResourceIdentifier);
    void releaseRemoteResource(WebCore::RenderingResourceIdentifier);

    bool waitForImageBufferBackendWasCreated();
    bool waitForFlushDisplayListWasCommitted();

private:
    RemoteRenderingBackendProxy();

    // Messages to be received.
    void imageBufferBackendWasCreated(const WebCore::FloatSize& logicalSize, const WebCore::IntSize& backendSize, float resolutionScale, WebCore::ColorSpace, ImageBufferBackendHandle, WebCore::RenderingResourceIdentifier);
    void flushDisplayListWasCommitted(DisplayListFlushIdentifier, WebCore::RenderingResourceIdentifier);

    RemoteResourceCacheProxy m_remoteResourceCacheProxy;
    HashMap<WebCore::DisplayList::ItemBufferIdentifier, RefPtr<SharedMemory>> m_sharedItemBuffers;
    RenderingBackendIdentifier m_renderingBackendIdentifier { RenderingBackendIdentifier::generate() };
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
