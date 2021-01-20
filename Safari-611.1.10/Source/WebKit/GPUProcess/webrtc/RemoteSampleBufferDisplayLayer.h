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

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)

#include "LayerHostingContext.h"
#include "MessageReceiver.h"
#include "MessageSender.h"
#include "SampleBufferDisplayLayerIdentifier.h"
#include <WebCore/SampleBufferDisplayLayer.h>
#include <wtf/MediaTime.h>

namespace WebCore {
class ImageTransferSessionVT;
class LocalSampleBufferDisplayLayer;
class RemoteVideoSample;
};

namespace WebKit {

class RemoteSampleBufferDisplayLayer : public WebCore::SampleBufferDisplayLayer::Client, public IPC::MessageReceiver, private IPC::MessageSender {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<RemoteSampleBufferDisplayLayer> create(SampleBufferDisplayLayerIdentifier, Ref<IPC::Connection>&&);
    ~RemoteSampleBufferDisplayLayer();

    using LayerInitializationCallback = CompletionHandler<void(Optional<LayerHostingContextID>)>;
    void initialize(bool hideRootLayer, WebCore::IntSize, LayerInitializationCallback&&);

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    CGRect bounds() const;
    
private:
    RemoteSampleBufferDisplayLayer(SampleBufferDisplayLayerIdentifier, Ref<IPC::Connection>&&);

    void updateDisplayMode(bool hideDisplayLayer, bool hideRootLayer);
    void updateAffineTransform(CGAffineTransform);
    void updateBoundsAndPosition(CGRect, WebCore::MediaSample::VideoRotation);
    void flush();
    void flushAndRemoveImage();
    void play();
    void pause();
    void enqueueSample(WebCore::RemoteVideoSample&&);
    void clearEnqueuedSamples();

    // IPC::MessageSender
    IPC::Connection* messageSenderConnection() const final;
    uint64_t messageSenderDestinationID() const final { return m_identifier.toUInt64(); }

    // WebCore::SampleBufferDisplayLayer::Client
    void sampleBufferDisplayLayerStatusDidChange(WebCore::SampleBufferDisplayLayer&) final;

    SampleBufferDisplayLayerIdentifier m_identifier;
    Ref<IPC::Connection> m_connection;
    std::unique_ptr<WebCore::ImageTransferSessionVT> m_imageTransferSession;
    std::unique_ptr<WebCore::LocalSampleBufferDisplayLayer> m_sampleBufferDisplayLayer;
    std::unique_ptr<LayerHostingContext> m_layerHostingContext;
};

}

#endif // PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)
