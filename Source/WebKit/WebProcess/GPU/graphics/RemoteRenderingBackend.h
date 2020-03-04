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

#include "ImageBufferBackendHandle.h"
#include "ImageBufferFlushIdentifier.h"
#include "ImageBufferIdentifier.h"
#include "MessageReceiver.h"
#include "MessageSender.h"
#include "RemoteImageBufferMessageHandler.h"
#include "RenderingBackendIdentifier.h"
#include <WebCore/ColorSpace.h>
#include <WebCore/DisplayList.h>
#include <WebCore/FloatSize.h>
#include <WebCore/RenderingMode.h>
#include <wtf/HashMap.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class RemoteRenderingBackend
    : public IPC::MessageSender
    , private IPC::MessageReceiver
    , public CanMakeWeakPtr<RemoteRenderingBackend> {
public:
    static std::unique_ptr<RemoteRenderingBackend> create();

    ~RemoteRenderingBackend();

    RenderingBackendIdentifier renderingBackendIdentifier() const { return m_renderingBackendIdentifier; }

    // IPC::MessageSender.
    IPC::Connection* messageSenderConnection() const override;
    uint64_t messageSenderDestinationID() const override;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    std::unique_ptr<WebCore::ImageBuffer> createImageBuffer(const WebCore::FloatSize&, WebCore::ShouldAccelerate, float resolutionScale, WebCore::ColorSpace);
    void releaseImageBuffer(ImageBufferIdentifier);

    bool waitForCreateImageBufferBackend();
    bool waitForCommitImageBufferFlushContext();

private:
    RemoteRenderingBackend();

    // Messages to be received.
    void createImageBufferBackend(const WebCore::FloatSize& logicalSize, const WebCore::IntSize& backendSize, float resolutionScale, WebCore::ColorSpace, ImageBufferBackendHandle, ImageBufferIdentifier);
    void commitImageBufferFlushContext(ImageBufferFlushIdentifier, ImageBufferIdentifier);

    using ImageBufferMessageHandlerMap = HashMap<ImageBufferIdentifier, RemoteImageBufferMessageHandler*>;
    ImageBufferMessageHandlerMap m_imageBufferMessageHandlerMap;
    RenderingBackendIdentifier m_renderingBackendIdentifier { RenderingBackendIdentifier::generate() };
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
