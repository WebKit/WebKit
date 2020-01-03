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
#include "LibWebRTCCodecsProxy.h"

#if USE(LIBWEBRTC) && PLATFORM(COCOA) && ENABLE(GPU_PROCESS)

#include "DataReference.h"
#include "GPUConnectionToWebProcess.h"
#include "LibWebRTCCodecsMessages.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/LibWebRTCMacros.h>
#include <WebCore/RemoteVideoSample.h>
#include <webrtc/sdk/WebKit/WebKitUtilities.h>
#include <wtf/MediaTime.h>

namespace WebKit {

LibWebRTCCodecsProxy::LibWebRTCCodecsProxy(GPUConnectionToWebProcess& connection)
    : m_gpuConnectionToWebProcess(connection)
{
}

LibWebRTCCodecsProxy::~LibWebRTCCodecsProxy()
{
    for (auto decoder : m_decoders.values())
        webrtc::releaseLocalDecoder(decoder);
}

void LibWebRTCCodecsProxy::createDecoder(RTCDecoderIdentifier identifier)
{
    ASSERT(!m_decoders.contains(identifier));
    m_decoders.add(identifier, webrtc::createLocalDecoder(^(CVPixelBufferRef pixelBuffer, uint32_t timeStampNs, uint32_t timeStamp) {
        if (auto sample = WebCore::RemoteVideoSample::create(pixelBuffer, MediaTime(timeStampNs, 1)))
            m_gpuConnectionToWebProcess.connection().send(Messages::LibWebRTCCodecs::CompletedDecoding { identifier, timeStamp, *sample }, 0);
    }));
}

void LibWebRTCCodecsProxy::releaseDecoder(RTCDecoderIdentifier identifier)
{
    ASSERT(m_decoders.contains(identifier));
    if (auto decoder = m_decoders.take(identifier))
        webrtc::releaseLocalDecoder(decoder);
}

void LibWebRTCCodecsProxy::decodeFrame(RTCDecoderIdentifier identifier, uint32_t timeStamp, const IPC::DataReference& data)
{
    ASSERT(m_decoders.contains(identifier));
    auto decoder = m_decoders.get(identifier);
    if (!decoder)
        return;

    if (webrtc::decodeFrame(decoder, timeStamp, data.data(), data.size()))
        m_gpuConnectionToWebProcess.connection().send(Messages::LibWebRTCCodecs::FailedDecoding { identifier }, 0);
}

}

#endif
