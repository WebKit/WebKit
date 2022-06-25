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
#import "GPUProcess.h"
#import "IPCTester.h"
#import "LibWebRTCCodecsMessages.h"
#import "LibWebRTCCodecsProxyMessages.h"
#import "RemoteVideoFrameIdentifier.h"
#import "RemoteVideoFrameObjectHeap.h"
#import "SharedVideoFrame.h"
#import "WebCoreArgumentCoders.h"
#import <WebCore/CVUtilities.h>
#import <WebCore/LibWebRTCProvider.h>
#import <WebCore/PixelBufferConformerCV.h>
#import <WebCore/VideoFrameCV.h>
#import <webrtc/sdk/WebKit/WebKitDecoder.h>
#import <webrtc/sdk/WebKit/WebKitEncoder.h>
#import <wtf/BlockPtr.h>
#import <wtf/MediaTime.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import <WebCore/CoreVideoSoftLink.h>

namespace WebKit {
using namespace WebCore;

Ref<LibWebRTCCodecsProxy> LibWebRTCCodecsProxy::create(GPUConnectionToWebProcess& webProcessConnection)
{
    auto instance = adoptRef(*new LibWebRTCCodecsProxy(webProcessConnection));
    instance->initialize();
    return instance;
}

LibWebRTCCodecsProxy::LibWebRTCCodecsProxy(GPUConnectionToWebProcess& webProcessConnection)
    : m_connection(webProcessConnection.connection())
    , m_queue(webProcessConnection.gpuProcess().libWebRTCCodecsQueue())
    , m_videoFrameObjectHeap(webProcessConnection.videoFrameObjectHeap())
    , m_resourceOwner(webProcessConnection.webProcessIdentity())
{
}

LibWebRTCCodecsProxy::~LibWebRTCCodecsProxy() = default;

void LibWebRTCCodecsProxy::stopListeningForIPC(Ref<LibWebRTCCodecsProxy>&& refFromConnection)
{
    m_connection->removeWorkQueueMessageReceiver(Messages::LibWebRTCCodecsProxy::messageReceiverName());

    m_queue->dispatch([this, protectedThis = WTFMove(refFromConnection)] {
        assertIsCurrent(workQueue());
        auto decoders = WTFMove(m_decoders);
        for (auto decoder : decoders.values())
            webrtc::releaseLocalDecoder(decoder);
        auto encoders = WTFMove(m_encoders);
        for (auto& encoder : encoders.values())
            webrtc::releaseLocalEncoder(encoder.webrtcEncoder);
    });
}

void LibWebRTCCodecsProxy::initialize()
{
    m_connection->addWorkQueueMessageReceiver(Messages::LibWebRTCCodecsProxy::messageReceiverName(), m_queue, *this);
}

auto LibWebRTCCodecsProxy::createDecoderCallback(RTCDecoderIdentifier identifier, bool useRemoteFrames)
{
    RefPtr<RemoteVideoFrameObjectHeap> videoFrameObjectHeap;
    if (useRemoteFrames)
        videoFrameObjectHeap = m_videoFrameObjectHeap.ptr();
    return [identifier, connection = m_connection, resourceOwner = m_resourceOwner, videoFrameObjectHeap = WTFMove(videoFrameObjectHeap)] (CVPixelBufferRef pixelBuffer, uint32_t timeStampNs, uint32_t timeStamp) mutable {
        auto videoFrame = WebCore::VideoFrameCV::create(MediaTime(timeStampNs, 1), false, WebCore::VideoFrame::Rotation::None, pixelBuffer);
        if (resourceOwner)
            videoFrame->setOwnershipIdentity(resourceOwner);
        if (videoFrameObjectHeap) {
            auto properties = videoFrameObjectHeap->add(WTFMove(videoFrame));
            connection->send(Messages::LibWebRTCCodecs::CompletedDecoding { identifier, timeStamp, timeStampNs, WTFMove(properties) }, 0);
            return;
        }
        connection->send(Messages::LibWebRTCCodecs::CompletedDecodingCV { identifier, timeStamp, timeStampNs, pixelBuffer }, 0);
    };
}

void LibWebRTCCodecsProxy::createH264Decoder(RTCDecoderIdentifier identifier, bool useRemoteFrames)
{
    assertIsCurrent(workQueue());
    auto result = m_decoders.add(identifier, webrtc::createLocalH264Decoder(makeBlockPtr(createDecoderCallback(identifier, useRemoteFrames)).get()));
    ASSERT_UNUSED(result, result.isNewEntry || isTestingIPC());
    m_hasEncodersOrDecoders = true;
}

void LibWebRTCCodecsProxy::createH265Decoder(RTCDecoderIdentifier identifier, bool useRemoteFrames)
{
    assertIsCurrent(workQueue());
    auto result = m_decoders.add(identifier, webrtc::createLocalH265Decoder(makeBlockPtr(createDecoderCallback(identifier, useRemoteFrames)).get()));
    ASSERT_UNUSED(result, result.isNewEntry || isTestingIPC());
    m_hasEncodersOrDecoders = true;
}

void LibWebRTCCodecsProxy::createVP9Decoder(RTCDecoderIdentifier identifier, bool useRemoteFrames)
{
    assertIsCurrent(workQueue());
    auto result = m_decoders.add(identifier, webrtc::createLocalVP9Decoder(makeBlockPtr(createDecoderCallback(identifier, useRemoteFrames)).get()));
    ASSERT_UNUSED(result, result.isNewEntry || isTestingIPC());
    m_hasEncodersOrDecoders = true;
}

void LibWebRTCCodecsProxy::releaseDecoder(RTCDecoderIdentifier identifier)
{
    assertIsCurrent(workQueue());
    auto decoder = m_decoders.take(identifier);
    if (!decoder) {
        ASSERT_IS_TESTING_IPC();
        return;
    }
    webrtc::releaseLocalDecoder(decoder);
    m_hasEncodersOrDecoders = !m_encoders.isEmpty() || !m_decoders.isEmpty();
}

void LibWebRTCCodecsProxy::decodeFrame(RTCDecoderIdentifier identifier, uint32_t timeStamp, const IPC::DataReference& data) WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    assertIsCurrent(workQueue());
    auto decoder = m_decoders.get(identifier);
    if (!decoder) {
        ASSERT_IS_TESTING_IPC();
        return;
    }
    if (webrtc::decodeFrame(decoder, timeStamp, data.data(), data.size()))
        m_connection->send(Messages::LibWebRTCCodecs::FailedDecoding { identifier }, 0);
}

