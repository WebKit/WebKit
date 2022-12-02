/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
#include "LibWebRTCCodecs.h"

#if USE(LIBWEBRTC) && PLATFORM(COCOA) && ENABLE(GPU_PROCESS)

#include "GPUProcessConnection.h"
#include "LibWebRTCCodecsMessages.h"
#include "LibWebRTCCodecsProxyMessages.h"
#include "Logging.h"
#include "RemoteVideoFrameObjectHeapProxy.h"
#include "RemoteVideoFrameProxy.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include <WebCore/CVUtilities.h>
#include <WebCore/DeprecatedGlobalSettings.h>
#include <WebCore/LibWebRTCMacros.h>
#include <WebCore/PlatformMediaSessionManager.h>
#include <WebCore/VP9UtilitiesCocoa.h>
#include <WebCore/VideoFrameCV.h>
#include <wtf/MainThread.h>

ALLOW_COMMA_BEGIN

#include <webrtc/sdk/WebKit/WebKitDecoder.h>
#include <webrtc/sdk/WebKit/WebKitEncoder.h>

ALLOW_COMMA_END

#include <pal/cf/CoreMediaSoftLink.h>

namespace WebKit {
using namespace WebCore;

static webrtc::WebKitVideoDecoder createVideoDecoder(const webrtc::SdpVideoFormat& format)
{
    auto& codecs = WebProcess::singleton().libWebRTCCodecs();
    if (format.name == "H264")
        return codecs.createDecoder(VideoCodecType::H264);

    if (format.name == "H265")
        return codecs.createDecoder(VideoCodecType::H265);

    if (format.name == "VP9" && codecs.supportVP9VTB())
        return codecs.createDecoder(VideoCodecType::VP9);

    return nullptr;
}

std::optional<VideoCodecType> LibWebRTCCodecs::videoCodecTypeFromWebCodec(const String& codec)
{
    if (codec.startsWith("vp9.0"_s)) {
        if (!supportVP9VTB())
            return { };
        return VideoCodecType::VP9;
    }

    if (codec.startsWith("avc1."_s))
        return VideoCodecType::H264;

    // FIXME: Expose H265 if available.
    return { };
}

static int32_t releaseVideoDecoder(webrtc::WebKitVideoDecoder decoder)
{
    return WebProcess::singleton().libWebRTCCodecs().releaseDecoder(*static_cast<LibWebRTCCodecs::Decoder*>(decoder));
}

static int32_t decodeVideoFrame(webrtc::WebKitVideoDecoder decoder, uint32_t timeStamp, const uint8_t* data, size_t size, uint16_t width,  uint16_t height)
{
    return WebProcess::singleton().libWebRTCCodecs().decodeFrame(*static_cast<LibWebRTCCodecs::Decoder*>(decoder), timeStamp, data, size, width, height);
}

static int32_t registerDecodeCompleteCallback(webrtc::WebKitVideoDecoder decoder, void* decodedImageCallback)
{
    WebProcess::singleton().libWebRTCCodecs().registerDecodeFrameCallback(*static_cast<LibWebRTCCodecs::Decoder*>(decoder), decodedImageCallback);
    return 0;
}

static webrtc::WebKitVideoEncoder createVideoEncoder(const webrtc::SdpVideoFormat& format)
{
    if (format.name == "H264")
        return WebProcess::singleton().libWebRTCCodecs().createEncoder(VideoCodecType::H264, format.parameters);

    if (format.name == "H265")
        return WebProcess::singleton().libWebRTCCodecs().createEncoder(VideoCodecType::H265, format.parameters);

    return nullptr;
}

static int32_t releaseVideoEncoder(webrtc::WebKitVideoEncoder encoder)
{
    return WebProcess::singleton().libWebRTCCodecs().releaseEncoder(*static_cast<LibWebRTCCodecs::Encoder*>(encoder));
}

static int32_t initializeVideoEncoder(webrtc::WebKitVideoEncoder encoder, const webrtc::VideoCodec& codec)
{
    return WebProcess::singleton().libWebRTCCodecs().initializeEncoder(*static_cast<LibWebRTCCodecs::Encoder*>(encoder), codec.width, codec.height, codec.startBitrate, codec.maxBitrate, codec.minBitrate, codec.maxFramerate);
}

static inline VideoFrame::Rotation toVideoRotation(webrtc::VideoRotation rotation)
{
    switch (rotation) {
    case webrtc::kVideoRotation_0:
        return VideoFrame::Rotation::None;
    case webrtc::kVideoRotation_180:
        return VideoFrame::Rotation::UpsideDown;
    case webrtc::kVideoRotation_90:
        return VideoFrame::Rotation::Right;
    case webrtc::kVideoRotation_270:
        return VideoFrame::Rotation::Left;
    }
    ASSERT_NOT_REACHED();
    return VideoFrame::Rotation::None;
}

static void createRemoteDecoder(LibWebRTCCodecs::Decoder& decoder, IPC::Connection& connection, bool useRemoteFrames)
{
    connection.send(Messages::LibWebRTCCodecsProxy::CreateDecoder { decoder.identifier, decoder.type, useRemoteFrames }, 0);
}

static int32_t encodeVideoFrame(webrtc::WebKitVideoEncoder encoder, const webrtc::VideoFrame& frame, bool shouldEncodeAsKeyFrame)
{
    return WebProcess::singleton().libWebRTCCodecs().encodeFrame(*static_cast<LibWebRTCCodecs::Encoder*>(encoder), frame, shouldEncodeAsKeyFrame);
}

static int32_t registerEncodeCompleteCallback(webrtc::WebKitVideoEncoder encoder, void* encodedImageCallback)
{
    WebProcess::singleton().libWebRTCCodecs().registerEncodeFrameCallback(*static_cast<LibWebRTCCodecs::Encoder*>(encoder), encodedImageCallback);
    return 0;
}

static void setEncodeRatesCallback(webrtc::WebKitVideoEncoder encoder, const webrtc::VideoEncoder::RateControlParameters& parameters)
{
    uint32_t bitRate = parameters.bitrate.get_sum_kbps();
    uint32_t frameRate = static_cast<uint32_t>(parameters.framerate_fps + 0.5);
    WebProcess::singleton().libWebRTCCodecs().setEncodeRates(*static_cast<LibWebRTCCodecs::Encoder*>(encoder), bitRate, frameRate);
}

Ref<LibWebRTCCodecs> LibWebRTCCodecs::create()
{
    return adoptRef(*new LibWebRTCCodecs);
}

LibWebRTCCodecs::LibWebRTCCodecs()
    : m_queue(WorkQueue::create("LibWebRTCCodecs", WorkQueue::QOS::UserInteractive))
{
}

void LibWebRTCCodecs::ensureGPUProcessConnectionOnMainThreadWithLock()
{
    ASSERT(isMainRunLoop());
    if (m_connection)
        return;

    auto& gpuConnection = WebProcess::singleton().ensureGPUProcessConnection();
    gpuConnection.addClient(*this);
    m_connection = &gpuConnection.connection();
    m_videoFrameObjectHeapProxy = &gpuConnection.videoFrameObjectHeapProxy();
    m_connection->addWorkQueueMessageReceiver(Messages::LibWebRTCCodecs::messageReceiverName(), m_queue, *this);

    if (m_loggingLevel)
        m_connection->send(Messages::LibWebRTCCodecsProxy::SetRTCLoggingLevel { *m_loggingLevel }, 0);
}

// May be called on any thread.
void LibWebRTCCodecs::ensureGPUProcessConnectionAndDispatchToThread(Function<void()>&& task)
{
    m_needsGPUProcessConnection = true;

    Locker locker { m_connectionLock };

    // Fast path when we already have a connection.
    if (m_connection) {
        m_queue->dispatch(WTFMove(task));
        return;
    }

    // We don't have a connection to the GPUProcess yet, we need to hop to the main thread to initiate it.
    m_tasksToDispatchAfterEstablishingConnection.append(WTFMove(task));
    if (m_tasksToDispatchAfterEstablishingConnection.size() != 1)
        return;

    callOnMainRunLoop([this] {
        Locker locker { m_connectionLock };
        ensureGPUProcessConnectionOnMainThreadWithLock();
        for (auto& task : std::exchange(m_tasksToDispatchAfterEstablishingConnection, { }))
            m_queue->dispatch(WTFMove(task));
    });
}

void LibWebRTCCodecs::gpuProcessConnectionMayNoLongerBeNeeded()
{
    assertIsCurrent(workQueue());

    if (m_encoders.isEmpty() && m_decoders.isEmpty())
        m_needsGPUProcessConnection = false;
}

LibWebRTCCodecs::~LibWebRTCCodecs()
{
    ASSERT_NOT_REACHED();
}

void LibWebRTCCodecs::setCallbacks(bool useGPUProcess, bool useRemoteFrames)
{
    ASSERT(isMainRunLoop());

    if (!useGPUProcess) {
        webrtc::setVideoDecoderCallbacks(nullptr, nullptr, nullptr, nullptr);
        webrtc::setVideoEncoderCallbacks(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        return;
    }

    // Let's create WebProcess libWebRTCCodecs since it may be called from various threads.
    WebProcess::singleton().libWebRTCCodecs().m_useRemoteFrames = useRemoteFrames;

#if ENABLE(VP9)
    WebProcess::singleton().libWebRTCCodecs().setVP9VTBSupport(WebProcess::singleton().ensureGPUProcessConnection().hasVP9HardwareDecoder());
#endif

    webrtc::setVideoDecoderCallbacks(createVideoDecoder, releaseVideoDecoder, decodeVideoFrame, registerDecodeCompleteCallback);
    webrtc::setVideoEncoderCallbacks(createVideoEncoder, releaseVideoEncoder, initializeVideoEncoder, encodeVideoFrame, registerEncodeCompleteCallback, setEncodeRatesCallback);
}

// May be called on any thread.
LibWebRTCCodecs::Decoder* LibWebRTCCodecs::createDecoder(VideoCodecType type)
{
    return createDecoderInternal(type, [](auto&) { });
}

void LibWebRTCCodecs::createDecoderAndWaitUntilReady(VideoCodecType type, Function<void(Decoder&)>&& callback)
{
    createDecoderInternal(type, WTFMove(callback));
}

LibWebRTCCodecs::Decoder* LibWebRTCCodecs::createDecoderInternal(VideoCodecType type, Function<void(Decoder&)>&& callback)
{
    auto decoder = makeUnique<Decoder>();
    auto* result = decoder.get();
    decoder->identifier = VideoDecoderIdentifier::generateThreadSafe();
    decoder->type = type;

    ensureGPUProcessConnectionAndDispatchToThread([this, decoder = WTFMove(decoder), callback = WTFMove(callback)]() mutable {
        assertIsCurrent(workQueue());

        auto* decodePointer = decoder.get();
        {
            Locker locker { m_connectionLock };
            createRemoteDecoder(*decoder, *m_connection, m_useRemoteFrames);
            setDecoderConnection(*decoder, m_connection.get());

            auto decoderIdentifier = decoder->identifier;
            ASSERT(!m_decoders.contains(decoderIdentifier));
            m_decoders.add(decoderIdentifier, WTFMove(decoder));
        }
        callback(*decodePointer);
    });
    return result;
}

// May be called on any thread.
int32_t LibWebRTCCodecs::releaseDecoder(Decoder& decoder)
{
#if ASSERT_ENABLED
    {
        Locker locker { decoder.decodedImageCallbackLock };
        ASSERT(!decoder.decodedImageCallback);
    }
#endif
    ensureGPUProcessConnectionAndDispatchToThread([this, decoderIdentifier = decoder.identifier] {
        assertIsCurrent(workQueue());

        ASSERT(m_decoders.contains(decoderIdentifier));
        if (auto decoder = m_decoders.take(decoderIdentifier)) {
            Locker locker { m_connectionLock };
            decoderConnection(*decoder)->send(Messages::LibWebRTCCodecsProxy::ReleaseDecoder { decoderIdentifier }, 0);
            gpuProcessConnectionMayNoLongerBeNeeded();
        }
    });
    return 0;
}

// May be called on any thread.
void LibWebRTCCodecs::flushDecoder(Decoder& decoder, Function<void()>&& callback)
{
    Locker locker { m_connectionLock };
    if (!decoder.connection || decoder.hasError) {
        callback();
        return;
    }

    decoder.connection->send(Messages::LibWebRTCCodecsProxy::FlushDecoder { decoder.identifier }, 0);
    Locker flushLocker { decoder.flushCallbacksLock };
    decoder.flushCallbacks.append(WTFMove(callback));
}

void LibWebRTCCodecs::setDecoderFormatDescription(Decoder& decoder, const uint8_t* data, size_t size, uint16_t width, uint16_t height)
{
    Locker locker { m_connectionLock };
    ASSERT(decoder.connection);
    if (!decoder.connection)
        return;

    decoder.connection->send(Messages::LibWebRTCCodecsProxy::SetDecoderFormatDescription { decoder.identifier, IPC::DataReference { data, size }, width, height }, 0);
}

int32_t LibWebRTCCodecs::decodeFrame(Decoder& decoder, int64_t timeStamp, const uint8_t* data, size_t size, uint16_t width, uint16_t height)
{
    Locker locker { m_connectionLock };
    if (!decoder.connection || decoder.hasError) {
        decoder.hasError = false;
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    if (decoder.type == VideoCodecType::VP9 && (width || height))
        decoder.connection->send(Messages::LibWebRTCCodecsProxy::SetFrameSize { decoder.identifier, width, height }, 0);

    decoder.connection->send(Messages::LibWebRTCCodecsProxy::DecodeFrame { decoder.identifier, timeStamp, IPC::DataReference { data, size } }, 0);
    return WEBRTC_VIDEO_CODEC_OK;
}

void LibWebRTCCodecs::registerDecodeFrameCallback(Decoder& decoder, void* decodedImageCallback)
{
    ASSERT(!isMainRunLoop());

    Locker locker { decoder.decodedImageCallbackLock };
    ASSERT(!decoder.decoderCallback);
    decoder.decodedImageCallback = decodedImageCallback;
}

// May be called on any thread.
void LibWebRTCCodecs::registerDecodedVideoFrameCallback(Decoder& decoder, DecoderCallback&& callback)
{
    Locker locker { decoder.decodedImageCallbackLock };
    ASSERT(!decoder.decodedImageCallback);
    decoder.decoderCallback = WTFMove(callback);
}

void LibWebRTCCodecs::failedDecoding(VideoDecoderIdentifier decoderIdentifier)
{
    assertIsCurrent(workQueue());

    if (auto* decoder = m_decoders.get(decoderIdentifier)) {
        decoder->hasError = true;
        Locker locker { decoder->decodedImageCallbackLock };
        if (decoder->decoderCallback)
            decoder->decoderCallback(nullptr, 0);
    }
}

void LibWebRTCCodecs::flushDecoderCompleted(VideoDecoderIdentifier decoderIdentifier)
{
    assertIsCurrent(workQueue());

    auto* decoder = m_decoders.get(decoderIdentifier);
    if (!decoder)
        return;

    Locker locker { decoder->flushCallbacksLock };
    if (!decoder->flushCallbacks.isEmpty())
        decoder->flushCallbacks.takeFirst()();
}

void LibWebRTCCodecs::completedDecoding(VideoDecoderIdentifier decoderIdentifier, int64_t timeStamp, int64_t timeStampNs, RemoteVideoFrameProxy::Properties&& properties)
{
    assertIsCurrent(workQueue());

    // Adopt RemoteVideoFrameProxy::Properties to RemoteVideoFrameProxy instance before the early outs, so that the reference gets adopted.
    // Typically RemoteVideoFrameProxy::Properties&& sent to destinations that are already removed need to be handled separately.
    // LibWebRTCCodecs is not ever removed, so we do not do this. However, if it ever is, LibWebRTCCodecs::handleMessageToRemovedDestination()
    // needs to be implemented.
    Ref<RemoteVideoFrameProxy> remoteVideoFrame = [&] {
        Locker locker { m_connectionLock };
        return RemoteVideoFrameProxy::create(*m_connection, *m_videoFrameObjectHeapProxy, WTFMove(properties));
    }();
    // FIXME: Do error logging.
    auto* decoder = m_decoders.get(decoderIdentifier);
    if (!decoder)
        return;
    if (!decoder->decodedImageCallbackLock.tryLock())
        return;
    Locker locker { AdoptLock, decoder->decodedImageCallbackLock };
    if (decoder->decoderCallback) {
        decoder->decoderCallback(WTFMove(remoteVideoFrame), timeStamp);
        return;
    }
    if (!decoder->decodedImageCallback)
        return;
    auto& frame = remoteVideoFrame.leakRef(); // Balanced by the release callback of videoDecoderTaskComplete.
    webrtc::videoDecoderTaskComplete(decoder->decodedImageCallback, timeStamp, timeStampNs / 1000, &frame,
        [](auto* pointer) { return static_cast<RemoteVideoFrameProxy*>(pointer)->pixelBuffer(); },
        [](auto* pointer) { static_cast<RemoteVideoFrameProxy*>(pointer)->deref(); },
        frame.size().width(), frame.size().height());
}

void LibWebRTCCodecs::completedDecodingCV(VideoDecoderIdentifier decoderIdentifier, int64_t timeStamp, int64_t timeStampNs, RetainPtr<CVPixelBufferRef>&& pixelBuffer)
{
    assertIsCurrent(workQueue());

    // FIXME: Do error logging.
    auto* decoder = m_decoders.get(decoderIdentifier);
    if (!decoder)
        return;
    if (!decoder->decodedImageCallbackLock.tryLock())
        return;
    Locker locker { AdoptLock, decoder->decodedImageCallbackLock };
    if (!pixelBuffer) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (decoder->decoderCallback) {
        decoder->decoderCallback(VideoFrameCV::create(MediaTime { timeStampNs, 1000000000 }, false, VideoFrame::Rotation::None, WTFMove(pixelBuffer)), timeStamp);
        return;
    }

    if (!decoder->decodedImageCallback)
        return;

    webrtc::videoDecoderTaskComplete(decoder->decodedImageCallback, timeStamp, timeStampNs / 1000, pixelBuffer.get());
}

static inline webrtc::VideoCodecType toWebRTCCodecType(VideoCodecType type)
{
    switch (type) {
    case VideoCodecType::H264:
        return webrtc::kVideoCodecH264;
    case VideoCodecType::H265:
        return webrtc::kVideoCodecH265;
    case VideoCodecType::VP9:
        return webrtc::kVideoCodecVP9;
    }
}

LibWebRTCCodecs::Encoder* LibWebRTCCodecs::createEncoder(VideoCodecType type, const std::map<std::string, std::string>& parameters)
{
    return createEncoderInternal(type, parameters, true, true, [](auto&) { });
}

void LibWebRTCCodecs::createEncoderAndWaitUntilReady(VideoCodecType type, const std::map<std::string, std::string>& parameters, bool isRealtime, bool useAnnexB, Function<void(Encoder&)>&& callback)
{
    createEncoderInternal(type, parameters, isRealtime, useAnnexB, WTFMove(callback));
}

LibWebRTCCodecs::Encoder* LibWebRTCCodecs::createEncoderInternal(VideoCodecType type, const std::map<std::string, std::string>& formatParameters, bool isRealtime, bool useAnnexB, Function<void(Encoder&)>&& callback)
{
    auto encoder = makeUnique<Encoder>();
    auto* result = encoder.get();
    encoder->identifier = VideoEncoderIdentifier::generateThreadSafe();
    encoder->type = type;
    encoder->useAnnexB = useAnnexB;
    encoder->isRealtime = isRealtime;

    auto parameters = WTF::map(formatParameters, [](auto& entry) {
        return std::pair { String::fromUTF8(entry.first.data(), entry.first.length()), String::fromUTF8(entry.second.data(), entry.second.length()) };
    });

    ensureGPUProcessConnectionAndDispatchToThread([this, encoder = WTFMove(encoder), parameters = WTFMove(parameters), callback = WTFMove(callback)]() mutable {
        assertIsCurrent(workQueue());

        auto* encoderPointer = encoder.get();

        auto connection = [&]() -> Ref<IPC::Connection> {
            Locker locker { m_connectionLock };
            return *m_connection;
        }();

        {
            Locker locker { m_encodersConnectionLock };
            connection->send(Messages::LibWebRTCCodecsProxy::CreateEncoder { encoder->identifier, encoder->type, parameters, encoder->isRealtime, encoder->useAnnexB }, 0);
            setEncoderConnection(*encoder, connection.ptr());
        }

        encoder->parameters = WTFMove(parameters);
        auto encoderIdentifier = encoder->identifier;
        ASSERT(!m_encoders.contains(encoderIdentifier));
        m_encoders.add(encoderIdentifier, WTFMove(encoder));

        callback(*encoderPointer);
    });
    return result;
}

int32_t LibWebRTCCodecs::releaseEncoder(Encoder& encoder)
{
#if ASSERT_ENABLED
    {
        Locker locker { encoder.encodedImageCallbackLock };
        ASSERT(!encoder.encodedImageCallback);
    }
#endif
    ensureGPUProcessConnectionAndDispatchToThread([this, encoderIdentifier = encoder.identifier] {
        assertIsCurrent(workQueue());

        ASSERT(m_encoders.contains(encoderIdentifier));
        auto encoder = m_encoders.take(encoderIdentifier);

        Locker locker { m_encodersConnectionLock };
        encoderConnection(*encoder)->send(Messages::LibWebRTCCodecsProxy::ReleaseEncoder { encoderIdentifier }, 0);

        gpuProcessConnectionMayNoLongerBeNeeded();
    });
    return 0;
}

int32_t LibWebRTCCodecs::initializeEncoder(Encoder& encoder, uint16_t width, uint16_t height, unsigned startBitRate, unsigned maxBitRate, unsigned minBitRate, uint32_t maxFrameRate)
{
    ASSERT(!isMainRunLoop());

    ensureGPUProcessConnectionAndDispatchToThread([this, encoderIdentifier = encoder.identifier, width, height, startBitRate, maxBitRate, minBitRate, maxFrameRate]() mutable {
        assertIsCurrent(workQueue());

        auto* encoder = m_encoders.get(encoderIdentifier);
        encoder->initializationData = EncoderInitializationData { width, height, startBitRate, maxBitRate, minBitRate, maxFrameRate };

        Locker locker { m_encodersConnectionLock };
        encoderConnection(*encoder)->send(Messages::LibWebRTCCodecsProxy::InitializeEncoder { encoderIdentifier, width, height, startBitRate, maxBitRate, minBitRate, maxFrameRate }, 0);
    });
    return 0;
}

template<typename Frame> int32_t LibWebRTCCodecs::encodeFrameInternal(Encoder& encoder, const Frame& frame, bool shouldEncodeAsKeyFrame, WebCore::VideoFrame::Rotation rotation, MediaTime mediaTime, uint32_t timestamp)
{
    Locker locker { m_encodersConnectionLock };
    auto* connection = encoderConnection(encoder);
    if (!connection)
        return WEBRTC_VIDEO_CODEC_ERROR;

    auto buffer = encoder.sharedVideoFrameWriter.writeBuffer(frame,
        [&](auto& semaphore) { encoder.connection->send(Messages::LibWebRTCCodecsProxy::SetSharedVideoFrameSemaphore { encoder.identifier, semaphore }, 0); },
        [&](auto& handle) { encoder.connection->send(Messages::LibWebRTCCodecsProxy::SetSharedVideoFrameMemory { encoder.identifier, handle }, 0); });
    if (!buffer)
        return WEBRTC_VIDEO_CODEC_ERROR;

    SharedVideoFrame sharedVideoFrame { mediaTime, false, rotation, WTFMove(*buffer) };
    encoder.connection->send(Messages::LibWebRTCCodecsProxy::EncodeFrame { encoder.identifier, sharedVideoFrame, timestamp, shouldEncodeAsKeyFrame }, 0);
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t LibWebRTCCodecs::encodeFrame(Encoder& encoder, const webrtc::VideoFrame& frame, bool shouldEncodeAsKeyFrame)
{
    return encodeFrameInternal(encoder, frame, shouldEncodeAsKeyFrame, toVideoRotation(frame.rotation()), MediaTime(frame.timestamp_us() * 1000, 1000000), frame.timestamp());
}

int32_t LibWebRTCCodecs::encodeFrame(Encoder& encoder, const WebCore::VideoFrame& frame, uint32_t timestamp, bool shouldEncodeAsKeyFrame)
{
    return encodeFrameInternal(encoder, frame, shouldEncodeAsKeyFrame, frame.rotation(), frame.presentationTime(), timestamp);
}

void LibWebRTCCodecs::flushEncoder(Encoder& encoder, Function<void()>&& callback)
{
    Locker locker { m_encodersConnectionLock };
    auto* connection = encoderConnection(encoder);
    if (!connection) {
        callback();
        return;
    }

    connection->send(Messages::LibWebRTCCodecsProxy::FlushEncoder { encoder.identifier }, 0);
    Locker flushLocker { encoder.flushCallbacksLock };
    encoder.flushCallbacks.append(WTFMove(callback));
}

void LibWebRTCCodecs::flushEncoderCompleted(VideoEncoderIdentifier identifier)
{
    assertIsCurrent(workQueue());

    auto* encoder = m_encoders.get(identifier);
    if (!encoder)
        return;

    Locker flushLocker { encoder->flushCallbacksLock };
    if (!encoder->flushCallbacks.isEmpty())
        encoder->flushCallbacks.takeFirst()();
}

void LibWebRTCCodecs::registerEncodeFrameCallback(Encoder& encoder, void* encodedImageCallback)
{
    ASSERT(!isMainRunLoop());

    Locker locker { encoder.encodedImageCallbackLock };

    ASSERT(!encoder.encoderCallback);
    encoder.encodedImageCallback = encodedImageCallback;
}

void LibWebRTCCodecs::registerEncodedVideoFrameCallback(Encoder& encoder, EncoderCallback&& callback)
{
    Locker locker { encoder.encodedImageCallbackLock };

    ASSERT(!encoder.encodedImageCallback);
    encoder.encoderCallback = WTFMove(callback);
}

void LibWebRTCCodecs::registerEncoderDescriptionCallback(Encoder& encoder, DescriptionCallback&& callback)
{
    Locker locker { encoder.encodedImageCallbackLock };

    ASSERT(!encoder.encodedImageCallback);
    encoder.descriptionCallback = WTFMove(callback);
}

void LibWebRTCCodecs::setEncodeRates(Encoder& encoder, uint32_t bitRate, uint32_t frameRate)
{
    ASSERT(!isMainRunLoop());

    Locker locker { m_encodersConnectionLock };

    auto* connection = encoderConnection(encoder);
    if (!connection) {
        ensureGPUProcessConnectionAndDispatchToThread([this, hasSentInitialEncodeRates = &encoder.hasSentInitialEncodeRates, encoderIdentifier = encoder.identifier, bitRate, frameRate] {
            assertIsCurrent(workQueue());
            ASSERT(m_encoders.get(encoderIdentifier));

            // hasSentInitialEncodeRates remains valid as encoder destruction goes through ensureGPUProcessConnectionAndDispatchToThread.
            if (*hasSentInitialEncodeRates)
                return;

            Locker locker { m_connectionLock };
            m_connection->send(Messages::LibWebRTCCodecsProxy::SetEncodeRates { encoderIdentifier, bitRate, frameRate }, 0);
        });
        return;
    }
    encoder.hasSentInitialEncodeRates = true;
    connection->send(Messages::LibWebRTCCodecsProxy::SetEncodeRates { encoder.identifier, bitRate, frameRate }, 0);
}

void LibWebRTCCodecs::completedEncoding(VideoEncoderIdentifier identifier, IPC::DataReference&& data, const webrtc::WebKitEncodedFrameInfo& info)
{
    assertIsCurrent(workQueue());

    // FIXME: Do error logging.
    auto* encoder = m_encoders.get(identifier);
    if (!encoder)
        return;

    if (!encoder->encodedImageCallbackLock.tryLock())
        return;

    Locker locker { AdoptLock, encoder->encodedImageCallbackLock };

    if (encoder->encoderCallback) {
        encoder->encoderCallback({ data.data(), data.size() }, info.frameType == webrtc::VideoFrameType::kVideoFrameKey, info.timeStamp);
        return;
    }

    if (!encoder->encodedImageCallback)
        return;

    webrtc::encoderVideoTaskComplete(encoder->encodedImageCallback, toWebRTCCodecType(encoder->type), data.data(), data.size(), info);
}

void LibWebRTCCodecs::setEncodingDescription(WebKit::VideoEncoderIdentifier identifier, IPC::DataReference&& data)
{
    assertIsCurrent(workQueue());

    // FIXME: Do error logging.
    auto* encoder = m_encoders.get(identifier);
    if (!encoder)
        return;

    if (!encoder->encodedImageCallbackLock.tryLock())
        return;

    Locker locker { AdoptLock, encoder->encodedImageCallbackLock };

    if (encoder->descriptionCallback)
        encoder->descriptionCallback({ data.data(), data.size() });
}

CVPixelBufferPoolRef LibWebRTCCodecs::pixelBufferPool(size_t width, size_t height, OSType type)
{
    if (!m_pixelBufferPool || m_pixelBufferPoolWidth != width || m_pixelBufferPoolHeight != height) {
        m_pixelBufferPool = nullptr;
        m_pixelBufferPoolWidth = 0;
        m_pixelBufferPoolHeight = 0;
        if (auto pool = WebCore::createIOSurfaceCVPixelBufferPool(width, height, type)) {
            m_pixelBufferPool = WTFMove(*pool);
            m_pixelBufferPoolWidth = width;
            m_pixelBufferPoolHeight = height;
        }
    }
    return m_pixelBufferPool.get();
}

void LibWebRTCCodecs::gpuProcessConnectionDidClose(GPUProcessConnection&)
{
    ASSERT(isMainRunLoop());

    Locker locker { m_connectionLock };
    std::exchange(m_connection, nullptr)->removeWorkQueueMessageReceiver(Messages::LibWebRTCCodecs::messageReceiverName());
    if (!m_needsGPUProcessConnection)
        return;

    ensureGPUProcessConnectionOnMainThreadWithLock();
    m_queue->dispatch([this, connection = m_connection]() {
        assertIsCurrent(workQueue());
        {
            Locker locker { m_connectionLock };
            for (auto& decoder : m_decoders.values()) {
                {
                    Locker locker { decoder->flushCallbacksLock };
                    while (!decoder->flushCallbacks.isEmpty())
                        decoder->flushCallbacks.takeFirst()();
                }
                createRemoteDecoder(*decoder, *connection, m_useRemoteFrames);
                setDecoderConnection(*decoder, connection.get());
            }
        }

        // In case we are waiting for GPUProcess, let's end the wait to not deadlock.
        for (auto& encoder : m_encoders.values())
            encoder->sharedVideoFrameWriter.disable();

        Locker locker { m_encodersConnectionLock };
        for (auto& encoder : m_encoders.values()) {
            connection->send(Messages::LibWebRTCCodecsProxy::CreateEncoder { encoder->identifier, encoder->type, encoder->parameters, encoder->isRealtime, encoder->useAnnexB }, 0);
            if (encoder->initializationData)
                connection->send(Messages::LibWebRTCCodecsProxy::InitializeEncoder { encoder->identifier, encoder->initializationData->width, encoder->initializationData->height, encoder->initializationData->startBitRate, encoder->initializationData->maxBitRate, encoder->initializationData->minBitRate, encoder->initializationData->maxFrameRate }, 0);
            setEncoderConnection(*encoder, connection.get());
            encoder->sharedVideoFrameWriter = { };
        }
    });
}

void LibWebRTCCodecs::setLoggingLevel(WTFLogLevel level)
{
    m_loggingLevel = level;

    Locker locker { m_connectionLock };
    if (m_connection)
        m_connection->send(Messages::LibWebRTCCodecsProxy::SetRTCLoggingLevel(level), 0);
}

IPC::Connection* LibWebRTCCodecs::encoderConnection(Encoder& encoder)
{
    return encoder.connection.get();
}

void LibWebRTCCodecs::setEncoderConnection(Encoder& encoder, RefPtr<IPC::Connection>&& connection)
{
    encoder.connection = WTFMove(connection);
}

IPC::Connection* LibWebRTCCodecs::decoderConnection(Decoder& decoder)
{
    return decoder.connection.get();
}

void LibWebRTCCodecs::setDecoderConnection(Decoder& decoder, RefPtr<IPC::Connection>&& connection)
{
    decoder.connection = WTFMove(connection);
}

}

#endif
