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
#import "IPCUtilities.h"
#import "LibWebRTCCodecsMessages.h"
#import "LibWebRTCCodecsProxyMessages.h"
#import "Logging.h"
#import "RemoteVideoFrameIdentifier.h"
#import "RemoteVideoFrameObjectHeap.h"
#import "SharedVideoFrame.h"
#import "WebCoreArgumentCoders.h"
#import <WebCore/CVUtilities.h>
#import <WebCore/FrameRateMonitor.h>
#import <WebCore/HEVCUtilitiesCocoa.h>
#import <WebCore/LibWebRTCProvider.h>
#import <WebCore/PixelBufferConformerCV.h>
#import <WebCore/VideoFrameCV.h>
#import <wtf/BlockPtr.h>
#import <wtf/MediaTime.h>

ALLOW_COMMA_BEGIN

#include <webrtc/sdk/WebKit/WebKitDecoder.h>
#include <webrtc/sdk/WebKit/WebKitEncoder.h>

ALLOW_COMMA_END

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
        decoders.clear();
        auto encoders = WTFMove(m_encoders);
        for (auto& encoder : encoders.values())
            webrtc::releaseLocalEncoder(encoder.webrtcEncoder);
    });
}

void LibWebRTCCodecsProxy::initialize()
{
    m_connection->addWorkQueueMessageReceiver(Messages::LibWebRTCCodecsProxy::messageReceiverName(), m_queue, *this);
}

