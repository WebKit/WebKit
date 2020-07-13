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

#include "config.h"
#include "SampleBufferDisplayLayer.h"

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)

#include "GPUProcessConnection.h"
#include "LayerHostingContext.h"
#include "RemoteSampleBufferDisplayLayerManagerMessages.h"
#include "RemoteSampleBufferDisplayLayerManagerMessagesReplies.h"
#include "RemoteSampleBufferDisplayLayerMessages.h"
#include "SampleBufferDisplayLayerManager.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include <WebCore/RemoteVideoSample.h>

namespace WebKit {
using namespace WebCore;

std::unique_ptr<SampleBufferDisplayLayer> SampleBufferDisplayLayer::create(SampleBufferDisplayLayerManager& manager, Client& client)
{
    return std::unique_ptr<SampleBufferDisplayLayer>(new SampleBufferDisplayLayer(manager, client));
}

SampleBufferDisplayLayer::SampleBufferDisplayLayer(SampleBufferDisplayLayerManager& manager, Client& client)
    : WebCore::SampleBufferDisplayLayer(client)
    , m_manager(makeWeakPtr(manager))
    , m_connection(WebProcess::singleton().ensureGPUProcessConnection().connection())
    , m_identifier(SampleBufferDisplayLayerIdentifier::generate())
{
    manager.addLayer(*this);
}

void SampleBufferDisplayLayer::initialize(bool hideRootLayer, IntSize size, CompletionHandler<void(bool)>&& callback)
{
    m_connection->sendWithAsyncReply(Messages::RemoteSampleBufferDisplayLayerManager::CreateLayer { m_identifier, hideRootLayer, size }, [this, weakThis = makeWeakPtr(this), callback = WTFMove(callback)](auto contextId) mutable {
        if (!weakThis)
            return callback(false);
        if (contextId)
            m_videoLayer = LayerHostingContext::createPlatformLayerForHostingContext(*contextId);
        callback(!!m_videoLayer);
    });
}

SampleBufferDisplayLayer::~SampleBufferDisplayLayer()
{
    m_connection->send(Messages::RemoteSampleBufferDisplayLayerManager::ReleaseLayer { m_identifier }, 0);
    if (m_manager)
        m_manager->removeLayer(*this);
}

bool SampleBufferDisplayLayer::didFail() const
{
    return m_didFail;
}

void SampleBufferDisplayLayer::updateDisplayMode(bool hideDisplayLayer, bool hideRootLayer)
{
    m_connection->send(Messages::RemoteSampleBufferDisplayLayer::UpdateDisplayMode { hideDisplayLayer, hideRootLayer }, m_identifier);
}

void SampleBufferDisplayLayer::updateAffineTransform(CGAffineTransform transform)
{
    m_connection->send(Messages::RemoteSampleBufferDisplayLayer::UpdateAffineTransform { transform }, m_identifier);
}

void SampleBufferDisplayLayer::updateBoundsAndPosition(CGRect bounds, MediaSample::VideoRotation rotation)
{
    m_connection->send(Messages::RemoteSampleBufferDisplayLayer::UpdateBoundsAndPosition { bounds, rotation }, m_identifier);
}

void SampleBufferDisplayLayer::flush()
{
    m_connection->send(Messages::RemoteSampleBufferDisplayLayer::Flush { }, m_identifier);
}

void SampleBufferDisplayLayer::flushAndRemoveImage()
{
    m_connection->send(Messages::RemoteSampleBufferDisplayLayer::FlushAndRemoveImage { }, m_identifier);
}

void SampleBufferDisplayLayer::play()
{
    m_connection->send(Messages::RemoteSampleBufferDisplayLayer::Play { }, m_identifier);
}

void SampleBufferDisplayLayer::pause()
{
    m_connection->send(Messages::RemoteSampleBufferDisplayLayer::Pause { }, m_identifier);
}

void SampleBufferDisplayLayer::enqueueSample(MediaSample& sample)
{
    if (auto remoteSample = RemoteVideoSample::create(sample))
        m_connection->send(Messages::RemoteSampleBufferDisplayLayer::EnqueueSample { *remoteSample }, m_identifier);
}

void SampleBufferDisplayLayer::clearEnqueuedSamples()
{
    m_connection->send(Messages::RemoteSampleBufferDisplayLayer::ClearEnqueuedSamples { }, m_identifier);
}

PlatformLayer* SampleBufferDisplayLayer::rootLayer()
{
    return m_videoLayer.get();
}

void SampleBufferDisplayLayer::setDidFail(bool value)
{
    m_didFail = value;
}

}

#endif
