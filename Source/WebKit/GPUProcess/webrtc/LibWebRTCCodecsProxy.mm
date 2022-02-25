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
#import "LibWebRTCCodecsMessages.h"
#import "LibWebRTCCodecsProxyMessages.h"
#import "RemoteVideoFrameIdentifier.h"
#import "RemoteVideoFrameObjectHeap.h"
#import "SharedVideoFrame.h"
#import "WebCoreArgumentCoders.h"
#import <WebCore/CVUtilities.h>
#import <WebCore/LibWebRTCProvider.h>
#import <WebCore/MediaSampleAVFObjC.h>
#import <WebCore/RemoteVideoSample.h>
#import <webrtc/sdk/WebKit/WebKitDecoder.h>
#import <webrtc/sdk/WebKit/WebKitEncoder.h>
#import <wtf/BlockPtr.h>
#import <wtf/MediaTime.h>

#import <pal/cf/CoreMediaSoftLink.h>

namespace WebKit {

LibWebRTCCodecsProxy::LibWebRTCCodecsProxy(GPUConnectionToWebProcess& connection)
    : m_gpuConnectionToWebProcess(connection)
    , m_queue(connection.gpuProcess().libWebRTCCodecsQueue())
    , m_videoFrameObjectHeap(connection.videoFrameObjectHeap())
    , m_resourceOwner(connection.webProcessIdentity())
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

    dispatchToThread([this, protectedThis = Ref { *this }] {
        Locker locker { m_lock };
        auto decoders = WTFMove(m_decoders);
        for (auto decoder : decoders.values())
            webrtc::releaseLocalDecoder(decoder);
        auto encoders = WTFMove(m_encoders);
        for (auto& encoder : encoders.values())
            webrtc::releaseLocalEncoder(encoder.webrtcEncoder);
    });
}

static Function<void(CVPixelBufferRef pixelBuffer, uint32_t timeStampNs, uint32_t timeStamp)> createDecoderCallback(RTCDecoderIdentifier identifier, GPUConnectionToWebProcess& gpuConnectionToWebProcess, bool useRemoteFrames)
{
    return [connection = Ref { gpuConnectionToWebProcess.connection() }, resourceOwner = gpuConnectionToWebProcess.webProcessIdentity(), videoFrameObjectHeap = Ref { gpuConnectionToWebProcess.videoFrameObjectHeap() }, identifier, useRemoteFrames] (CVPixelBufferRef pixelBuffer, uint32_t timeStampNs, uint32_t timeStamp)  mutable {
        auto sample = WebCore::MediaSampleAVFObjC::createImageSample(pixelBuffer, WebCore::MediaSample::VideoRotation::None, false, MediaTime(timeStampNs, 1), { });
        if (!sample)
            return;
        if (resourceOwner)
            sample->setOwnershipIdentity(resourceOwner);
        if (useRemoteFrames) {
            auto properties = videoFrameObjectHeap->add(sample.releaseNonNull());
            connection->send(Messages::LibWebRTCCodecs::CompletedDecoding { identifier, timeStamp, WTFMove(properties) }, 0);
            return;
        }
        auto remoteSample = WebCore::RemoteVideoSample::create(*sample);
        if (!remoteSample)
            return;
        connection->send(Messages::LibWebRTCCodecs::CompletedDecodingCV { identifier, timeStamp, *remoteSample }, 0);
    };
}

void LibWebRTCCodecsProxy::createH264Decoder(RTCDecoderIdentifier identifier, bool useRemoteFrames)
{
    ASSERT(!isMainRunLoop());
    Locker locker { m_lock };
    ASSERT(!m_decoders.contains(identifier));
    m_decoders.add(identifier, webrtc::createLocalH264Decoder(makeBlockPtr(createDecoderCallback(identifier, m_gpuConnectionToWebProcess, useRemoteFrames)).get()));
}

void LibWebRTCCodecsProxy::createH265Decoder(RTCDecoderIdentifier identifier, bool useRemoteFrames)
{
    ASSERT(!isMainRunLoop());
    Locker locker { m_lock };
    ASSERT(!m_decoders.contains(identifier));
    m_decoders.add(identifier, webrtc::createLocalH265Decoder(makeBlockPtr(createDecoderCallback(identifier, m_gpuConnectionToWebProcess, useRemoteFrames)).get()));
}

void LibWebRTCCodecsProxy::createVP9Decoder(RTCDecoderIdentifier identifier, bool useRemoteFrames)
{
    ASSERT(!isMainRunLoop());
    Locker locker { m_lock };
    ASSERT(!m_decoders.contains(identifier));
    m_decoders.add(identifier, webrtc::createLocalVP9Decoder(makeBlockPtr(createDecoderCallback(identifier, m_gpuConnectionToWebProcess, useRemoteFrames)).get()));
}

