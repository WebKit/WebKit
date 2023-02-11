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

#include "GPUProcessConnection.h"
#include "MessageReceiver.h"
#include "SampleBufferDisplayLayerIdentifier.h"
#include "SharedVideoFrame.h"
#include <WebCore/SampleBufferDisplayLayer.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
enum class VideoFrameRotation : uint16_t;
}

namespace WebKit {

class SampleBufferDisplayLayerManager;

class SampleBufferDisplayLayer final : public WebCore::SampleBufferDisplayLayer, public IPC::MessageReceiver, public GPUProcessConnection::Client {
public:
    static std::unique_ptr<SampleBufferDisplayLayer> create(SampleBufferDisplayLayerManager&, WebCore::SampleBufferDisplayLayer::Client&);
    ~SampleBufferDisplayLayer();

    SampleBufferDisplayLayerIdentifier identifier() const { return m_identifier; }

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    using GPUProcessConnection::Client::weakPtrFactory;
    using GPUProcessConnection::Client::WeakValueType;
    using GPUProcessConnection::Client::WeakPtrImplType;

private:
    SampleBufferDisplayLayer(SampleBufferDisplayLayerManager&, WebCore::SampleBufferDisplayLayer::Client&);
    void disconnectGPUProcessConnectionIfNeeded();

    // WebCore::SampleBufferDisplayLayer
    void initialize(bool hideRootLayer, WebCore::IntSize, CompletionHandler<void(bool)>&&) final;
#if !RELEASE_LOG_DISABLED
    void setLogIdentifier(String&&) final;
#endif
    bool didFail() const final;
    void updateDisplayMode(bool hideDisplayLayer, bool hideRootLayer) final;
    void updateAffineTransform(CGAffineTransform) final;
    void updateBoundsAndPosition(CGRect, WebCore::VideoFrameRotation) final;
    void flush() final;
    void flushAndRemoveImage() final;
    void play() final;
    void pause() final;
    void enqueueVideoFrame(WebCore::VideoFrame&) final;
    void clearVideoFrames() final;
    PlatformLayer* rootLayer() final;

    // GPUProcessConnection::Client
    void gpuProcessConnectionDidClose(GPUProcessConnection&) final;

    void setDidFail(bool);

    GPUProcessConnection* m_gpuProcessConnection;
    WeakPtr<SampleBufferDisplayLayerManager> m_manager;
    Ref<IPC::Connection> m_connection;
    SampleBufferDisplayLayerIdentifier m_identifier;

    PlatformLayerContainer m_videoLayer;
    bool m_didFail { false };
    bool m_paused { false };

    SharedVideoFrameWriter m_sharedVideoFrameWriter;
};

}

#endif // PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)