void LibWebRTCCodecsProxy::setFrameSize(RTCDecoderIdentifier identifier, uint16_t width, uint16_t height) WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    assertIsCurrent(workQueue());
    auto decoder = m_decoders.get(identifier);
    if (!decoder) {
        ASSERT_IS_TESTING_IPC();
        return;
    }
    webrtc::setDecoderFrameSize(decoder, width, height);
}

void LibWebRTCCodecsProxy::createEncoder(RTCEncoderIdentifier identifier, const String& formatName, const Vector<std::pair<String, String>>& parameters, bool useLowLatency)
{
    assertIsCurrent(workQueue());
    std::map<std::string, std::string> rtcParameters;
    for (auto& parameter : parameters)
        rtcParameters.emplace(parameter.first.utf8().data(), parameter.second.utf8().data());

    auto* encoder = webrtc::createLocalEncoder(webrtc::SdpVideoFormat { formatName.utf8().data(), rtcParameters }, makeBlockPtr([connection = m_connection, identifier](const uint8_t* buffer, size_t size, const webrtc::WebKitEncodedFrameInfo& info) {
        connection->send(Messages::LibWebRTCCodecs::CompletedEncoding { identifier, IPC::DataReference { buffer, size }, info }, 0);
    }).get());
    webrtc::setLocalEncoderLowLatency(encoder, useLowLatency);
    auto result = m_encoders.add(identifier, Encoder { encoder, makeUnique<SharedVideoFrameReader>(Ref { m_videoFrameObjectHeap }, m_resourceOwner) });
    ASSERT_UNUSED(result, result.isNewEntry || isTestingIPC());
    m_hasEncodersOrDecoders = true;
}

void LibWebRTCCodecsProxy::releaseEncoder(RTCEncoderIdentifier identifier)
{
    assertIsCurrent(workQueue());
    auto encoder = m_encoders.take(identifier);
    if (!encoder.webrtcEncoder) {
        ASSERT_IS_TESTING_IPC();
        return;
    }
    webrtc::releaseLocalEncoder(encoder.webrtcEncoder);
    m_hasEncodersOrDecoders = !m_encoders.isEmpty() || !m_decoders.isEmpty();
}

void LibWebRTCCodecsProxy::initializeEncoder(RTCEncoderIdentifier identifier, uint16_t width, uint16_t height, unsigned startBitrate, unsigned maxBitrate, unsigned minBitrate, uint32_t maxFramerate)
{
    assertIsCurrent(workQueue());
    auto* encoder = findEncoder(identifier);
    if (!encoder) {
        ASSERT_IS_TESTING_IPC();
        return;
    }
    webrtc::initializeLocalEncoder(encoder->webrtcEncoder, width, height, startBitrate, maxBitrate, minBitrate, maxFramerate);
}