void LibWebRTCCodecsProxy::releaseDecoder(RTCDecoderIdentifier identifier)
{
    ASSERT(!isMainRunLoop());
    Locker locker { m_lock };
    ASSERT(m_decoders.contains(identifier));
    if (auto decoder = m_decoders.take(identifier))
        webrtc::releaseLocalDecoder(decoder);
}

// For performance reasons, this function accesses m_decoders without locking. This is safe because this function runs on the libWebRTCCodecsQueue
// and m_decoders only get modified on this queue.
void LibWebRTCCodecsProxy::decodeFrame(RTCDecoderIdentifier identifier, uint32_t timeStamp, const IPC::DataReference& data) WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    ASSERT(!isMainRunLoop());
    ASSERT(m_decoders.contains(identifier));
    auto decoder = m_decoders.get(identifier);
    if (!decoder)
        return;

    if (webrtc::decodeFrame(decoder, timeStamp, data.data(), data.size()))
        m_gpuConnectionToWebProcess.connection().send(Messages::LibWebRTCCodecs::FailedDecoding { identifier }, 0);
}

// For performance reasons, this function accesses m_decoders without locking. This is safe because this function runs on the libWebRTCCodecsQueue
// and m_decoders only get modified on this queue.
void LibWebRTCCodecsProxy::setFrameSize(RTCDecoderIdentifier identifier, uint16_t width, uint16_t height) WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    ASSERT(!isMainRunLoop());
    ASSERT(m_decoders.contains(identifier));
    auto decoder = m_decoders.get(identifier);
    if (!decoder)
        return;

    webrtc::setDecoderFrameSize(decoder, width, height);
}

void LibWebRTCCodecsProxy::createEncoder(RTCEncoderIdentifier identifier, const String& formatName, const Vector<std::pair<String, String>>& parameters, bool useLowLatency)
{
    ASSERT(!isMainRunLoop());
    Locker locker { m_lock };
    ASSERT(!m_encoders.contains(identifier));

    std::map<std::string, std::string> rtcParameters;
    for (auto& parameter : parameters)
        rtcParameters.emplace(parameter.first.utf8().data(), parameter.second.utf8().data());

    auto* encoder = webrtc::createLocalEncoder(webrtc::SdpVideoFormat { formatName.utf8().data(), rtcParameters }, makeBlockPtr([connection = Ref { m_gpuConnectionToWebProcess.connection() }, identifier](const uint8_t* buffer, size_t size, const webrtc::WebKitEncodedFrameInfo& info) {
        connection->send(Messages::LibWebRTCCodecs::CompletedEncoding { identifier, IPC::DataReference { buffer, size }, info }, 0);
    }).get());
    webrtc::setLocalEncoderLowLatency(encoder, useLowLatency);
    m_encoders.add(identifier, Encoder { encoder, nullptr });
}

void LibWebRTCCodecsProxy::releaseEncoder(RTCEncoderIdentifier identifier)
{
    ASSERT(!isMainRunLoop());
    Locker locker { m_lock };
    ASSERT(m_encoders.contains(identifier));
    auto encoder = m_encoders.take(identifier);
    if (encoder.webrtcEncoder)
        webrtc::releaseLocalEncoder(encoder.webrtcEncoder);
}

// For performance reasons, this function accesses m_encoders without locking. This is safe because this function runs on the libWebRTCCodecsQueue
// and m_encoders only get modified on this queue.
void LibWebRTCCodecsProxy::initializeEncoder(RTCEncoderIdentifier identifier, uint16_t width, uint16_t height, unsigned startBitrate, unsigned maxBitrate, unsigned minBitrate, uint32_t maxFramerate)
{
    auto* encoder = findEncoderWithoutLock(identifier);
    if (!encoder)
        return;

    webrtc::initializeLocalEncoder(encoder->webrtcEncoder, width, height, startBitrate, maxBitrate, minBitrate, maxFramerate);
}

LibWebRTCCodecsProxy::Encoder* LibWebRTCCodecsProxy::findEncoderWithoutLock(RTCEncoderIdentifier identifier) WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    ASSERT(!isMainRunLoop());
    auto iterator = m_encoders.find(identifier);
    if (iterator == m_encoders.end())
        return nullptr;
    return &iterator->value;
}