auto LibWebRTCCodecsProxy::createDecoderCallback(VideoDecoderIdentifier identifier, bool useRemoteFrames, bool enableAdditionalLogging)
{
    RefPtr<RemoteVideoFrameObjectHeap> videoFrameObjectHeap;
    if (useRemoteFrames)
        videoFrameObjectHeap = m_videoFrameObjectHeap.ptr();
    std::unique_ptr<FrameRateMonitor> frameRateMonitor;
    if (enableAdditionalLogging) {
        frameRateMonitor = makeUnique<FrameRateMonitor>([identifier](auto info) {
            callOnMainRunLoop([identifier, info] {
                auto frameTime = info.frameTime.secondsSinceEpoch().value();
                auto lastFrameTime = info.lastFrameTime.secondsSinceEpoch().value();
                RELEASE_LOG(WebRTC, "LibWebRTCCodecsProxy decoder %" PRIu64 " generated a video frame at %f, previous frame was at %f, observed frame rate is %f, delay since last frame is %f ms, frame count is %lu", identifier.toUInt64(), frameTime, lastFrameTime, info.observedFrameRate, (frameTime - lastFrameTime) * 1000, info.frameCount);
            });
        });
    }
    return [identifier, connection = m_connection, resourceOwner = m_resourceOwner, videoFrameObjectHeap = WTFMove(videoFrameObjectHeap), frameRateMonitor = WTFMove(frameRateMonitor)] (CVPixelBufferRef pixelBuffer, int64_t timeStamp, int64_t timeStampNs) mutable {
        if (!pixelBuffer) {
            connection->send(Messages::LibWebRTCCodecs::FailedDecoding { identifier }, 0);
            return;
        }

        if (frameRateMonitor)
            frameRateMonitor->update();

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

std::unique_ptr<WebCore::WebRTCVideoDecoder> LibWebRTCCodecsProxy::createLocalDecoder(VideoDecoderIdentifier identifier, VideoCodecType codecType, bool useRemoteFrames, bool enableAdditionalLogging)
{
    auto block = makeBlockPtr(createDecoderCallback(identifier, useRemoteFrames, enableAdditionalLogging));

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/LibWebRTCCodecsProxyAdditions.mm>
#endif

    switch (codecType) {
    case VideoCodecType::H264:
        return WebRTCVideoDecoder::createFromLocalDecoder(webrtc::createLocalH264Decoder(block.get())).moveToUniquePtr();
    case VideoCodecType::H265:
        return WebRTCVideoDecoder::createFromLocalDecoder(webrtc::createLocalH265Decoder(block.get())).moveToUniquePtr();
    case VideoCodecType::VP9:
        return WebRTCVideoDecoder::createFromLocalDecoder(webrtc::createLocalVP9Decoder(block.get())).moveToUniquePtr();
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

static bool validateCodecString(VideoCodecType codecType, const String& codecString)
{
    // FIXME: Further tighten checks.
    switch (codecType) {
    case VideoCodecType::H264: {
        auto parameters = parseAVCCodecParameters(codecString);
        // Limit to High Profile, level 5.2.
        return parameters && parameters->profileIDC <= 100 && parameters->levelIDC <= 52;
    }
    case VideoCodecType::H265: {
        auto parameters = parseHEVCCodecParameters(codecString);
        return parameters && validateHEVCParameters(*parameters, false, false);
    }
    case VideoCodecType::VP9:
        ASSERT(codecString.startsWith("vp09.0"_s));
        return true;
    case VideoCodecType::AV1:
        if (codecString.startsWith("av01."_s) && codecString.length() > 7)
            return false;
        auto profile = codecString[5];
        return profile == '0' || profile == '1' || profile == '2';
    }
    ASSERT_NOT_REACHED();
    return true;
}

void LibWebRTCCodecsProxy::createDecoder(VideoDecoderIdentifier identifier, VideoCodecType codecType, const String& codecString, bool useRemoteFrames, bool enableAdditionalLogging, CompletionHandler<void(bool)>&& callback)
{
    assertIsCurrent(workQueue());

    if (!codecString.isNull() && !validateCodecString(codecType, codecString)) {
        callback(false);
        return;
    }

    auto decoder = createLocalDecoder(identifier, codecType, useRemoteFrames, enableAdditionalLogging);
    if (!decoder) {
        callback(false);
        return;
    }

    std::unique_ptr<FrameRateMonitor> frameRateMonitor;
    if (enableAdditionalLogging) {
        frameRateMonitor = makeUnique<FrameRateMonitor>([identifier](auto info) {
            callOnMainRunLoop([identifier, info] {
                auto frameTime = info.frameTime.secondsSinceEpoch().value();
                auto lastFrameTime = info.lastFrameTime.secondsSinceEpoch().value();
                RELEASE_LOG(WebRTC, "LibWebRTCCodecsProxy decoder %" PRIu64 " received a compressed frame at %f, previous frame was at %f, observed frame rate is %f, delay since last frame is %f ms, frame count is %lu", identifier.toUInt64(), frameTime, lastFrameTime, info.observedFrameRate, (frameTime - lastFrameTime) * 1000, info.frameCount);
            });
        });
    }

    auto result = m_decoders.add(identifier,  Decoder { WTFMove(decoder), WTFMove(frameRateMonitor) });
    ASSERT_UNUSED(result, result.isNewEntry || IPC::isTestingIPC());
    m_hasEncodersOrDecoders = true;
    callback(true);
}

void LibWebRTCCodecsProxy::releaseDecoder(VideoDecoderIdentifier identifier)
{
    assertIsCurrent(workQueue());
    auto iterator = m_decoders.find(identifier);
    if (iterator == m_decoders.end())
        return;

    m_decoders.remove(iterator);
    m_hasEncodersOrDecoders = !m_encoders.isEmpty() || !m_decoders.isEmpty();
}

void LibWebRTCCodecsProxy::flushDecoder(VideoDecoderIdentifier identifier)
{
    doDecoderTask(identifier, [&](auto& decoder) {
        decoder.webrtcDecoder->flush();
        m_connection->send(Messages::LibWebRTCCodecs::FlushDecoderCompleted { identifier }, 0);
    });
}

void LibWebRTCCodecsProxy::setDecoderFormatDescription(VideoDecoderIdentifier identifier, const IPC::DataReference& data, uint16_t width, uint16_t height)
{
    doDecoderTask(identifier, [&](auto& decoder) {
        decoder.webrtcDecoder->setFormat(data.data(), data.size(), width, height);
    });
}

void LibWebRTCCodecsProxy::decodeFrame(VideoDecoderIdentifier identifier, int64_t timeStamp, const IPC::DataReference& data) WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    doDecoderTask(identifier, [&](auto& decoder) {
        if (decoder.frameRateMonitor)
            decoder.frameRateMonitor->update();
        if (decoder.webrtcDecoder->decodeFrame(timeStamp, data.data(), data.size()))
            m_connection->send(Messages::LibWebRTCCodecs::FailedDecoding { identifier }, 0);
    });
}

void LibWebRTCCodecsProxy::setFrameSize(VideoDecoderIdentifier identifier, uint16_t width, uint16_t height) WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    doDecoderTask(identifier, [&](auto& decoder) {
        decoder.webrtcDecoder->setFrameSize(width, height);
    });
}

void LibWebRTCCodecsProxy::doDecoderTask(VideoDecoderIdentifier identifier, Function<void(Decoder&)>&& task)
{
    assertIsCurrent(workQueue());
    auto iterator = m_decoders.find(identifier);
    if (iterator == m_decoders.end())
        return;

    task(iterator->value);
}

static bool validateEncoderConfiguration(VideoCodecType codecType, const String& codecString, bool useLowLatency, VideoEncoderScalabilityMode scalabilityMode)
{
    // FIXME: Further tighten checks.
    switch (codecType) {
    case VideoCodecType::H264: {
        if (scalabilityMode != VideoEncoderScalabilityMode::L1T1 && scalabilityMode != VideoEncoderScalabilityMode::L1T2)
            return false;

        if (scalabilityMode == VideoEncoderScalabilityMode::L1T2 && !useLowLatency)
            return false;

        if (!codecString.isNull()) {
            auto parameters = parseAVCCodecParameters(codecString);
            // Limit to High Profile, level 5.2.
            return parameters && parameters->profileIDC <= 100 && parameters->levelIDC <= 52;
        }
        return true;
    }
    case VideoCodecType::H265: {
        if (scalabilityMode != VideoEncoderScalabilityMode::L1T1)
            return false;

        if (!codecString.isNull()) {
            auto parameters = parseHEVCCodecParameters(codecString);
            return parameters && validateHEVCParameters(*parameters, false, false);
        }
        return true;
    }
    case VideoCodecType::VP9:
    case VideoCodecType::AV1:
        break;
    }
    ASSERT_NOT_REACHED();
    return true;
}

void LibWebRTCCodecsProxy::createEncoder(VideoEncoderIdentifier identifier, VideoCodecType codecType, const String& codecString, const Vector<std::pair<String, String>>& parameters, bool useLowLatency, bool useAnnexB, VideoEncoderScalabilityMode scalabilityMode, CompletionHandler<void(bool)>&& callback)
{
    assertIsCurrent(workQueue());
    std::map<std::string, std::string> rtcParameters;
    for (auto& parameter : parameters)
        rtcParameters.emplace(parameter.first.utf8().data(), parameter.second.utf8().data());

    if (codecType != VideoCodecType::H264 && codecType != VideoCodecType::H265) {
        callback(false);
        return;
    }

    if (!validateEncoderConfiguration(codecType, codecString, useLowLatency, scalabilityMode)) {
        callback(false);
        return;
    }

    auto errorBlock = makeBlockPtr([weakThis = ThreadSafeWeakPtr { *this }, queue = m_queue, identifier](bool result) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->notifyEncoderResult(identifier, result);
    });
    auto newFrameBlock = makeBlockPtr([weakThis = ThreadSafeWeakPtr { *this }, queue = m_queue, connection = m_connection, identifier](const uint8_t* buffer, size_t size, const webrtc::WebKitEncodedFrameInfo& info) {
        connection->send(Messages::LibWebRTCCodecs::CompletedEncoding { identifier, IPC::DataReference { buffer, size }, info }, 0);
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->notifyEncoderResult(identifier, true);
    });
    auto newConfigurationBlock = makeBlockPtr([connection = m_connection, identifier](const uint8_t* buffer, size_t size) {
        // Current encoders are limited to this configuration. We might want in the future to let encoders notify which colorSpace they are selecting.
        PlatformVideoColorSpace colorSpace { PlatformVideoColorPrimaries::Bt709, PlatformVideoTransferCharacteristics::Iec6196621, PlatformVideoMatrixCoefficients::Bt709, true };
        connection->send(Messages::LibWebRTCCodecs::SetEncodingConfiguration { identifier, IPC::DataReference { buffer, size }, colorSpace }, 0);
    });

    webrtc::LocalEncoderScalabilityMode rtcScalabilityMode;
    switch (scalabilityMode) {
    case VideoEncoderScalabilityMode::L1T1:
        rtcScalabilityMode = webrtc::LocalEncoderScalabilityMode::L1T1;
        break;
    case VideoEncoderScalabilityMode::L1T2:
        rtcScalabilityMode = webrtc::LocalEncoderScalabilityMode::L1T2;
        break;
    case VideoEncoderScalabilityMode::L1T3:
        callback(false);
        return;
    }
    auto* encoder = webrtc::createLocalEncoder(webrtc::SdpVideoFormat { codecType == VideoCodecType::H264 ? "H264" : "H265", rtcParameters }, useAnnexB, rtcScalabilityMode, newFrameBlock.get(), newConfigurationBlock.get(), errorBlock.get());
    if (!encoder) {
        callback(false);
        return;
    }

    webrtc::setLocalEncoderLowLatency(encoder, useLowLatency);
    auto result = m_encoders.add(identifier, Encoder { encoder, makeUnique<SharedVideoFrameReader>(Ref { m_videoFrameObjectHeap }, m_resourceOwner), { } });
    ASSERT_UNUSED(result, result.isNewEntry || IPC::isTestingIPC());
    m_hasEncodersOrDecoders = true;
    callback(true);
}

void LibWebRTCCodecsProxy::releaseEncoder(VideoEncoderIdentifier identifier)
{
    assertIsCurrent(workQueue());
    auto encoder = m_encoders.take(identifier);
    if (!encoder.webrtcEncoder)
        return;

    webrtc::releaseLocalEncoder(encoder.webrtcEncoder);

    m_queue->dispatch([encodingCallbacks = WTFMove(encoder.encodingCallbacks)] () mutable {
        while (!encodingCallbacks.isEmpty())
            encodingCallbacks.takeFirst()(-2);
    });

    m_hasEncodersOrDecoders = !m_encoders.isEmpty() || !m_decoders.isEmpty();
}

void LibWebRTCCodecsProxy::initializeEncoder(VideoEncoderIdentifier identifier, uint16_t width, uint16_t height, unsigned startBitrate, unsigned maxBitrate, unsigned minBitrate, uint32_t maxFramerate)
{
    assertIsCurrent(workQueue());
    auto* encoder = findEncoder(identifier);
    if (!encoder)
        return;

    webrtc::initializeLocalEncoder(encoder->webrtcEncoder, width, height, startBitrate, maxBitrate, minBitrate, maxFramerate);
}

LibWebRTCCodecsProxy::Encoder* LibWebRTCCodecsProxy::findEncoder(VideoEncoderIdentifier identifier)
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

void LibWebRTCCodecsProxy::encodeFrame(VideoEncoderIdentifier identifier, SharedVideoFrame&& sharedVideoFrame, int64_t timeStamp, std::optional<uint64_t> duration, bool shouldEncodeAsKeyFrame, CompletionHandler<void(bool)>&& callback)
{
    assertIsCurrent(workQueue());
    auto* encoder = findEncoder(identifier);
    if (!encoder) {
        // Make sure to read RemoteVideoFrameReadReference to prevent memory leaks.
        if (std::holds_alternative<RemoteVideoFrameReadReference>(sharedVideoFrame.buffer))
            m_videoFrameObjectHeap->get(WTFMove(std::get<RemoteVideoFrameReadReference>(sharedVideoFrame.buffer)));
        callback(false);
        return;
    }

    auto pixelBuffer = encoder->frameReader->readBuffer(WTFMove(sharedVideoFrame.buffer));
    if (!pixelBuffer) {
        callback(false);
        return;
    }

    if (CVPixelBufferGetPixelFormatType(pixelBuffer.get()) == kCVPixelFormatType_32BGRA) {
        if (!m_pixelBufferConformer) {
            m_pixelBufferConformer = makeUnique<WebCore::PixelBufferConformerCV>((__bridge CFDictionaryRef)@{ (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange) });
        }
        pixelBuffer = m_pixelBufferConformer->convert(pixelBuffer.get());
        if (!pixelBuffer) {
            callback(false);
            return;
        }
    }

#if !PLATFORM(MACCATALYST)
    encoder->encodingCallbacks.append(WTFMove(callback));
    webrtc::encodeLocalEncoderFrame(encoder->webrtcEncoder, pixelBuffer.get(), sharedVideoFrame.time.toTimeScale(1000000).timeValue(), timeStamp, duration, toWebRTCVideoRotation(sharedVideoFrame.rotation), shouldEncodeAsKeyFrame);
#else
    callback(false);
#endif
}

void LibWebRTCCodecsProxy::notifyEncoderResult(VideoEncoderIdentifier identifier, bool result)
{
    m_queue->dispatch([this, weakThis = ThreadSafeWeakPtr { *this }, identifier, result] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        assertIsCurrent(workQueue());
        if (auto* encoder = findEncoder(identifier))
            encoder->encodingCallbacks.takeFirst()(result);
    });
}

