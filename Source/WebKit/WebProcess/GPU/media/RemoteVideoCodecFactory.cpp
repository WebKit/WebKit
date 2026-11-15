/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
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
#include "RemoteVideoCodecFactory.h"

#if USE(LIBWEBRTC) && PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(WEB_CODECS)

#include "LibWebRTCCodecs.h"
#include "WebProcess.h"
#include <WebCore/AV1Utilities.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/TrackInfo.h>
#include <WebCore/VP9UtilitiesCocoa.h>
#include <wtf/Borrow.h>
#include <wtf/Deque.h>
#include <wtf/Lock.h>
#include <wtf/StdUnorderedMap.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Variant.h>

namespace WebKit {

class RemoteVideoDecoderCallbacks : public ThreadSafeRefCounted<RemoteVideoDecoderCallbacks> {
public:
    static Ref<RemoteVideoDecoderCallbacks> create(WebCore::VideoDecoder::OutputCallback&& outputCallback) { return adoptRef(*new RemoteVideoDecoderCallbacks(WTF::move(outputCallback))); }

    void notifyDecodingResult(RefPtr<WebCore::VideoFrame>&&, int64_t timestamp);
    void notifySoftwareDecodingResult(std::expected<WebCore::VideoDecoder::DecodedFrame, String>&&);

    NODELETE void close() { m_isClosed = true; }
    void addDuration(int64_t timestamp, uint64_t duration)
    {
        Locker locker { m_timestampToDurationLock };
        m_timestampToDuration.insert_or_assign(timestamp, duration);
    }

private:
    explicit RemoteVideoDecoderCallbacks(WebCore::VideoDecoder::OutputCallback&&);

    WebCore::VideoDecoder::OutputCallback m_outputCallback;
    std::atomic<bool> m_isClosed { false };
    Lock m_timestampToDurationLock;
    StdUnorderedMap<int64_t, uint64_t> m_timestampToDuration WTF_GUARDED_BY_LOCK(m_timestampToDurationLock);
};

class RemoteVideoDecoder : public WebCore::VideoDecoder {
    WTF_MAKE_TZONE_ALLOCATED(RemoteVideoDecoder);
public:
    static Ref<WebCore::VideoDecoder> create(LibWebRTCCodecs::Decoder& decoder, Ref<RemoteVideoDecoderCallbacks>&& callbacks, const String& codec, const Config& config) { return adoptRef(*new RemoteVideoDecoder(decoder, WTF::move(callbacks), codec, config)); }
    ~RemoteVideoDecoder();

private:
    RemoteVideoDecoder(LibWebRTCCodecs::Decoder&, Ref<RemoteVideoDecoderCallbacks>&&, const String& codec, const Config&);

    Ref<DecodePromise> decode(WebCore::VideoEncodedData&&) final;
    Ref<GenericPromise> flush() final;
    void reset() final;
    void close() final;

    void keyFrameDecoded(LibWebRTCCodecs::DecodeFramePromise::Result&&);
    void softwareDecoderCreated(CreatePromise::Result&&);
    Ref<DecodePromise> decodeInternal(WebCore::VideoEncodedData&&) WTF_REQUIRES_LOCK(m_lock);
    void sendPendingKeyFrame() WTF_REQUIRES_LOCK(m_lock);
    Ref<GenericPromise> flushInternal() WTF_REQUIRES_LOCK(m_lock);
    void processQueuedTasks() WTF_REQUIRES_LOCK(m_lock);
    Ref<CreatePromise> createSoftwareDecoder();
    RefPtr<WebCore::VideoInfo> videoInfoFromKeyFrame(const WebCore::VideoEncodedData&) const;

    struct PendingFrame {
        WebCore::VideoEncodedData frame;
        DecodePromise::AutoRejectProducer producer;
    };
    struct PendingFlush {
        GenericPromise::AutoRejectProducer producer;
    };

