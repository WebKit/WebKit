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

#import "config.h"
#import "LibWebRTCCodecsProxy.h"

#if USE(LIBWEBRTC) && PLATFORM(COCOA) && ENABLE(GPU_PROCESS)

#import "GPUConnectionToWebProcess.h"
#import "LibWebRTCCodecsMessages.h"
#import "LibWebRTCCodecsProxyMessages.h"
#import "WebCoreArgumentCoders.h"
#import <WebCore/LibWebRTCMacros.h>
#import <WebCore/RemoteVideoSample.h>
#import <webrtc/sdk/WebKit/WebKitDecoder.h>
#import <webrtc/sdk/WebKit/WebKitEncoder.h>
#import <wtf/BlockPtr.h>
#import <wtf/MediaTime.h>

namespace WebKit {

LibWebRTCCodecsProxy::LibWebRTCCodecsProxy(GPUConnectionToWebProcess& connection)
    : m_gpuConnectionToWebProcess(connection)
    , m_queue(connection.gpuProcess().libWebRTCCodecsQueue())
{
    m_gpuConnectionToWebProcess.connection().addThreadMessageReceiver(Messages::LibWebRTCCodecsProxy::messageReceiverName(), this);
}

LibWebRTCCodecsProxy::~LibWebRTCCodecsProxy()
{
}

void LibWebRTCCodecsProxy::dispatchToThread(Function<void()>&& function)
{
    m_queue->dispatch(WTFMove(function));
}

void LibWebRTCCodecsProxy::close()
{
    m_gpuConnectionToWebProcess.connection().removeThreadMessageReceiver(Messages::LibWebRTCCodecsProxy::messageReceiverName());

    dispatchToThread([this, protectedThis = makeRef(*this)] {
        auto decoders = WTFMove(m_decoders);
        for (auto decoder : decoders.values())
            webrtc::releaseLocalDecoder(decoder);
        auto encoders = WTFMove(m_encoders);
        for (auto encoder : encoders.values())
            webrtc::releaseLocalEncoder(encoder);
    });
}

void LibWebRTCCodecsProxy::createH264Decoder(RTCDecoderIdentifier identifier)
{
    ASSERT(!m_decoders.contains(identifier));
    m_decoders.add(identifier, webrtc::createLocalH264Decoder(makeBlockPtr([connection = makeRef(m_gpuConnectionToWebProcess.connection()), identifier](CVPixelBufferRef pixelBuffer, uint32_t timeStampNs, uint32_t timeStamp) {
        if (auto sample = WebCore::RemoteVideoSample::create(pixelBuffer, MediaTime(timeStampNs, 1)))
            connection->send(Messages::LibWebRTCCodecs::CompletedDecoding { identifier, timeStamp, *sample }, 0);
    }).get()));
}

void LibWebRTCCodecsProxy::createH265Decoder(RTCDecoderIdentifier identifier)
{
    ASSERT(!m_decoders.contains(identifier));
    m_decoders.add(identifier, webrtc::createLocalH265Decoder(makeBlockPtr([connection = makeRef(m_gpuConnectionToWebProcess.connection()), identifier](CVPixelBufferRef pixelBuffer, uint32_t timeStampNs, uint32_t timeStamp) {
        if (auto sample = WebCore::RemoteVideoSample::create(pixelBuffer, MediaTime(timeStampNs, 1)))
            connection->send(Messages::LibWebRTCCodecs::CompletedDecoding { identifier, timeStamp, *sample }, 0);
    }).get()));
}

void LibWebRTCCodecsProxy::createVP9Decoder(RTCDecoderIdentifier identifier)
{
    ASSERT(!m_decoders.contains(identifier));
    m_decoders.add(identifier, webrtc::createLocalVP9Decoder(makeBlockPtr([connection = makeRef(m_gpuConnectionToWebProcess.connection()), identifier](CVPixelBufferRef pixelBuffer, uint32_t timeStampNs, uint32_t timeStamp) {
        if (auto sample = WebCore::RemoteVideoSample::create(pixelBuffer, MediaTime(timeStampNs, 1)))
            connection->send(Messages::LibWebRTCCodecs::CompletedDecoding { identifier, timeStamp, *sample }, 0);
    }).get()));
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

void LibWebRTCCodecsProxy::setFrameSize(RTCDecoderIdentifier identifier, uint16_t width, uint16_t height)
{
    ASSERT(m_decoders.contains(identifier));
    auto decoder = m_decoders.get(identifier);
    if (!decoder)
        return;

    webrtc::setDecoderFrameSize(decoder, width, height);
}

void LibWebRTCCodecsProxy::createEncoder(RTCEncoderIdentifier identifier, const String& formatName, const Vector<std::pair<String, String>>& parameters, bool useLowLatency)
{
    ASSERT(!m_encoders.contains(identifier));

    std::map<std::string, std::string> rtcParameters;
    for (auto& parameter : parameters)
        rtcParameters.emplace(parameter.first.utf8().data(), parameter.second.utf8().data());

    auto* encoder = webrtc::createLocalEncoder(webrtc::SdpVideoFormat { formatName.utf8().data(), rtcParameters }, makeBlockPtr([connection = makeRef(m_gpuConnectionToWebProcess.connection()), identifier](const uint8_t* buffer, size_t size, const webrtc::WebKitEncodedFrameInfo& info) {
        connection->send(Messages::LibWebRTCCodecs::CompletedEncoding { identifier, IPC::DataReference { buffer, size }, info }, 0);
    }).get());
    webrtc::setLocalEncoderLowLatency(encoder, useLowLatency);
    m_encoders.add(identifier, encoder);
}

void LibWebRTCCodecsProxy::releaseEncoder(RTCEncoderIdentifier identifier)
{
    ASSERT(m_encoders.contains(identifier));
    if (auto encoder = m_encoders.take(identifier))
        webrtc::releaseLocalEncoder(encoder);
}

void LibWebRTCCodecsProxy::initializeEncoder(RTCEncoderIdentifier identifier, uint16_t width, uint16_t height, unsigned startBitrate, unsigned maxBitrate, unsigned minBitrate, uint32_t maxFramerate)
{
    ASSERT(m_encoders.contains(identifier));
    auto encoder = m_encoders.get(identifier);
    if (!encoder)
        return;

    webrtc::initializeLocalEncoder(encoder, width, height, startBitrate, maxBitrate, minBitrate, maxFramerate);
}

static inline webrtc::VideoRotation toWebRTCVideoRotation(MediaSample::VideoRotation rotation)
{
    switch (rotation) {
    case MediaSample::VideoRotation::None:
        return webrtc::kVideoRotation_0;
    case MediaSample::VideoRotation::UpsideDown:
        return webrtc::kVideoRotation_180;
    case MediaSample::VideoRotation::Right:
        return webrtc::kVideoRotation_90;
    case MediaSample::VideoRotation::Left:
        return webrtc::kVideoRotation_270;
    }
    ASSERT_NOT_REACHED();
    return webrtc::kVideoRotation_0;
}

void LibWebRTCCodecsProxy::encodeFrame(RTCEncoderIdentifier identifier, WebCore::RemoteVideoSample&& sample, uint32_t timeStamp, bool shouldEncodeAsKeyFrame)
{
    ASSERT(m_encoders.contains(identifier));
    auto encoder = m_encoders.get(identifier);
    if (!encoder)
        return;

    if (!m_imageTransferSession || m_imageTransferSession->pixelFormat() != sample.videoFormat())
        m_imageTransferSession = WebCore::ImageTransferSessionVT::create(sample.videoFormat());

#if !PLATFORM(MACCATALYST)
    auto pixelBuffer = m_imageTransferSession->createPixelBuffer(sample.surface());
    webrtc::encodeLocalEncoderFrame(encoder, pixelBuffer.get(), sample.time().toTimeScale(1000000).timeValue(), timeStamp, toWebRTCVideoRotation(sample.rotation()), shouldEncodeAsKeyFrame);
#endif
}

void LibWebRTCCodecsProxy::setEncodeRates(RTCEncoderIdentifier identifier, uint32_t bitRate, uint32_t frameRate)
{
    auto encoder = m_encoders.get(identifier);
    if (!encoder)
        return;

    webrtc::setLocalEncoderRates(encoder, bitRate, frameRate);
}

}

#endif
