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

#include "MediaSample.h"
#include "PlatformLayer.h"
#include <wtf/CompletionHandler.h>
#include <wtf/WeakPtr.h>

namespace WTF {
class MediaTime;
}

namespace WebCore {
class IntSize;
class MediaSample;

class SampleBufferDisplayLayer {
public:
    class Client : public CanMakeWeakPtr<Client> {
    public:
        virtual ~Client() = default;
        virtual void sampleBufferDisplayLayerStatusDidChange(SampleBufferDisplayLayer&) = 0;
    };

    WEBCORE_EXPORT static std::unique_ptr<SampleBufferDisplayLayer> create(Client&);
    using LayerCreator = std::unique_ptr<SampleBufferDisplayLayer> (*)(Client&);
    WEBCORE_EXPORT static void setCreator(LayerCreator);

    virtual ~SampleBufferDisplayLayer() = default;

    virtual void initialize(bool hideRootLayer, IntSize, CompletionHandler<void(bool didSucceed)>&&) = 0;
    virtual bool didFail() const = 0;

    virtual void updateDisplayMode(bool hideDisplayLayer, bool hideRootLayer) = 0;

    virtual void updateAffineTransform(CGAffineTransform) = 0;
    virtual void updateBoundsAndPosition(CGRect, MediaSample::VideoRotation) = 0;

    virtual void flush() = 0;
    virtual void flushAndRemoveImage() = 0;

    virtual void enqueueSample(MediaSample&) = 0;
    virtual void clearEnqueuedSamples() = 0;

    virtual PlatformLayer* rootLayer() = 0;

    enum class RenderPolicy { TimingInfo, Immediately };
    virtual void setRenderPolicy(RenderPolicy) { };

protected:
    explicit SampleBufferDisplayLayer(Client&);

    WeakPtr<Client> m_client;

private:
    static LayerCreator m_layerCreator;
};

inline SampleBufferDisplayLayer::SampleBufferDisplayLayer(Client& client)
    : m_client(makeWeakPtr(client))
{
}

}