    void decodeKeyFrameWithSoftwareDecoder(PendingFrame&&) WTF_REQUIRES_LOCK(m_lock);

    LibWebRTCCodecs::Decoder& m_internalDecoder;
    const Ref<RemoteVideoDecoderCallbacks> m_callbacks;
    const String m_codec;
    const Config m_config;
    const bool m_canFallBackToSoftwareDecoder;
    Lock m_lock;
    std::optional<PendingFrame> m_pendingKeyFrame WTF_GUARDED_BY_LOCK(m_lock);
    Deque<Variant<PendingFrame, PendingFlush>> m_queuedTasks WTF_GUARDED_BY_LOCK(m_lock);
    RefPtr<WebCore::VideoDecoder> m_softwareDecoder WTF_GUARDED_BY_LOCK(m_lock);
    RefPtr<WebCore::VideoInfo> m_softwareFallbackVideoInfo WTF_GUARDED_BY_LOCK(m_lock);
    bool m_isClosed WTF_GUARDED_BY_LOCK(m_lock) { false };
};

class RemoteVideoEncoderCallbacks : public ThreadSafeRefCounted<RemoteVideoEncoderCallbacks> {
public:
    static Ref<RemoteVideoEncoderCallbacks> create(WebCore::VideoEncoder::DescriptionCallback&& descriptionCallback, WebCore::VideoEncoder::OutputCallback&& outputCallback) { return adoptRef(*new RemoteVideoEncoderCallbacks(WTF::move(descriptionCallback), WTF::move(outputCallback))); }

    void notifyEncodedChunk(Vector<uint8_t>&&, bool isKeyFrame, int64_t timestamp, std::optional<uint64_t> duration, std::optional<unsigned> temporalIndex, WebCore::VideoFrameRotation, bool isMirrored);
    void notifyEncoderDescription(WebCore::VideoEncoderActiveConfiguration&&);

    NODELETE void close() { m_isClosed = true; }

private:
    RemoteVideoEncoderCallbacks(WebCore::VideoEncoder::DescriptionCallback&&, WebCore::VideoEncoder::OutputCallback&&);

    WebCore::VideoEncoder::DescriptionCallback m_descriptionCallback;
    WebCore::VideoEncoder::OutputCallback m_outputCallback;
    std::atomic<bool> m_isClosed { false };
};

class RemoteVideoEncoder : public WebCore::VideoEncoder {
    WTF_MAKE_TZONE_ALLOCATED(RemoteVideoEncoder);
public:
    static Ref<WebCore::VideoEncoder> create(LibWebRTCCodecs::Encoder& encoder, Ref<RemoteVideoEncoderCallbacks>&& callbacks) { return adoptRef(*new RemoteVideoEncoder(encoder, WTF::move(callbacks))); }
    ~RemoteVideoEncoder();

private:
    RemoteVideoEncoder(LibWebRTCCodecs::Encoder&, Ref<RemoteVideoEncoderCallbacks>&&);

    Ref<EncodePromise> encode(RawFrame&&, bool shouldGenerateKeyFrame) final;
    Ref<GenericPromise> flush() final;
    void reset() final;
    void close() final;
    Ref<GenericPromise> setRates(uint64_t bitRate, double frameRate) final;

