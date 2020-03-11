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
#include "ImageBufferFlushIdentifier.h"
#include "ImageBufferIdentifier.h"
#include "ImageDataReference.h"
#include "MessageReceiver.h"
#include "MessageSender.h"
#include "RemoteImageBufferMessageHandlerProxy.h"
#include "RenderingBackendIdentifier.h"
#include <WebCore/ColorSpace.h>
#include <WebCore/DisplayListItems.h>
#include <WebCore/FloatSize.h>
#include <WebCore/RenderingMode.h>
#include <wtf/HashMap.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
namespace DisplayList {
class DisplayList;
}
}

namespace WebKit {

class GPUConnectionToWebProcess;

class RemoteRenderingBackendProxy
    : public IPC::MessageSender
    , private IPC::MessageReceiver {
public:
    static std::unique_ptr<RemoteRenderingBackendProxy> create(GPUConnectionToWebProcess&, RenderingBackendIdentifier);
    virtual ~RemoteRenderingBackendProxy();

    RenderingBackendIdentifier renderingBackendIdentifier() const { return m_renderingBackendIdentifier; }

private:
    RemoteRenderingBackendProxy(GPUConnectionToWebProcess&, RenderingBackendIdentifier);

    // IPC::MessageSender.
    IPC::Connection* messageSenderConnection() const override;
    uint64_t messageSenderDestinationID() const override;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) override;

    // Messages to be received.
    void createImageBuffer(const WebCore::FloatSize& logicalSize, WebCore::RenderingMode, float resolutionScale, WebCore::ColorSpace, ImageBufferIdentifier);
    void releaseImageBuffer(ImageBufferIdentifier);
    void flushImageBufferDrawingContext(const WebCore::DisplayList::DisplayList&, ImageBufferIdentifier);
    void flushImageBufferDrawingContextAndCommit(const WebCore::DisplayList::DisplayList&, ImageBufferFlushIdentifier, ImageBufferIdentifier);
    void getImageData(WebCore::AlphaPremultiplication outputFormat, WebCore::IntRect srcRect, ImageBufferIdentifier, CompletionHandler<void(IPC::ImageDataReference&&)>&&);

    using ImageBufferMessageHandlerMap = HashMap<ImageBufferIdentifier, std::unique_ptr<RemoteImageBufferMessageHandlerProxy>>;
    ImageBufferMessageHandlerMap m_imageBufferMessageHandlerMap;
    WeakPtr<GPUConnectionToWebProcess> m_gpuConnectionToWebProcess;
    RenderingBackendIdentifier m_renderingBackendIdentifier;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
