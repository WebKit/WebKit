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
#include "DisplayListFlushIdentifier.h"
#include "ImageBufferBackendHandle.h"
#include "ImageDataReference.h"
#include "MessageReceiver.h"
#include "MessageSender.h"
#include "RemoteResourceCache.h"
#include "RenderingBackendIdentifier.h"
#include <WebCore/ColorSpace.h>
#include <WebCore/DisplayListItems.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
namespace DisplayList {
class DisplayList;
}
class FloatSize;
enum class ColorSpace : uint8_t;
enum class RenderingMode : uint8_t;
}

namespace WebKit {

class GPUConnectionToWebProcess;

class RemoteRenderingBackend
    : public IPC::MessageSender
    , private IPC::MessageReceiver {
public:
    static std::unique_ptr<RemoteRenderingBackend> create(GPUConnectionToWebProcess&, RenderingBackendIdentifier);
    virtual ~RemoteRenderingBackend();

    GPUConnectionToWebProcess* gpuConnectionToWebProcess() const;

    // Messages to be sent.
    void imageBufferBackendWasCreated(const WebCore::FloatSize& logicalSize, const WebCore::IntSize& backendSize, float resolutionScale, WebCore::ColorSpace, ImageBufferBackendHandle, WebCore::RemoteResourceIdentifier);
    void flushDisplayListWasCommitted(DisplayListFlushIdentifier, WebCore::RemoteResourceIdentifier);

private:
    RemoteRenderingBackend(GPUConnectionToWebProcess&, RenderingBackendIdentifier);

    // IPC::MessageSender.
    IPC::Connection* messageSenderConnection() const override;
    uint64_t messageSenderDestinationID() const override;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) override;

    // Messages to be received.
    void createImageBuffer(const WebCore::FloatSize& logicalSize, WebCore::RenderingMode, float resolutionScale, WebCore::ColorSpace, WebCore::RemoteResourceIdentifier);
    void flushDisplayList(const WebCore::DisplayList::DisplayList&, WebCore::RemoteResourceIdentifier);
    void flushDisplayListAndCommit(const WebCore::DisplayList::DisplayList&, DisplayListFlushIdentifier, WebCore::RemoteResourceIdentifier);
    void getImageData(WebCore::AlphaPremultiplication outputFormat, WebCore::IntRect srcRect, WebCore::RemoteResourceIdentifier, CompletionHandler<void(IPC::ImageDataReference&&)>&&);
    void releaseRemoteResource(WebCore::RemoteResourceIdentifier);

    RemoteResourceCache m_remoteResourceCache;
    WeakPtr<GPUConnectionToWebProcess> m_gpuConnectionToWebProcess;
    RenderingBackendIdentifier m_renderingBackendIdentifier;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