void LibWebRTCCodecsProxy::flushEncoder(VideoEncoderIdentifier identifier, CompletionHandler<void()>&& callback)
{
    assertIsCurrent(workQueue());
    if (auto* encoder = findEncoder(identifier))
        webrtc::flushLocalEncoder(encoder->webrtcEncoder);
    // FIXME: It would be nice to ASSERT that when executing callback, the encoding task deque is empty.
    workQueue().dispatch(WTFMove(callback));
}

void LibWebRTCCodecsProxy::setEncodeRates(VideoEncoderIdentifier identifier, uint32_t bitRate, uint32_t frameRate)
{
    assertIsCurrent(workQueue());
    auto* encoder = findEncoder(identifier);
    if (!encoder)
        return;

    webrtc::setLocalEncoderRates(encoder->webrtcEncoder, bitRate, frameRate);
}

void LibWebRTCCodecsProxy::setSharedVideoFrameSemaphore(VideoEncoderIdentifier identifier, IPC::Semaphore&& semaphore)
{
    assertIsCurrent(workQueue());
    auto* encoder = findEncoder(identifier);
    if (!encoder)
        return;

    encoder->frameReader->setSemaphore(WTFMove(semaphore));
}

void LibWebRTCCodecsProxy::setSharedVideoFrameMemory(VideoEncoderIdentifier identifier, SharedMemory::Handle&& handle)
{
    assertIsCurrent(workQueue());
    auto* encoder = findEncoder(identifier);
    if (!encoder)
        return;

    encoder->frameReader->setSharedMemory(WTFMove(handle));
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
