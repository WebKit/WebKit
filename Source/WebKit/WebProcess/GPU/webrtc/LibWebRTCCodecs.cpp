/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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
#include "LibWebRTCProvider.h"
#include "Logging.h"
#include "RemoteVideoFrameObjectHeapProxy.h"
#include "RemoteVideoFrameProxy.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include <WebCore/CVUtilities.h>
#include <WebCore/LibWebRTCDav1dDecoder.h>
#include <WebCore/LibWebRTCMacros.h>
#include <WebCore/NativeImage.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformMediaSessionManager.h>
#include <WebCore/VP9UtilitiesCocoa.h>
#include <WebCore/VideoFrameCV.h>
#include <wtf/MainThread.h>
#include <wtf/TZoneMallocInlines.h>

ALLOW_COMMA_BEGIN

#include <webrtc/webkit_sdk/WebKit/WebKitDecoder.h>
#include <webrtc/webkit_sdk/WebKit/WebKitEncoder.h>

ALLOW_COMMA_END

#include <pal/cf/CoreMediaSoftLink.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WebKit {
using namespace WebCore;

static webrtc::WebKitVideoDecoder createVideoDecoder(const webrtc::SdpVideoFormat& format)
{
    auto& codecs = WebProcess::singleton().libWebRTCCodecs();
    auto codecString = String::fromUTF8(format.name);

    if (equalIgnoringASCIICase(codecString, "H264"_s))
        return { codecs.createDecoder(WebCore::VideoCodecType::H264), false };

    if (equalIgnoringASCIICase(codecString, "H265"_s))
        return { codecs.createDecoder(WebCore::VideoCodecType::H265), false };

#if ENABLE(VP9)
    if (equalIgnoringASCIICase(codecString, "VP9"_s) && codecs.isSupportingVP9HardwareDecoder())
        return { codecs.createDecoder(WebCore::VideoCodecType::VP9), false };
#endif

#if ENABLE(AV1)
    if (equalIgnoringASCIICase(codecString, "AV1"_s)) {
        if (codecs.hasAV1HardwareDecoder())
            return { codecs.createDecoder(WebCore::VideoCodecType::AV1), false };
        auto av1Decoder = createLibWebRTCDav1dDecoder().moveToUniquePtr();
        return { av1Decoder.release(), true };
    }
#endif

    return { };
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(LibWebRTCCodecs);
WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED(LibWebRTCCodecsDecoder, LibWebRTCCodecs::Decoder);
WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED(LibWebRTCCodecsEncoder, LibWebRTCCodecs::Encoder);

std::optional<WebCore::VideoCodecType> LibWebRTCCodecs::videoCodecTypeFromWebCodec(const String& codec)
{
    // WebCodecs is assuming case sensitive comparisons.
    if (codec.startsWith("avc1."_s))
        return WebCore::VideoCodecType::H264;

    if (codec.startsWith("vp09.0"_s)) {
        if (!isSupportingVP9HardwareDecoder())
            return { };
        return WebCore::VideoCodecType::VP9;
    }

    if (codec.startsWith("av01."_s)) {
        if (!hasAV1HardwareDecoder())
            return { };
        return WebCore::VideoCodecType::AV1;
    }

    if (codec.startsWith("hev1."_s) || codec.startsWith("hvc1."_s))
        return WebCore::VideoCodecType::H265;

    return { };
}

#if ENABLE(AV1)
void LibWebRTCCodecs::setHasAV1HardwareDecoder(bool hasAV1HardwareDecoder)
{
    m_hasAV1HardwareDecoder = hasAV1HardwareDecoder;
    if (!hasAV1HardwareDecoder)
        return;
    Page::forEachPage([](auto& page) {
        auto& settings = page.settings();
        if (settings.webRTCAV1CodecEnabled() && settings.webCodecsAV1Enabled())
            return;
        settings.setWebRTCAV1CodecEnabled(true);
        settings.setWebCodecsAV1Enabled(true);
        page.settingsDidChange();
    });
}
#endif

std::optional<WebCore::VideoCodecType> LibWebRTCCodecs::videoEncoderTypeFromWebCodec(const String& codec)
{
    // WebCodecs is assuming case sensitive comparisons.
    if (codec.startsWith("avc1."_s))
        return WebCore::VideoCodecType::H264;

    if (codec.startsWith("hev1."_s) || codec.startsWith("hvc1."_s))
        return WebCore::VideoCodecType::H265;

    return { };
}

static int32_t releaseVideoDecoder(webrtc::WebKitVideoDecoder::Value decoder)
{
    return WebProcess::singleton().libWebRTCCodecs().releaseDecoder(*static_cast<LibWebRTCCodecs::Decoder*>(decoder));
}

static int32_t decodeVideoFrame(webrtc::WebKitVideoDecoder::Value decoder, uint32_t timeStamp, const uint8_t* data, size_t size, uint16_t width,  uint16_t height)
{
    return WebProcess::singleton().libWebRTCCodecs().decodeWebRTCFrame(*static_cast<LibWebRTCCodecs::Decoder*>(decoder), timeStamp, { data, size }, width, height);
}

static int32_t registerDecodeCompleteCallback(webrtc::WebKitVideoDecoder::Value decoder, void* decodedImageCallback)
{
    WebProcess::singleton().libWebRTCCodecs().registerDecodeFrameCallback(*static_cast<LibWebRTCCodecs::Decoder*>(decoder), decodedImageCallback);
    return 0;
}

static webrtc::WebKitVideoEncoder createVideoEncoder(const webrtc::SdpVideoFormat& format)
{
    if (format.name == "H264" || format.name == "h264")
        return WebProcess::singleton().libWebRTCCodecs().createEncoder(WebCore::VideoCodecType::H264, format.parameters);

    if (format.name == "H265" || format.name == "h265")
        return WebProcess::singleton().libWebRTCCodecs().createEncoder(WebCore::VideoCodecType::H265, format.parameters);

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

static void createRemoteDecoder(LibWebRTCCodecs::Decoder& decoder, IPC::Connection& connection, bool useRemoteFrames, bool enableAdditionalLogging, Function<void(bool)>&& callback)
{
    connection.sendWithAsyncReplyOnDispatcher(Messages::LibWebRTCCodecsProxy::CreateDecoder { decoder.identifier, decoder.type, decoder.codec, useRemoteFrames, enableAdditionalLogging }, WebProcess::singleton().libWebRTCCodecs().workQueue(), WTFMove(callback), 0);
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
    uint32_t bitRateInKbps = parameters.bitrate.get_sum_kbps();
    uint32_t frameRate = static_cast<uint32_t>(parameters.framerate_fps + 0.5);
    WebProcess::singleton().libWebRTCCodecs().setEncodeRates(*static_cast<LibWebRTCCodecs::Encoder*>(encoder), bitRateInKbps, frameRate);
}

Ref<LibWebRTCCodecs> LibWebRTCCodecs::create()
{
    return adoptRef(*new LibWebRTCCodecs);
}

LibWebRTCCodecs::LibWebRTCCodecs()
    : m_queue(WorkQueue::create("LibWebRTCCodecs"_s, WorkQueue::QOS::UserInteractive))
{
}

void LibWebRTCCodecs::initializeIfNeeded()
{
    // Let's create the GPUProcess connection once to know whether we should do VP9 in GPUProcess.
    static std::once_flag doInitializationOnce;
    std::call_once(doInitializationOnce, [] {
        callOnMainRunLoopAndWait([] {
#if HAVE(AUDIT_TOKEN)
            WebProcess::singleton().ensureGPUProcessConnection().auditToken();
#else
            WebProcess::singleton().ensureGPUProcessConnection();
#endif
        });
    });
}

void LibWebRTCCodecs::ensureGPUProcessConnectionOnMainThreadWithLock()
{
    ASSERT(isMainRunLoop());
    if (m_connection)
        return;

    Ref gpuConnection = WebProcess::singleton().ensureGPUProcessConnection();
    gpuConnection->addClient(*this);
    m_connection = &gpuConnection->connection();
    m_videoFrameObjectHeapProxy = &gpuConnection->videoFrameObjectHeapProxy();
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

    if (!LibWebRTCProvider::webRTCAvailable())
        return;

    // We can enable GPUProcess but disable it is difficult once enabled since callbacks may be used in background threads.
    if (!useGPUProcess)
        return;

    auto& libWebRTCCodecs = WebProcess::singleton().libWebRTCCodecs();
    libWebRTCCodecs.m_useRemoteFrames = useRemoteFrames;
    if (libWebRTCCodecs.m_useGPUProcess)
        return;

    libWebRTCCodecs.m_useGPUProcess = useGPUProcess;

    webrtc::setVideoDecoderCallbacks(createVideoDecoder, releaseVideoDecoder, decodeVideoFrame, registerDecodeCompleteCallback);
    webrtc::setVideoEncoderCallbacks(createVideoEncoder, releaseVideoEncoder, initializeVideoEncoder, encodeVideoFrame, registerEncodeCompleteCallback, setEncodeRatesCallback);
}

void LibWebRTCCodecs::setWebRTCMediaPipelineAdditionalLoggingEnabled(bool enabled)
{
    if (!LibWebRTCProvider::webRTCAvailable())
        return;
    WebProcess::singleton().libWebRTCCodecs().m_enableAdditionalLogging = enabled;
}

// May be called on any thread.
LibWebRTCCodecs::Decoder* LibWebRTCCodecs::createDecoder(WebCore::VideoCodecType type)
{
    return createDecoderInternal(type, { }, [](auto*) { });
}

void LibWebRTCCodecs::createDecoderAndWaitUntilReady(WebCore::VideoCodecType type, const String& codec, Function<void(Decoder*)>&& callback)
{
    createDecoderInternal(type, codec, WTFMove(callback));
}

LibWebRTCCodecs::Decoder* LibWebRTCCodecs::createDecoderInternal(WebCore::VideoCodecType type, const String& codec, Function<void(Decoder*)>&& callback)
{
    auto decoder = makeUnique<Decoder>(VideoDecoderIdentifier::generate());
    auto* result = decoder.get();
    decoder->type = type;
    decoder->codec = codec.isolatedCopy();

    ensureGPUProcessConnectionAndDispatchToThread([this, decoder = WTFMove(decoder), callback = WTFMove(callback)]() mutable {
        assertIsCurrent(workQueue());

        {
            Locker locker { m_connectionLock };
            createRemoteDecoder(*decoder, *protectedConnection(), m_useRemoteFrames, m_enableAdditionalLogging, [identifier = decoder->identifier, callback = WTFMove(callback)](bool result) mutable {
                if (!result) {
                    callback(nullptr);
                    return;
                }

                auto& codecs = WebProcess::singleton().libWebRTCCodecs();
                assertIsCurrent(codecs.workQueue());
                callback(codecs.m_decoders.get(identifier));
            });

            setDecoderConnection(*decoder, m_connection.get());

            auto decoderIdentifier = decoder->identifier;
            ASSERT(!m_decoders.contains(decoderIdentifier));
            m_decoders.add(decoderIdentifier, WTFMove(decoder));
        }
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
Ref<GenericPromise> LibWebRTCCodecs::flushDecoder(Decoder& decoder)
{
    Locker locker { m_connectionLock };
    if (!decoder.connection || decoder.hasError)
        return GenericPromise::createAndResolve();

    return decoder.connection->sendWithPromisedReply(Messages::LibWebRTCCodecsProxy::FlushDecoder { decoder.identifier }, 0)->whenSettled(workQueue(), [] (auto&&) {
        return GenericPromise::createAndResolve();
    });
}

void LibWebRTCCodecs::setDecoderFormatDescription(Decoder& decoder, std::span<const uint8_t> data, uint16_t width, uint16_t height)
{
    Locker locker { m_connectionLock };
    ASSERT(decoder.connection);
    if (!decoder.connection)
        return;

    decoder.connection->send(Messages::LibWebRTCCodecsProxy::SetDecoderFormatDescription { decoder.identifier, data, width, height }, 0);
}

Ref<LibWebRTCCodecs::FramePromise> LibWebRTCCodecs::sendFrameToDecode(Decoder& decoder, int64_t timeStamp, std::span<const uint8_t> data, uint16_t width, uint16_t height)
{
    if (decoder.type == WebCore::VideoCodecType::VP9 && (width || height))
        decoder.connection->send(Messages::LibWebRTCCodecsProxy::SetFrameSize { decoder.identifier, width, height }, 0);

    return decoder.connection->sendWithPromisedReply(Messages::LibWebRTCCodecsProxy::DecodeFrame { decoder.identifier, timeStamp, data }, 0)->whenSettled(workQueue(), [] (auto&& result) mutable {
        if (!result)
            return FramePromise::createAndReject("Decoding task did not complete"_s);

        if (!*result)
            return FramePromise::createAndReject("Decoding task failed"_s);

        return FramePromise::createAndResolve();
    });
}

int32_t LibWebRTCCodecs::decodeWebRTCFrame(Decoder& decoder, int64_t timeStamp, std::span<const uint8_t> data, uint16_t width, uint16_t height)
{
    auto promise = decodeFrameInternal(decoder, timeStamp, data, width, height);
    return promise ? WEBRTC_VIDEO_CODEC_OK : WEBRTC_VIDEO_CODEC_ERROR;
}

Ref<LibWebRTCCodecs::FramePromise> LibWebRTCCodecs::decodeFrame(Decoder& decoder, int64_t timeStamp, std::span<const uint8_t> data, uint16_t width, uint16_t height)
{
    auto promise = decodeFrameInternal(decoder, timeStamp, data, width, height);
    return promise ? promise.releaseNonNull() : FramePromise::createAndReject("Decoding task did not complete"_s);
}

RefPtr<LibWebRTCCodecs::FramePromise> LibWebRTCCodecs::decodeFrameInternal(Decoder& decoder, int64_t timeStamp, std::span<const uint8_t> data, uint16_t width, uint16_t height)
{
    Locker locker { m_connectionLock };
    if (decoder.hasError) {
        decoder.hasError = false;
        return nullptr;
    }

    if (!decoder.connection) {
        FramePromise::AutoRejectProducer producer;
        auto promise = producer.promise();

        decoder.pendingFrames.append({ timeStamp, data, width, height, WTFMove(producer) });
        return promise;
    }

    return sendFrameToDecode(decoder, timeStamp, data, width, height);
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

void LibWebRTCCodecs::completedDecoding(VideoDecoderIdentifier decoderIdentifier, int64_t timeStamp, int64_t timeStampNs, RemoteVideoFrameProxy::Properties&& properties)
{
    assertIsCurrent(workQueue());

    // Adopt RemoteVideoFrameProxy::Properties to RemoteVideoFrameProxy instance before the early outs, so that the reference gets adopted.
    // Typically RemoteVideoFrameProxy::Properties&& sent to destinations that are already removed need to be handled separately.
    // LibWebRTCCodecs is not ever removed, so we do not do this. However, if it ever is, LibWebRTCCodecs::handleMessageToRemovedDestination()
    // needs to be implemented.
    Ref<RemoteVideoFrameProxy> remoteVideoFrame = [&] {
        Locker locker { m_connectionLock };
        return RemoteVideoFrameProxy::create(protectedConnection().releaseNonNull(), *protectedVideoFrameObjectHeapProxy(), WTFMove(properties));
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

static inline webrtc::VideoCodecType toWebRTCCodecType(WebCore::VideoCodecType type)
{
    switch (type) {
    case WebCore::VideoCodecType::H264:
        return webrtc::kVideoCodecH264;
    case WebCore::VideoCodecType::H265:
        return webrtc::kVideoCodecH265;
    case WebCore::VideoCodecType::VP9:
        return webrtc::kVideoCodecVP9;
    case WebCore::VideoCodecType::AV1:
        return webrtc::kVideoCodecAV1;
    }
}

LibWebRTCCodecs::Encoder* LibWebRTCCodecs::createEncoder(WebCore::VideoCodecType type, const std::map<std::string, std::string>& parameters)
{
    return createEncoderInternal(type, { }, parameters, true, true, VideoEncoderScalabilityMode::L1T1, [](auto*) { });
}

#if ENABLE(WEB_CODECS)
void LibWebRTCCodecs::createEncoderAndWaitUntilInitialized(WebCore::VideoCodecType type, const String& codec, const std::map<std::string, std::string>& parameters, const VideoEncoder::Config& config, Function<void(Encoder*)>&& callback)
{
    createEncoderInternal(type, codec, parameters, config.isRealtime, config.useAnnexB, config.scalabilityMode, [config, callback = WTFMove(callback)] (auto* encoder) {
        if (encoder) {
            // Encoders use kbps bit rates.
            WebProcess::singleton().libWebRTCCodecs().initializeEncoderInternal(*encoder, config.width, config.height, config.bitRate / 1000, 2 * config.bitRate / 1000, 0, config.frameRate);
            WebProcess::singleton().libWebRTCCodecs().setEncodeRates(*encoder, config.bitRate / 1000, config.frameRate);
        }
        callback(encoder);
    });
}
#endif // ENABLE(WEB_CODECS)

static void createRemoteEncoder(LibWebRTCCodecs::Encoder& encoder, IPC::Connection& connection, const Vector<std::pair<String, String>>& parameters, Function<void(bool)>&& callback)
{
    connection.sendWithAsyncReplyOnDispatcher(Messages::LibWebRTCCodecsProxy::CreateEncoder { encoder.identifier, encoder.type, encoder.codec, parameters, encoder.isRealtime, encoder.useAnnexB, encoder.scalabilityMode }, WebProcess::singleton().libWebRTCCodecs().workQueue(), WTFMove(callback), 0);
}

LibWebRTCCodecs::Encoder* LibWebRTCCodecs::createEncoderInternal(WebCore::VideoCodecType type, const String& codec, const std::map<std::string, std::string>& formatParameters, bool isRealtime, bool useAnnexB, VideoEncoderScalabilityMode scalabilityMode, Function<void(Encoder*)>&& callback)
{
    auto encoder = makeUnique<Encoder>(VideoEncoderIdentifier::generate());
    auto* result = encoder.get();
    encoder->type = type;
    encoder->useAnnexB = useAnnexB;
    encoder->isRealtime = isRealtime;
    encoder->codec = codec.isolatedCopy();
    encoder->scalabilityMode = scalabilityMode;

    auto parameters = WTF::map(formatParameters, [](auto& entry) {
        return std::pair { String::fromUTF8(entry.first), String::fromUTF8(entry.second) };
    });

    ensureGPUProcessConnectionAndDispatchToThread([this, encoder = WTFMove(encoder), parameters = WTFMove(parameters), callback = WTFMove(callback)]() mutable {
        assertIsCurrent(workQueue());

        auto connection = [&]() -> Ref<IPC::Connection> {
            Locker locker { m_connectionLock };
            return *m_connection;
        }();

        {
            Locker locker { m_encodersConnectionLock };
            createRemoteEncoder(*encoder, connection.get(), parameters, [identifier = encoder->identifier, callback = WTFMove(callback)](bool result) mutable {
                if (!result) {
                    callback(nullptr);
                    return;
                }

                auto& codecs = WebProcess::singleton().libWebRTCCodecs();
                assertIsCurrent(codecs.workQueue());
                callback(codecs.m_encoders.get(identifier));
            });
            setEncoderConnection(*encoder, connection.ptr());
        }

        encoder->parameters = WTFMove(parameters);
        auto encoderIdentifier = encoder->identifier;
        ASSERT(!m_encoders.contains(encoderIdentifier));
        m_encoders.add(encoderIdentifier, WTFMove(encoder));
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

        initializeEncoderInternal(*m_encoders.get(encoderIdentifier), width, height, startBitRate, maxBitRate, minBitRate, maxFrameRate);
    });
    return 0;
}

void LibWebRTCCodecs::initializeEncoderInternal(Encoder& encoder, uint16_t width, uint16_t height, unsigned startBitRate, unsigned maxBitRate, unsigned minBitRate, uint32_t maxFrameRate)
{
    assertIsCurrent(workQueue());

    encoder.initializationData = EncoderInitializationData { width, height, startBitRate, maxBitRate, minBitRate, maxFrameRate };

    Locker locker { m_encodersConnectionLock };
    encoderConnection(encoder)->send(Messages::LibWebRTCCodecsProxy::InitializeEncoder { encoder.identifier, width, height, startBitRate, maxBitRate, minBitRate, maxFrameRate }, 0);
}

template<typename Frame> RefPtr<LibWebRTCCodecs::FramePromise> LibWebRTCCodecs::encodeFrameInternal(Encoder& encoder, const Frame& frame, bool shouldEncodeAsKeyFrame, WebCore::VideoFrame::Rotation rotation, MediaTime mediaTime, int64_t timestamp, std::optional<uint64_t> duration)
{
    Locker locker { m_encodersConnectionLock };
    auto* connection = encoderConnection(encoder);
    if (!connection)
        return nullptr;

    auto buffer = encoder.sharedVideoFrameWriter.writeBuffer(frame,
        [&](auto& semaphore) { encoder.connection->send(Messages::LibWebRTCCodecsProxy::SetSharedVideoFrameSemaphore { encoder.identifier, semaphore }, 0); },
        [&](SharedMemory::Handle&& handle) { encoder.connection->send(Messages::LibWebRTCCodecsProxy::SetSharedVideoFrameMemory { encoder.identifier, WTFMove(handle) }, 0); });
    if (!buffer)
        return nullptr;

    SharedVideoFrame sharedVideoFrame { mediaTime, false, rotation, WTFMove(*buffer) };
    auto promise = encoder.connection->sendWithPromisedReply(Messages::LibWebRTCCodecsProxy::EncodeFrame { encoder.identifier, WTFMove(sharedVideoFrame), timestamp, duration, shouldEncodeAsKeyFrame });
    return promise->whenSettled(workQueue(), [] (auto&& result) mutable {
        if (!result)
            return FramePromise::createAndReject("Encoding task did not complete"_s);

        if (!*result)
            return FramePromise::createAndReject("Encoding task failed"_s);

        return FramePromise::createAndResolve();
    });
}

int32_t LibWebRTCCodecs::encodeFrame(Encoder& encoder, const webrtc::VideoFrame& frame, bool shouldEncodeAsKeyFrame)
{
    auto promise = encodeFrameInternal(encoder, frame, shouldEncodeAsKeyFrame, toVideoRotation(frame.rotation()), MediaTime::createWithDouble(Seconds::fromMicroseconds(frame.timestamp_us()).value()), frame.timestamp(), { });
    return promise ? WEBRTC_VIDEO_CODEC_OK : WEBRTC_VIDEO_CODEC_ERROR;
}

Ref<LibWebRTCCodecs::FramePromise> LibWebRTCCodecs::encodeFrame(Encoder& encoder, const WebCore::VideoFrame& frame, int64_t timestamp, std::optional<uint64_t> duration, bool shouldEncodeAsKeyFrame)
{
    auto promise = encodeFrameInternal(encoder, frame, shouldEncodeAsKeyFrame, frame.rotation(), frame.presentationTime(), timestamp, duration);
    return promise ? promise.releaseNonNull() : FramePromise::createAndReject("Encoding task did not complete"_s);
}

Ref<GenericPromise> LibWebRTCCodecs::flushEncoder(Encoder& encoder)
{
    Locker locker { m_encodersConnectionLock };
    RefPtr connection = encoderConnection(encoder);
    if (!connection)
        return GenericPromise::createAndResolve();

    return connection->sendWithPromisedReply(Messages::LibWebRTCCodecsProxy::FlushEncoder { encoder.identifier })->whenSettled(workQueue(), [] (auto&&) {
        return GenericPromise::createAndResolve();
    });
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

#if ENABLE(WEB_CODECS)
void LibWebRTCCodecs::registerEncoderDescriptionCallback(Encoder& encoder, DescriptionCallback&& callback)
{
    Locker locker { encoder.encodedImageCallbackLock };

    ASSERT(!encoder.encodedImageCallback);
    encoder.descriptionCallback = WTFMove(callback);
}
#endif

Ref<GenericPromise> LibWebRTCCodecs::setEncodeRates(Encoder& encoder, uint32_t bitRateInKbps, uint32_t frameRate)
{
    Locker locker { m_encodersConnectionLock };

    auto* connection = encoderConnection(encoder);
    if (!connection) {
        GenericPromise::AutoRejectProducer producer;
        auto promise = producer.promise();

        ensureGPUProcessConnectionAndDispatchToThread([this, hasSentInitialEncodeRates = &encoder.hasSentInitialEncodeRates, encoderIdentifier = encoder.identifier, bitRateInKbps, frameRate, producer = WTFMove(producer)] () mutable {
            assertIsCurrent(workQueue());
            ASSERT(m_encoders.get(encoderIdentifier));

            // hasSentInitialEncodeRates remains valid as encoder destruction goes through ensureGPUProcessConnectionAndDispatchToThread.
            if (*hasSentInitialEncodeRates)
                return;

            Locker locker { m_connectionLock };
            m_connection->sendWithPromisedReply(Messages::LibWebRTCCodecsProxy::SetEncodeRates { encoderIdentifier, bitRateInKbps, frameRate })->whenSettled(workQueue(), [producer = WTFMove(producer)] (auto&&) {
                producer.resolve();
            });
        });

        return promise;
    }
    encoder.hasSentInitialEncodeRates = true;
    return connection->sendWithPromisedReply(Messages::LibWebRTCCodecsProxy::SetEncodeRates { encoder.identifier, bitRateInKbps, frameRate })->whenSettled(workQueue(), [] (auto&&) {
        return GenericPromise::createAndResolve();
    });
}

void LibWebRTCCodecs::completedEncoding(VideoEncoderIdentifier identifier, std::span<const uint8_t> data, const webrtc::WebKitEncodedFrameInfo& info)
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
        auto temporalIndex = info.temporalIndex >= 0 ? std::make_optional<unsigned>(info.temporalIndex) : std::nullopt;
        encoder->encoderCallback({ data.data(), data.size() }, info.frameType == webrtc::VideoFrameType::kVideoFrameKey, info.timeStamp, info.duration, temporalIndex);
        return;
    }

    if (!encoder->encodedImageCallback)
        return;

    webrtc::encoderVideoTaskComplete(encoder->encodedImageCallback, toWebRTCCodecType(encoder->type), data.data(), data.size(), info);
}

void LibWebRTCCodecs::setEncodingConfiguration(WebKit::VideoEncoderIdentifier identifier, std::span<const uint8_t> description, std::optional<WebCore::PlatformVideoColorSpace> colorSpace)
{
    assertIsCurrent(workQueue());

    // FIXME: Do error logging.
    auto* encoder = m_encoders.get(identifier);
    if (!encoder)
        return;

    if (!encoder->encodedImageCallbackLock.tryLock())
        return;

    Locker locker { AdoptLock, encoder->encodedImageCallbackLock };

#if ENABLE(WEB_CODECS)
    if (encoder->descriptionCallback) {
        std::optional<Vector<uint8_t>> decoderDescriptionData;
        if (description.size())
            decoderDescriptionData = Vector<uint8_t> { description };
        encoder->descriptionCallback(WebCore::VideoEncoderActiveConfiguration { { }, { }, { }, { }, { }, WTFMove(decoderDescriptionData), WTFMove(colorSpace) });
    }
#endif
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
                createRemoteDecoder(*decoder, *connection, m_useRemoteFrames, m_enableAdditionalLogging, [](auto) { });
                setDecoderConnection(*decoder, connection.get());
            }
        }

        // In case we are waiting for GPUProcess, let's end the wait to not deadlock.
        for (auto& encoder : m_encoders.values())
            encoder->sharedVideoFrameWriter.disable();

        Locker locker { m_encodersConnectionLock };
        for (auto& encoder : m_encoders.values()) {
            createRemoteEncoder(*encoder, *connection, encoder->parameters, [](bool) { });
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
    auto frames = std::exchange(decoder.pendingFrames, { });
    for (auto& frame : frames)
        sendFrameToDecode(decoder, frame.timeStamp, frame.data.span(), frame.width, frame.height)->chainTo(WTFMove(frame.producer));
}

inline RefPtr<RemoteVideoFrameObjectHeapProxy> LibWebRTCCodecs::protectedVideoFrameObjectHeapProxy() const
{
    return m_videoFrameObjectHeapProxy;
}

}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif
