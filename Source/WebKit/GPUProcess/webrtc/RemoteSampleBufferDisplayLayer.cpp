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
#include "RemoteSampleBufferDisplayLayer.h"

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(VIDEO_TRACK) && ENABLE(MEDIA_STREAM)

#include "SampleBufferDisplayLayerMessages.h"
#include <WebCore/ImageTransferSessionVT.h>
#include <WebCore/LocalSampleBufferDisplayLayer.h>
#include <WebCore/MediaSampleAVFObjC.h>
#include <WebCore/RemoteVideoSample.h>

using namespace WebCore;

namespace WebKit {

std::unique_ptr<RemoteSampleBufferDisplayLayer> RemoteSampleBufferDisplayLayer::create(SampleBufferDisplayLayerIdentifier identifier, Ref<IPC::Connection>&& connection, bool hideRootLayer, IntSize size)
{
    auto layer = std::unique_ptr<RemoteSampleBufferDisplayLayer>(new RemoteSampleBufferDisplayLayer(identifier, WTFMove(connection), hideRootLayer, size));
    return layer->m_sampleBufferDisplayLayer ? WTFMove(layer) : nullptr;
}

RemoteSampleBufferDisplayLayer::RemoteSampleBufferDisplayLayer(SampleBufferDisplayLayerIdentifier identifier, Ref<IPC::Connection>&& connection, bool hideRootLayer, IntSize size)
    : m_identifier(identifier)
    , m_connection(WTFMove(connection))
    , m_sampleBufferDisplayLayer(LocalSampleBufferDisplayLayer::create(*this, hideRootLayer, size))
{
    ASSERT(m_sampleBufferDisplayLayer);
    if (!m_sampleBufferDisplayLayer)
        return;

    m_layerHostingContext = LayerHostingContext::createForExternalHostingProcess();
    m_layerHostingContext->setRootLayer(m_sampleBufferDisplayLayer->rootLayer());
}

RemoteSampleBufferDisplayLayer::~RemoteSampleBufferDisplayLayer()
{
}

Optional<LayerHostingContextID> RemoteSampleBufferDisplayLayer::contextID()
{
    return m_layerHostingContext->contextID();
}

CGRect RemoteSampleBufferDisplayLayer::bounds() const
{
    return m_sampleBufferDisplayLayer->bounds();
}

void RemoteSampleBufferDisplayLayer::updateDisplayMode(bool hideDisplayLayer, bool hideRootLayer)
{
    m_sampleBufferDisplayLayer->updateDisplayMode(hideDisplayLayer, hideRootLayer);
}

void RemoteSampleBufferDisplayLayer::updateAffineTransform(CGAffineTransform transform)
{
    m_sampleBufferDisplayLayer->updateAffineTransform(transform);
}

void RemoteSampleBufferDisplayLayer::updateBoundsAndPosition(CGRect bounds, CGPoint position)
{
    m_sampleBufferDisplayLayer->updateRootLayerBoundsAndPosition(bounds, position);
}

void RemoteSampleBufferDisplayLayer::flush()
{
    m_sampleBufferDisplayLayer->flush();
}

void RemoteSampleBufferDisplayLayer::flushAndRemoveImage()
{
    m_sampleBufferDisplayLayer->flushAndRemoveImage();
}

void RemoteSampleBufferDisplayLayer::enqueueSample(WebCore::RemoteVideoSample&& remoteSample)
{
    m_mediaTime = remoteSample.time();

    if (!m_imageTransferSession || m_imageTransferSession->pixelFormat() != remoteSample.videoFormat())
        m_imageTransferSession = ImageTransferSessionVT::create(remoteSample.videoFormat());

    ASSERT(m_imageTransferSession);
    if (!m_imageTransferSession)
        return;

    auto sample = m_imageTransferSession->createMediaSample(remoteSample.surface(), remoteSample.time(), remoteSample.size());

    ASSERT(sample);
    if (!sample)
        return;

    MediaSampleAVFObjC::setAsDisplayImmediately(*sample);
    m_sampleBufferDisplayLayer->enqueueSample(*sample);
}

void RemoteSampleBufferDisplayLayer::clearEnqueuedSamples()
{
    m_sampleBufferDisplayLayer->clearEnqueuedSamples();
}

IPC::Connection* RemoteSampleBufferDisplayLayer::messageSenderConnection() const
{
    return m_connection.ptr();
}

void RemoteSampleBufferDisplayLayer::sampleBufferDisplayLayerStatusDidChange(WebCore::SampleBufferDisplayLayer&)
{
    send(Messages::SampleBufferDisplayLayer::SetDidFail { m_sampleBufferDisplayLayer->didFail() });
}

WTF::MediaTime RemoteSampleBufferDisplayLayer::streamTime() const
{
    // This is only an approximation which will clear all samples enqueued in m_sampleBufferDisplayLayer except the one being pushed.
    return m_mediaTime;
}

}

#endif // PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(VIDEO_TRACK) && ENABLE(MEDIA_STREAM)
