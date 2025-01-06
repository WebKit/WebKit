/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include <wtf/StdUnorderedMap.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

class RemoteVideoDecoderCallbacks : public ThreadSafeRefCounted<RemoteVideoDecoderCallbacks> {
public:
    static Ref<RemoteVideoDecoderCallbacks> create(WebCore::VideoDecoder::OutputCallback&& outputCallback) { return adoptRef(*new RemoteVideoDecoderCallbacks(WTFMove(outputCallback))); }
    ~RemoteVideoDecoderCallbacks() = default;

    void notifyDecodingResult(RefPtr<WebCore::VideoFrame>&&, int64_t timestamp);

    void close() { m_isClosed = true; }
    void addDuration(int64_t timestamp, uint64_t duration) { m_timestampToDuration.insert_or_assign(timestamp, duration); }

private:
    explicit RemoteVideoDecoderCallbacks(WebCore::VideoDecoder::OutputCallback&&);

    WebCore::VideoDecoder::OutputCallback m_outputCallback;
    bool m_isClosed { false };
    StdUnorderedMap<int64_t, uint64_t> m_timestampToDuration;
};

class RemoteVideoDecoder : public WebCore::VideoDecoder {
    WTF_MAKE_TZONE_ALLOCATED(RemoteVideoDecoder);
public:
    static Ref<WebCore::VideoDecoder> create(LibWebRTCCodecs::Decoder& decoder, Ref<RemoteVideoDecoderCallbacks>&& callbacks, uint16_t width, uint16_t height) { return adoptRef(*new RemoteVideoDecoder(decoder, WTFMove(callbacks), width, height)); }
    ~RemoteVideoDecoder();

private:
    RemoteVideoDecoder(LibWebRTCCodecs::Decoder&, Ref<RemoteVideoDecoderCallbacks>&&, uint16_t width, uint16_t height);

    Ref<DecodePromise> decode(EncodedFrame&&) final;
    Ref<GenericPromise> flush() final;
    void reset() final;
    void close() final;

    LibWebRTCCodecs::Decoder& m_internalDecoder;
    Ref<RemoteVideoDecoderCallbacks> m_callbacks;

    uint16_t m_width { 0 };
    uint16_t m_height { 0 };
};

class RemoteVideoEncoderCallbacks : public ThreadSafeRefCounted<RemoteVideoEncoderCallbacks> {
public:
    static Ref<RemoteVideoEncoderCallbacks> create(WebCore::VideoEncoder::DescriptionCallback&& descriptionCallback, WebCore::VideoEncoder::OutputCallback&& outputCallback) { return adoptRef(*new RemoteVideoEncoderCallbacks(WTFMove(descriptionCallback), WTFMove(outputCallback))); }
    ~RemoteVideoEncoderCallbacks() = default;

    void notifyEncodedChunk(Vector<uint8_t>&&, bool isKeyFrame, int64_t timestamp, std::optional<uint64_t> duration, std::optional<unsigned> temporalIndex);
    void notifyEncoderDescription(WebCore::VideoEncoderActiveConfiguration&&);

    void close() { m_isClosed = true; }

private:
    RemoteVideoEncoderCallbacks(WebCore::VideoEncoder::DescriptionCallback&&, WebCore::VideoEncoder::OutputCallback&&);

    WebCore::VideoEncoder::DescriptionCallback m_descriptionCallback;
    WebCore::VideoEncoder::OutputCallback m_outputCallback;
    bool m_isClosed { false };
};

class RemoteVideoEncoder : public WebCore::VideoEncoder {
    WTF_MAKE_TZONE_ALLOCATED(RemoteVideoEncoder);
public:
    static Ref<WebCore::VideoEncoder> create(LibWebRTCCodecs::Encoder& encoder, Ref<RemoteVideoEncoderCallbacks>&& callbacks) { return adoptRef(*new RemoteVideoEncoder(encoder, WTFMove(callbacks))); }
    ~RemoteVideoEncoder();

private:
    RemoteVideoEncoder(LibWebRTCCodecs::Encoder&, Ref<RemoteVideoEncoderCallbacks>&&);

    Ref<EncodePromise> encode(RawFrame&&, bool shouldGenerateKeyFrame) final;
    Ref<GenericPromise> flush() final;
    void reset() final;
    void close() final;
    Ref<GenericPromise> setRates(uint64_t bitRate, double frameRate) final;