static inline webrtc::VideoRotation toWebRTCVideoRotation(WebCore::MediaSample::VideoRotation rotation)
{
    switch (rotation) {
    case WebCore::MediaSample::VideoRotation::None:
        return webrtc::kVideoRotation_0;
    case WebCore::MediaSample::VideoRotation::UpsideDown:
        return webrtc::kVideoRotation_180;
    case WebCore::MediaSample::VideoRotation::Right:
        return webrtc::kVideoRotation_90;
    case WebCore::MediaSample::VideoRotation::Left:
        return webrtc::kVideoRotation_270;
    }
    ASSERT_NOT_REACHED();
    return webrtc::kVideoRotation_0;
}

// For performance reasons, this function accesses m_encoders without locking. This is safe because this function runs on the libWebRTCCodecsQueue
// and m_encoders only get modified on this queue.
void LibWebRTCCodecsProxy::encodeFrame(RTCEncoderIdentifier identifier, WebCore::RemoteVideoSample&& sample, uint32_t timeStamp, bool shouldEncodeAsKeyFrame, std::optional<RemoteVideoFrameReadReference> sampleReference)
{
    RetainPtr<CVPixelBufferRef> pixelBuffer;
    if (sampleReference) {
        auto sample = m_videoFrameObjectHeap->get(WTFMove(*sampleReference));
        if (!sample)
            return;

        auto platformSample = sample->platformSample();
        ASSERT(platformSample.type == WebCore::PlatformSample::CMSampleBufferType);
        pixelBuffer = static_cast<CVPixelBufferRef>(PAL::CMSampleBufferGetImageBuffer(platformSample.sample.cmSampleBuffer));
    }

    ASSERT(findEncoderWithoutLock(identifier));
    auto* encoder = findEncoderWithoutLock(identifier);
    if (!encoder)
        return;

#if !PLATFORM(MACCATALYST)
    if (!pixelBuffer) {
        if (sample.surface()) {
            if (auto buffer = WebCore::createCVPixelBuffer(sample.surface()))
                pixelBuffer = WTFMove(*buffer);
        } else if (encoder->frameReader)
            pixelBuffer = encoder->frameReader->read();

        if (!pixelBuffer)
            return;
    }

    webrtc::encodeLocalEncoderFrame(encoder->webrtcEncoder, pixelBuffer.get(), sample.time().toTimeScale(1000000).timeValue(), timeStamp, toWebRTCVideoRotation(sample.rotation()), shouldEncodeAsKeyFrame);
#endif
}

// For performance reasons, this function accesses m_encoders without locking. This is safe because this function runs on the libWebRTCCodecsQueue
// and m_encoders only get modified on this queue.
void LibWebRTCCodecsProxy::setEncodeRates(RTCEncoderIdentifier identifier, uint32_t bitRate, uint32_t frameRate)
{
    auto* encoder = findEncoderWithoutLock(identifier);
    if (!encoder)
        return;

    webrtc::setLocalEncoderRates(encoder->webrtcEncoder, bitRate, frameRate);
}

// For performance reasons, this function accesses m_encoders without locking. This is safe because this function runs on the libWebRTCCodecsQueue
// and m_encoders only get modified on this queue.
void LibWebRTCCodecsProxy::setSharedVideoFrameSemaphore(RTCEncoderIdentifier identifier, IPC::Semaphore&& semaphore)
{
    ASSERT(findEncoderWithoutLock(identifier));
    auto* encoder = findEncoderWithoutLock(identifier);
    if (!encoder)
        return;

    if (!encoder->frameReader)
        encoder->frameReader = makeUnique<SharedVideoFrameReader>(Ref { m_videoFrameObjectHeap }, m_resourceOwner);
    encoder->frameReader->setSemaphore(WTFMove(semaphore));
}

// For performance reasons, this function accesses m_encoders without locking. This is safe because this function runs on the libWebRTCCodecsQueue
// and m_encoders only get modified on this queue.
void LibWebRTCCodecsProxy::setSharedVideoFrameMemory(RTCEncoderIdentifier identifier, const SharedMemory::IPCHandle& ipcHandle)
{
    ASSERT(findEncoderWithoutLock(identifier));
    auto* encoder = findEncoderWithoutLock(identifier);
    if (!encoder)
        return;

    if (!encoder->frameReader)
        encoder->frameReader = makeUnique<SharedVideoFrameReader>(Ref { m_videoFrameObjectHeap }, m_resourceOwner);
    encoder->frameReader->setSharedMemory(ipcHandle);
}

bool LibWebRTCCodecsProxy::allowsExitUnderMemoryPressure() const
{
    ASSERT(isMainRunLoop());
    Locker locker { m_lock };
    return m_encoders.isEmpty() && m_decoders.isEmpty();
}

void LibWebRTCCodecsProxy::setRTCLoggingLevel(WTFLogLevel level)
{
    WebCore::LibWebRTCProvider::setRTCLogging(level);
}

}

#endif
