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

#if ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)

#include "SampleBufferDisplayLayer.h"
#include <wtf/Deque.h>
#include <wtf/Forward.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS AVSampleBufferDisplayLayer;
OBJC_CLASS WebAVSampleBufferStatusChangeListener;

namespace WebCore {

class WEBCORE_EXPORT LocalSampleBufferDisplayLayer final : public SampleBufferDisplayLayer, public CanMakeWeakPtr<LocalSampleBufferDisplayLayer> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<LocalSampleBufferDisplayLayer> create(Client&, bool hideRootLayer, IntSize);

    LocalSampleBufferDisplayLayer(RetainPtr<AVSampleBufferDisplayLayer>&&, Client&, bool hideRootLayer, IntSize);
    ~LocalSampleBufferDisplayLayer();

    // API used by WebAVSampleBufferStatusChangeListener
    void layerStatusDidChange();
    void layerErrorDidChange();
    void rootLayerBoundsDidChange();

    PlatformLayer* displayLayer();

    PlatformLayer* rootLayer() final;

    void updateRootLayerBoundsAndPosition(CGRect, CGPoint);

    bool didFail() const final;

    void updateDisplayMode(bool hideDisplayLayer, bool hideRootLayer) final;

    CGRect bounds() const final;
    void updateAffineTransform(CGAffineTransform)  final;
    void updateBoundsAndPosition(CGRect, CGPoint) final;

    void flush() final;
    void flushAndRemoveImage() final;

    void enqueueSample(MediaSample&) final;
    void clearEnqueuedSamples() final;

private:
    void removeOldSamplesFromPendingQueue();
    void addSampleToPendingQueue(MediaSample&);
    void requestNotificationWhenReadyForVideoData();

private:
    RetainPtr<WebAVSampleBufferStatusChangeListener> m_statusChangeListener;
    RetainPtr<AVSampleBufferDisplayLayer> m_sampleBufferDisplayLayer;
    RetainPtr<PlatformLayer> m_rootLayer;

    using PendingSampleQueue = Deque<Ref<MediaSample>>;
    PendingSampleQueue m_pendingVideoSampleQueue;
};

}

#endif // ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)