LibWebRTCCodecsProxy::Encoder* LibWebRTCCodecsProxy::findEncoder(RTCEncoderIdentifier identifier)
{
    auto iterator = m_encoders.find(identifier);
    if (iterator == m_encoders.end())
        return nullptr;
    return &iterator->value;
}

static inline webrtc::VideoRotation toWebRTCVideoRotation(WebCore::VideoFrame::Rotation rotation)
{
    switch (rotation) {
    case WebCore::VideoFrame::Rotation::None:
        return webrtc::kVideoRotation_0;
    case WebCore::VideoFrame::Rotation::UpsideDown:
        return webrtc::kVideoRotation_180;
    case WebCore::VideoFrame::Rotation::Right:
        return webrtc::kVideoRotation_90;
    case WebCore::VideoFrame::Rotation::Left:
        return webrtc::kVideoRotation_270;
    }
    ASSERT_NOT_REACHED();
    return webrtc::kVideoRotation_0;
}

void LibWebRTCCodecsProxy::encodeFrame(RTCEncoderIdentifier identifier, SharedVideoFrame&& sharedVideoFrame, uint32_t timeStamp, bool shouldEncodeAsKeyFrame)
{
    assertIsCurrent(workQueue());
    auto* encoder = findEncoder(identifier);
    if (!encoder) {
        ASSERT_IS_TESTING_IPC();
        // Make sure to read RemoteVideoFrameReadReference to prevent memory leaks.
        if (std::holds_alternative<RemoteVideoFrameReadReference>(sharedVideoFrame.buffer))
            m_videoFrameObjectHeap->get(WTFMove(std::get<RemoteVideoFrameReadReference>(sharedVideoFrame.buffer)));
        return;
    }

    auto pixelBuffer = encoder->frameReader->readBuffer(WTFMove(sharedVideoFrame.buffer));
    if (!pixelBuffer)
        return;

    if (CVPixelBufferGetPixelFormatType(pixelBuffer.get()) == kCVPixelFormatType_32BGRA) {
        if (!m_pixelBufferConformer) {
            m_pixelBufferConformer = makeUnique<WebCore::PixelBufferConformerCV>((__bridge CFDictionaryRef)@{ (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange) });
        }
        pixelBuffer = m_pixelBufferConformer->convert(pixelBuffer.get());
        if (!pixelBuffer)
            return;
    }

#if !PLATFORM(MACCATALYST)
    webrtc::encodeLocalEncoderFrame(encoder->webrtcEncoder, pixelBuffer.get(), sharedVideoFrame.time.toTimeScale(1000000).timeValue(), timeStamp, toWebRTCVideoRotation(sharedVideoFrame.rotation), shouldEncodeAsKeyFrame);
#endif
}

void LibWebRTCCodecsProxy::setEncodeRates(RTCEncoderIdentifier identifier, uint32_t bitRate, uint32_t frameRate)
{
    assertIsCurrent(workQueue());
    auto* encoder = findEncoder(identifier);
    if (!encoder) {
        ASSERT_IS_TESTING_IPC();
        return;
    }

    webrtc::setLocalEncoderRates(encoder->webrtcEncoder, bitRate, frameRate);
}

void LibWebRTCCodecsProxy::setSharedVideoFrameSemaphore(RTCEncoderIdentifier identifier, IPC::Semaphore&& semaphore)
{
    assertIsCurrent(workQueue());
    auto* encoder = findEncoder(identifier);
    if (!encoder) {
        ASSERT_IS_TESTING_IPC();
        return;
    }

    encoder->frameReader->setSemaphore(WTFMove(semaphore));
}

void LibWebRTCCodecsProxy::setSharedVideoFrameMemory(RTCEncoderIdentifier identifier, const SharedMemory::IPCHandle& ipcHandle)
{
    assertIsCurrent(workQueue());
    auto* encoder = findEncoder(identifier);
    if (!encoder) {
        ASSERT_IS_TESTING_IPC();
        return;
    }

    encoder->frameReader->setSharedMemory(ipcHandle);
}

bool LibWebRTCCodecsProxy::allowsExitUnderMemoryPressure() const
{
    assertIsMainRunLoop();
    return !m_hasEncodersOrDecoders;
}

void LibWebRTCCodecsProxy::setRTCLoggingLevel(WTFLogLevel level)
{
    WebCore::LibWebRTCProvider::setRTCLogging(level);
}

}

#endif