    LibWebRTCCodecs::Encoder& m_internalEncoder;
    const Ref<RemoteVideoEncoderCallbacks> m_callbacks;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteVideoCodecFactory);

RemoteVideoCodecFactory::RemoteVideoCodecFactory(WebProcess& process)
{
    ASSERT(isMainRunLoop());
    // We make sure to create libWebRTCCodecs() as it might be called from multiple threads.
    process.libWebRTCCodecs();
    WebCore::VideoDecoder::setCreatorCallback(RemoteVideoCodecFactory::createDecoder);
    WebCore::VideoEncoder::setCreatorCallback(RemoteVideoCodecFactory::createEncoder);
}

RemoteVideoCodecFactory::~RemoteVideoCodecFactory() = default;

NODELETE static bool shouldUseLocalDecoder(std::optional<WebCore::VideoCodecType> type, const WebCore::VideoDecoder::Config& config)
{
    if (!type)
        return true;

#if PLATFORM(MAC)
    if (*type == WebCore::VideoCodecType::VP9 && config.decoding == WebCore::VideoDecoder::HardwareAcceleration::No)
        return true;
#endif

    return false;
}

void RemoteVideoCodecFactory::createDecoder(const String& codec, const WebCore::VideoDecoder::Config& config, WebCore::VideoDecoder::CreateCallback&& createCallback, WebCore::VideoDecoder::OutputCallback&& outputCallback)
{
    LibWebRTCCodecs::initializeIfNeeded();

    Ref libWebRTCCodecs = WebProcess::singleton().libWebRTCCodecs();
    auto type = libWebRTCCodecs->videoCodecTypeFromWebCodec(codec);
    if (shouldUseLocalDecoder(type, config)) {
        WebCore::VideoDecoder::createLocalDecoder(codec, config, WTF::move(createCallback), WTF::move(outputCallback));
        return;
    }
    auto colorSpace = config.colorSpace;
    bool isAnnexB = (*type == WebCore::VideoCodecType::H264 || *type == WebCore::VideoCodecType::H265) && config.description.isEmpty();
    libWebRTCCodecs->createDecoderAndWaitUntilReady(*type, codec, isAnnexB, WTF::move(colorSpace), config.rotation, config.isMirrored, [codec = codec.isolatedCopy(), config, width = config.width, height = config.height, description = Vector<uint8_t> { config.description }, createCallback = WTF::move(createCallback), outputCallback = WTF::move(outputCallback)](auto* internalDecoder) mutable {
        if (!internalDecoder) {
            createCallback(makeUnexpected("Decoder creation failed"_s));
            return;
        }
        if (description.size())
            protect(WebProcess::singleton().libWebRTCCodecs())->setDecoderFormatDescription(*internalDecoder, borrow(description)->span(), width, height);

        auto callbacks = RemoteVideoDecoderCallbacks::create(WTF::move(outputCallback));
        createCallback(RemoteVideoDecoder::create(*internalDecoder, callbacks.copyRef(), codec, config));
    });
}

void RemoteVideoCodecFactory::createEncoder(const String& codec, const WebCore::VideoEncoder::Config& config, WebCore::VideoEncoder::CreateCallback&& createCallback, WebCore::VideoEncoder::DescriptionCallback&& descriptionCallback, WebCore::VideoEncoder::OutputCallback&& outputCallback)
{
    LibWebRTCCodecs::initializeIfNeeded();

    auto type = protect(WebProcess::singleton().libWebRTCCodecs())->videoEncoderTypeFromWebCodec(codec);
    if (!type) {
        WebCore::VideoEncoder::createLocalEncoder(codec, config, WTF::move(createCallback), WTF::move(descriptionCallback), WTF::move(outputCallback));
        return;
    }

    std::map<std::string, std::string> parameters;
    if (type == WebCore::VideoCodecType::H264) {
        if (auto position = codec.find('.');position != notFound && position != codec.length()) {
            auto profileLevelId = byteCast<char>(codec.span8().subspan(position + 1));
            parameters["profile-level-id"] = std::string(profileLevelId.data(), profileLevelId.size());
        }
    }

    protect(WebProcess::singleton().libWebRTCCodecs())->createEncoderAndWaitUntilInitialized(*type, codec, parameters, config, [createCallback = WTF::move(createCallback), descriptionCallback = WTF::move(descriptionCallback), outputCallback = WTF::move(outputCallback)](auto* internalEncoder) mutable {
        if (!internalEncoder) {
            createCallback(makeUnexpected("Encoder creation failed"_s));
            return;
        }
        Ref callbacks = RemoteVideoEncoderCallbacks::create(WTF::move(descriptionCallback), WTF::move(outputCallback));
        createCallback(RemoteVideoEncoder::create(*internalEncoder, WTF::move(callbacks)));
    });
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteVideoDecoder);

static bool canFallBackToSoftwareDecoder(WebCore::VideoCodecType type)
{
    switch (type) {
    case WebCore::VideoCodecType::VP9:
    case WebCore::VideoCodecType::AV1:
        return true;
    case WebCore::VideoCodecType::H264:
    case WebCore::VideoCodecType::H265:
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

RemoteVideoDecoder::RemoteVideoDecoder(LibWebRTCCodecs::Decoder& decoder, Ref<RemoteVideoDecoderCallbacks>&& callbacks, const String& codec, const Config& config)
    : m_internalDecoder(decoder)
    , m_callbacks(WTF::move(callbacks))
    , m_codec(codec.isolatedCopy())
    , m_config(config)
    , m_canFallBackToSoftwareDecoder(canFallBackToSoftwareDecoder(decoder.type))
{
    protect(WebProcess::singleton().libWebRTCCodecs())->registerDecodedVideoFrameCallback(m_internalDecoder, [callbacks = m_callbacks](RefPtr<WebCore::VideoFrame>&& videoFrame, auto timestamp) {
        callbacks->notifyDecodingResult(WTF::move(videoFrame), timestamp);
    });
}

RemoteVideoDecoder::~RemoteVideoDecoder()
{
    protect(WebProcess::singleton().libWebRTCCodecs())->releaseDecoder(m_internalDecoder);
}

Ref<RemoteVideoDecoder::DecodePromise> RemoteVideoDecoder::decode(WebCore::VideoEncodedData&& frame)
{
    Locker locker { m_lock };
    if (m_pendingKeyFrame) {
        DecodePromise::AutoRejectProducer producer;
        Ref promise = producer.promise();
        m_queuedTasks.append(PendingFrame { WTF::move(frame), WTF::move(producer) });
        return promise;
    }
    return decodeInternal(WTF::move(frame));
}

static String decodeFrameErrorMessage(LibWebRTCCodecs::DecodeFrameError error)
{
    switch (error) {
    case LibWebRTCCodecs::DecodeFrameError::DidNotComplete:
        return "Decoding task did not complete"_s;
    case LibWebRTCCodecs::DecodeFrameError::Failed:
        return "Decoding task failed"_s;
    case LibWebRTCCodecs::DecodeFrameError::DecoderCreationFailed:
        return "Decoder creation failed"_s;
    }
    ASSERT_NOT_REACHED();
    return { };
}

Ref<RemoteVideoDecoder::DecodePromise> RemoteVideoDecoder::decodeInternal(WebCore::VideoEncodedData&& frame)
{
    if (RefPtr softwareDecoder = m_softwareDecoder) {
        if (!frame.isKeyFrame)
            return softwareDecoder->decode(WTF::move(frame));

        RefPtr videoInfo = videoInfoFromKeyFrame(frame);
        if (!videoInfo || (m_softwareFallbackVideoInfo && *videoInfo == *m_softwareFallbackVideoInfo))
            return softwareDecoder->decode(WTF::move(frame));

        // The content changed; try the GPU process again once the software decoder has output its pending frames.
        DecodePromise::AutoRejectProducer producer;
        Ref promise = producer.promise();
        m_pendingKeyFrame.emplace(WTF::move(frame), WTF::move(producer));
        softwareDecoder->flush()->whenSettled(protect(WebProcess::singleton().libWebRTCCodecs())->workQueue(), [protectedThis = Ref { *this }](auto&&) {
            Locker locker { protectedThis->m_lock };
            protectedThis->sendPendingKeyFrame();
        });
        return promise;
    }

    if (frame.isKeyFrame && m_canFallBackToSoftwareDecoder) {
        // The GPU process creates its decoder on key frames; later frames wait until it is known whether that succeeded.
        DecodePromise::AutoRejectProducer producer;
        Ref promise = producer.promise();
        m_pendingKeyFrame.emplace(WTF::move(frame), WTF::move(producer));
        sendPendingKeyFrame();
        return promise;
    }

    if (frame.duration)
        m_callbacks->addDuration(frame.timestamp, *frame.duration);

    Ref codecs = WebProcess::singleton().libWebRTCCodecs();
    return codecs->decodeFrame(m_internalDecoder, frame.timestamp, WTF::move(frame.data))->whenSettled(codecs->workQueue(), [](auto&& result) {
        if (!result)
            return DecodePromise::createAndReject(decodeFrameErrorMessage(result.error()));
        return DecodePromise::createAndResolve();
    });
}

void RemoteVideoDecoder::sendPendingKeyFrame()
{
    ASSERT(m_pendingKeyFrame);
    auto& frame = m_pendingKeyFrame->frame;
    if (frame.duration)
        m_callbacks->addDuration(frame.timestamp, *frame.duration);

    Ref codecs = WebProcess::singleton().libWebRTCCodecs();
    codecs->decodeFrame(m_internalDecoder, frame.timestamp, Ref { frame.data })->whenSettled(codecs->workQueue(), [protectedThis = Ref { *this }](auto&& result) {
        protectedThis->keyFrameDecoded(WTF::move(result));
    });
}

void RemoteVideoDecoder::keyFrameDecoded(LibWebRTCCodecs::DecodeFramePromise::Result&& result)
{
    Locker locker { m_lock };
    ASSERT(m_pendingKeyFrame);

    bool decoderCreationFailed = !result && result.error() == LibWebRTCCodecs::DecodeFrameError::DecoderCreationFailed;
    if (decoderCreationFailed && !m_softwareDecoder) {
        // The key frame stays pending, holding back later frames and flushes, until the software decoder is created.
        createSoftwareDecoder()->whenSettled(protect(WebProcess::singleton().libWebRTCCodecs())->workQueue(), [protectedThis = Ref { *this }](auto&& creationResult) {
            protectedThis->softwareDecoderCreated(WTF::move(creationResult));
        });
        return;
    }

    auto keyFrame = WTF::move(*m_pendingKeyFrame);
    m_pendingKeyFrame.reset();

    if (result) {
        if (RefPtr softwareDecoder = std::exchange(m_softwareDecoder, nullptr))
            softwareDecoder->close();
        m_softwareFallbackVideoInfo = nullptr;
        keyFrame.producer.resolve();
    } else if (decoderCreationFailed)
        decodeKeyFrameWithSoftwareDecoder(WTF::move(keyFrame));
    else
        keyFrame.producer.reject(decodeFrameErrorMessage(result.error()));

    processQueuedTasks();
}

void RemoteVideoDecoder::softwareDecoderCreated(CreatePromise::Result&& result)
{
    Locker locker { m_lock };
    ASSERT(m_pendingKeyFrame);
    auto keyFrame = WTF::move(*m_pendingKeyFrame);
    m_pendingKeyFrame.reset();

    if (!result)
        keyFrame.producer.reject(result.error());
    else if (m_isClosed) {
        // Settle as the GPU process does for the decodes pending when a decoder is released.
        (*result)->close();
        keyFrame.producer.resolve();
        while (!m_queuedTasks.isEmpty()) {
            WTF::visit([](auto&& task) {
                task.producer.resolve();
            }, m_queuedTasks.takeFirst());
        }
        return;
    } else {
        m_softwareDecoder = WTF::move(*result);
        decodeKeyFrameWithSoftwareDecoder(WTF::move(keyFrame));
    }

    processQueuedTasks();
}

void RemoteVideoDecoder::decodeKeyFrameWithSoftwareDecoder(PendingFrame&& keyFrame)
{
    RefPtr softwareDecoder = m_softwareDecoder;
    ASSERT(softwareDecoder);
    m_softwareFallbackVideoInfo = videoInfoFromKeyFrame(keyFrame.frame);
    softwareDecoder->decode(WTF::move(keyFrame.frame))->chainTo(WTF::move(keyFrame.producer));
}

void RemoteVideoDecoder::processQueuedTasks()
{
    while (!m_pendingKeyFrame && !m_queuedTasks.isEmpty()) {
        auto task = m_queuedTasks.takeFirst();
        if (auto* pendingFrame = std::get_if<PendingFrame>(&task))
            decodeInternal(WTF::move(pendingFrame->frame))->chainTo(WTF::move(pendingFrame->producer));
        else
            flushInternal()->chainTo(WTF::move(std::get<PendingFlush>(task).producer));
    }
}

RefPtr<WebCore::VideoInfo> RemoteVideoDecoder::videoInfoFromKeyFrame(const WebCore::VideoEncodedData& frame) const
{
    Ref buffer = frame.data;
    auto data = buffer->span();
    switch (m_internalDecoder.type) {
    case WebCore::VideoCodecType::VP9:
        return WebCore::createVideoInfoFromVP9Stream(data, 0, 0);
    case WebCore::VideoCodecType::AV1:
        return WebCore::createVideoInfoFromAV1Stream(data);
    case WebCore::VideoCodecType::H264:
    case WebCore::VideoCodecType::H265:
        break;
    }
    return nullptr;
}

auto RemoteVideoDecoder::createSoftwareDecoder() -> Ref<CreatePromise>
{
    CreatePromise::Producer producer;
    Ref promise = producer.promise();
    WebCore::VideoDecoder::createLocalDecoder(m_codec, m_config, [producer = WTF::move(producer)](auto&& result) mutable {
        producer.settle(WTF::move(result));
    }, [callbacks = m_callbacks](auto&& result) {
        callbacks->notifySoftwareDecodingResult(WTF::move(result));
    });
    return promise;
}

Ref<GenericPromise> RemoteVideoDecoder::flush()
{
    Locker locker { m_lock };
    if (m_pendingKeyFrame) {
        GenericPromise::AutoRejectProducer producer;
        Ref promise = producer.promise();
        m_queuedTasks.append(PendingFlush { WTF::move(producer) });
        return promise;
    }
    return flushInternal();
}

Ref<GenericPromise> RemoteVideoDecoder::flushInternal()
{
    if (RefPtr softwareDecoder = m_softwareDecoder)
        return softwareDecoder->flush();
    return protect(WebProcess::singleton().libWebRTCCodecs())->flushDecoder(m_internalDecoder);
}

void RemoteVideoDecoder::reset()
{
    close();
}

void RemoteVideoDecoder::close()
{
    m_callbacks->close();

    Locker locker { m_lock };
    m_isClosed = true;
    if (RefPtr softwareDecoder = m_softwareDecoder)
        softwareDecoder->close();
}

RemoteVideoDecoderCallbacks::RemoteVideoDecoderCallbacks(WebCore::VideoDecoder::OutputCallback&& outputCallback)
    : m_outputCallback(WTF::move(outputCallback))
{
}

void RemoteVideoDecoderCallbacks::notifySoftwareDecodingResult(std::expected<WebCore::VideoDecoder::DecodedFrame, String>&& result)
{
    if (m_isClosed)
        return;

    m_outputCallback(WTF::move(result));
}

void RemoteVideoDecoderCallbacks::notifyDecodingResult(RefPtr<WebCore::VideoFrame>&& frame, int64_t timestamp)
{
    if (m_isClosed)
        return;

    if (!frame) {
        m_outputCallback(makeUnexpected("Decoder failure"_s));
        return;
    }
    uint64_t duration = 0;
    {
        Locker locker { m_timestampToDurationLock };
        auto iterator = m_timestampToDuration.find(timestamp);
        if (iterator != m_timestampToDuration.end()) {
            duration = iterator->second;
            m_timestampToDuration.erase(iterator);
        }
    }
    m_outputCallback(WebCore::VideoDecoder::DecodedFrame { frame.releaseNonNull(), static_cast<int64_t>(timestamp), duration });
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteVideoEncoder);

RemoteVideoEncoder::RemoteVideoEncoder(LibWebRTCCodecs::Encoder& encoder, Ref<RemoteVideoEncoderCallbacks>&& callbacks)
    : m_internalEncoder(encoder)
    , m_callbacks(WTF::move(callbacks))
{
    Ref libWebRTCCodecs = WebProcess::singleton().libWebRTCCodecs();
    libWebRTCCodecs->registerEncodedVideoFrameCallback(m_internalEncoder, [callbacks = m_callbacks](auto&& encodedChunk, bool isKeyFrame, auto timestamp, auto duration, auto temporalIndex, auto rotation, bool isMirrored) {
        callbacks->notifyEncodedChunk(Vector<uint8_t> { encodedChunk }, isKeyFrame, timestamp, duration, temporalIndex, rotation, isMirrored);
    });
    libWebRTCCodecs->registerEncoderDescriptionCallback(m_internalEncoder, [callbacks = m_callbacks](WebCore::VideoEncoderActiveConfiguration&& description) {
        callbacks->notifyEncoderDescription(WTF::move(description));
    });
}

RemoteVideoEncoder::~RemoteVideoEncoder()
{
    protect(WebProcess::singleton().libWebRTCCodecs())->releaseEncoder(m_internalEncoder);
}

Ref<RemoteVideoEncoder::EncodePromise> RemoteVideoEncoder::encode(RawFrame&& rawFrame, bool shouldGenerateKeyFrame)
{
    Ref codecs = WebProcess::singleton().libWebRTCCodecs();
    return codecs->encodeFrame(m_internalEncoder, protect(rawFrame.frame), rawFrame.timestamp, rawFrame.duration, shouldGenerateKeyFrame);
}

Ref<GenericPromise> RemoteVideoEncoder::setRates(uint64_t bitRate, double frameRate)
{
    auto bitRateInKbps = bitRate / 1000;
    RefPtr promise = protect(WebProcess::singleton().libWebRTCCodecs())->setEncodeRates(m_internalEncoder, bitRateInKbps, frameRate);
    ASSERT(promise);
    return promise ? promise.releaseNonNull() : GenericPromise::createAndResolve();
}

Ref<GenericPromise> RemoteVideoEncoder::flush()
{
    return protect(WebProcess::singleton().libWebRTCCodecs())->flushEncoder(m_internalEncoder);
}

void RemoteVideoEncoder::reset()
{
    close();
}

void RemoteVideoEncoder::close()
{
    m_callbacks->close();
}

RemoteVideoEncoderCallbacks::RemoteVideoEncoderCallbacks(WebCore::VideoEncoder::DescriptionCallback&& descriptionCallback, WebCore::VideoEncoder::OutputCallback&& outputCallback)
    : m_descriptionCallback(WTF::move(descriptionCallback))
    , m_outputCallback(WTF::move(outputCallback))
{
}

void RemoteVideoEncoderCallbacks::notifyEncodedChunk(Vector<uint8_t>&& data, bool isKeyFrame, int64_t timestamp, std::optional<uint64_t> duration, std::optional<unsigned> temporalIndex, WebCore::VideoFrameRotation rotation, bool isMirrored)
{
    if (m_isClosed)
        return;

    m_outputCallback({ WTF::move(data), isKeyFrame, timestamp, duration, temporalIndex, rotation, isMirrored });
}

void RemoteVideoEncoderCallbacks::notifyEncoderDescription(WebCore::VideoEncoderActiveConfiguration&& description)
{
    if (m_isClosed)
        return;

    m_descriptionCallback(WTF::move(description));
}

}

#endif // USE(LIBWEBRTC) && PLATFORM(COCOA) && ENABLE(GPU_PROCESS)