    LibWebRTCCodecs::Encoder& m_internalEncoder;
    Ref<RemoteVideoEncoderCallbacks> m_callbacks;
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

RemoteVideoCodecFactory::~RemoteVideoCodecFactory()
{
}

static bool shouldUseLocalDecoder(std::optional<WebCore::VideoCodecType> type, const WebCore::VideoDecoder::Config& config)
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

    auto type = WebProcess::singleton().libWebRTCCodecs().videoCodecTypeFromWebCodec(codec);
    if (shouldUseLocalDecoder(type, config)) {
        WebCore::VideoDecoder::createLocalDecoder(codec, config, WTFMove(createCallback), WTFMove(outputCallback));
        return;
    }
    WebProcess::singleton().libWebRTCCodecs().createDecoderAndWaitUntilReady(*type, codec, [width = config.width, height = config.height, description = Vector<uint8_t> { config.description }, createCallback = WTFMove(createCallback), outputCallback = WTFMove(outputCallback)](auto* internalDecoder) mutable {
        if (!internalDecoder) {
            createCallback(makeUnexpected("Decoder creation failed"_s));
            return;
        }
        if (description.size())
            WebProcess::singleton().libWebRTCCodecs().setDecoderFormatDescription(*internalDecoder, description.span(), width, height);

        auto callbacks = RemoteVideoDecoderCallbacks::create(WTFMove(outputCallback));
        createCallback(RemoteVideoDecoder::create(*internalDecoder, callbacks.copyRef(), width, height));
    });
}

void RemoteVideoCodecFactory::createEncoder(const String& codec, const WebCore::VideoEncoder::Config& config, WebCore::VideoEncoder::CreateCallback&& createCallback, WebCore::VideoEncoder::DescriptionCallback&& descriptionCallback, WebCore::VideoEncoder::OutputCallback&& outputCallback)
{
    LibWebRTCCodecs::initializeIfNeeded();

    auto type = WebProcess::singleton().libWebRTCCodecs().videoEncoderTypeFromWebCodec(codec);
    if (!type) {
        WebCore::VideoEncoder::createLocalEncoder(codec, config, WTFMove(createCallback), WTFMove(descriptionCallback), WTFMove(outputCallback));
        return;
    }

    std::map<std::string, std::string> parameters;
    if (type == WebCore::VideoCodecType::H264) {
        if (auto position = codec.find('.');position != notFound && position != codec.length()) {
            auto profileLevelId = spanReinterpretCast<const char>(codec.span8().subspan(position + 1));
            parameters["profile-level-id"] = std::string(profileLevelId.data(), profileLevelId.size());
        }
    }

    WebProcess::singleton().libWebRTCCodecs().createEncoderAndWaitUntilInitialized(*type, codec, parameters, config, [createCallback = WTFMove(createCallback), descriptionCallback = WTFMove(descriptionCallback), outputCallback = WTFMove(outputCallback)](auto* internalEncoder) mutable {
        if (!internalEncoder) {
            createCallback(makeUnexpected("Encoder creation failed"_s));
            return;
        }
        Ref callbacks = RemoteVideoEncoderCallbacks::create(WTFMove(descriptionCallback), WTFMove(outputCallback));
        createCallback(RemoteVideoEncoder::create(*internalEncoder, WTFMove(callbacks)));
    });
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteVideoDecoder);

RemoteVideoDecoder::RemoteVideoDecoder(LibWebRTCCodecs::Decoder& decoder, Ref<RemoteVideoDecoderCallbacks>&& callbacks, uint16_t width, uint16_t height)
    : m_internalDecoder(decoder)
    , m_callbacks(WTFMove(callbacks))
    , m_width(width)
    , m_height(height)
{
    WebProcess::singleton().libWebRTCCodecs().registerDecodedVideoFrameCallback(m_internalDecoder, [callbacks = m_callbacks](RefPtr<WebCore::VideoFrame>&& videoFrame, auto timestamp) {
        callbacks->notifyDecodingResult(WTFMove(videoFrame), timestamp);
    });
}

RemoteVideoDecoder::~RemoteVideoDecoder()
{
    WebProcess::singleton().libWebRTCCodecs().releaseDecoder(m_internalDecoder);
}

