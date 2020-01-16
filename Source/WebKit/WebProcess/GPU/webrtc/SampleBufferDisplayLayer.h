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

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(VIDEO_TRACK) && ENABLE(MEDIA_STREAM)

#include "MessageReceiver.h"
#include "SampleBufferDisplayLayerIdentifier.h"
#include <WebCore/SampleBufferDisplayLayer.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class SampleBufferDisplayLayerManager;

class SampleBufferDisplayLayer final : public WebCore::SampleBufferDisplayLayer, public IPC::MessageReceiver, public CanMakeWeakPtr<SampleBufferDisplayLayer> {
public:
    static std::unique_ptr<SampleBufferDisplayLayer> create(SampleBufferDisplayLayerManager&, Client&, bool hideRootLayer, WebCore::IntSize);
    ~SampleBufferDisplayLayer();

    SampleBufferDisplayLayerIdentifier identifier() const { return m_identifier; }

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

private:
    SampleBufferDisplayLayer(SampleBufferDisplayLayerManager&, Client&, bool hideRootLayer, WebCore::IntSize);

    // WebCore::SampleBufferDisplayLayer
    bool didFail() const final;
    void updateDisplayMode(bool hideDisplayLayer, bool hideRootLayer) final;
    CGRect bounds() const final;
    void updateAffineTransform(CGAffineTransform) final;
    void updateBoundsAndPosition(CGRect, CGPoint) final;
    void flush() final;
    void flushAndRemoveImage() final;
    void enqueueSample(WebCore::MediaSample&) final;
    void clearEnqueuedSamples() final;
    PlatformLayer* rootLayer() final;
    
    void setDidFail(bool);

    WeakPtr<SampleBufferDisplayLayerManager> m_manager;
    Ref<IPC::Connection> m_connection;
    SampleBufferDisplayLayerIdentifier m_identifier;

    RetainPtr<PlatformLayer> m_videoLayer;
    CGRect m_bounds;
    bool m_didFail { false };
};

}

#endif // PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(VIDEO_TRACK) && ENABLE(MEDIA_STREAM)