Ref<RemoteVideoDecoder::DecodePromise> RemoteVideoDecoder::decode(EncodedFrame&& frame)
{
    if (frame.duration)
        m_callbacks->addDuration(frame.timestamp, *frame.duration);

    Ref codecs = WebProcess::singleton().libWebRTCCodecs();
    return codecs->decodeFrame(m_internalDecoder, frame.timestamp, frame.data, m_width, m_height);
}

Ref<GenericPromise> RemoteVideoDecoder::flush()
{
    return WebProcess::singleton().libWebRTCCodecs().flushDecoder(m_internalDecoder);
}

void RemoteVideoDecoder::reset()
{
    close();
}

void RemoteVideoDecoder::close()
{
    m_callbacks->close();
}

RemoteVideoDecoderCallbacks::RemoteVideoDecoderCallbacks(WebCore::VideoDecoder::OutputCallback&& outputCallback)
    : m_outputCallback(WTFMove(outputCallback))
{
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
    auto iterator = m_timestampToDuration.find(timestamp);
    if (iterator != m_timestampToDuration.end()) {
        duration = iterator->second;
        m_timestampToDuration.erase(iterator);
    }
    m_outputCallback(WebCore::VideoDecoder::DecodedFrame { frame.releaseNonNull(), static_cast<int64_t>(timestamp), duration });
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteVideoEncoder);

RemoteVideoEncoder::RemoteVideoEncoder(LibWebRTCCodecs::Encoder& encoder, Ref<RemoteVideoEncoderCallbacks>&& callbacks)
    : m_internalEncoder(encoder)
    , m_callbacks(WTFMove(callbacks))
{
    WebProcess::singleton().libWebRTCCodecs().registerEncodedVideoFrameCallback(m_internalEncoder, [callbacks = m_callbacks](auto&& encodedChunk, bool isKeyFrame, auto timestamp, auto duration, auto temporalIndex) {
        callbacks->notifyEncodedChunk(Vector<uint8_t> { encodedChunk }, isKeyFrame, timestamp, duration, temporalIndex);
    });
    WebProcess::singleton().libWebRTCCodecs().registerEncoderDescriptionCallback(m_internalEncoder, [callbacks = m_callbacks](WebCore::VideoEncoderActiveConfiguration&& description) {
        callbacks->notifyEncoderDescription(WTFMove(description));
    });
}

RemoteVideoEncoder::~RemoteVideoEncoder()
{
    WebProcess::singleton().libWebRTCCodecs().releaseEncoder(m_internalEncoder);
}

Ref<RemoteVideoEncoder::EncodePromise> RemoteVideoEncoder::encode(RawFrame&& rawFrame, bool shouldGenerateKeyFrame)
{
    Ref codecs = WebProcess::singleton().libWebRTCCodecs();
    return codecs->encodeFrame(m_internalEncoder, rawFrame.frame.get(), rawFrame.timestamp, rawFrame.duration, shouldGenerateKeyFrame);
}

Ref<GenericPromise> RemoteVideoEncoder::setRates(uint64_t bitRate, double frameRate)
{
    auto bitRateInKbps = bitRate / 1000;
    return WebProcess::singleton().libWebRTCCodecs().setEncodeRates(m_internalEncoder, bitRateInKbps, frameRate);
}

Ref<GenericPromise> RemoteVideoEncoder::flush()
{
    return WebProcess::singleton().libWebRTCCodecs().flushEncoder(m_internalEncoder);
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
    : m_descriptionCallback(WTFMove(descriptionCallback))
    , m_outputCallback(WTFMove(outputCallback))
{
}

void RemoteVideoEncoderCallbacks::notifyEncodedChunk(Vector<uint8_t>&& data, bool isKeyFrame, int64_t timestamp, std::optional<uint64_t> duration, std::optional<unsigned> temporalIndex)
{
    if (m_isClosed)
        return;

    m_outputCallback({ WTFMove(data), isKeyFrame, timestamp, duration, temporalIndex });
}

void RemoteVideoEncoderCallbacks::notifyEncoderDescription(WebCore::VideoEncoderActiveConfiguration&& description)
{
    if (m_isClosed)
        return;

    m_descriptionCallback(WTFMove(description));
}

}

#endif // USE(LIBWEBRTC) && PLATFORM(COCOA) && ENABLE(GPU_PROCESS)
