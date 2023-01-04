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

namespace WebKit {

class RemoteVideoDecoderCallbacks : public ThreadSafeRefCounted<RemoteVideoDecoderCallbacks> {
public:
    static Ref<RemoteVideoDecoderCallbacks> create(VideoDecoder::OutputCallback&& outputCallback, VideoDecoder::PostTaskCallback&& postTaskCallback) { return adoptRef(*new RemoteVideoDecoderCallbacks(WTFMove(outputCallback), WTFMove(postTaskCallback))); }
    ~RemoteVideoDecoderCallbacks() = default;

    void postTask(Function<void()>&& task) { m_postTaskCallback(WTFMove(task)); }
    void notifyDecodingResult(RefPtr<WebCore::VideoFrame>&&, int64_t timestamp);

    // Must be called on the VideoDecoder thread, or within postTaskCallback.
    void close() { m_isClosed = true; }
    void addDuration(int64_t timestamp, uint64_t duration) { m_timestampToDuration.add(timestamp + 1, duration); }

private:
    RemoteVideoDecoderCallbacks(VideoDecoder::OutputCallback&&, VideoDecoder::PostTaskCallback&&);

    VideoDecoder::OutputCallback m_outputCallback;
    VideoDecoder::PostTaskCallback m_postTaskCallback;
    bool m_isClosed { false };
    HashMap<int64_t, uint64_t> m_timestampToDuration;
};

class RemoteVideoDecoder : public WebCore::VideoDecoder {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RemoteVideoDecoder(LibWebRTCCodecs::Decoder&, Ref<RemoteVideoDecoderCallbacks>&&, uint16_t width, uint16_t height);
    ~RemoteVideoDecoder();

private:
    void decode(EncodedFrame&&, DecodeCallback&&) final;
    void flush(Function<void()>&&) final;
    void reset() final;
    void close() final;

    LibWebRTCCodecs::Decoder& m_internalDecoder;
    Ref<RemoteVideoDecoderCallbacks> m_callbacks;

    uint16_t m_width { 0 };
    uint16_t m_height { 0 };
};

class RemoteVideoEncoderCallbacks : public ThreadSafeRefCounted<RemoteVideoEncoderCallbacks> {
public:
    static Ref<RemoteVideoEncoderCallbacks> create(VideoEncoder::DescriptionCallback&& descriptionCallback, VideoEncoder::OutputCallback&& outputCallback, VideoEncoder::PostTaskCallback&& postTaskCallback) { return adoptRef(*new RemoteVideoEncoderCallbacks(WTFMove(descriptionCallback), WTFMove(outputCallback), WTFMove(postTaskCallback))); }
    ~RemoteVideoEncoderCallbacks() = default;

    void postTask(Function<void()>&& task) { m_postTaskCallback(WTFMove(task)); }
    void notifyEncodedChunk(Vector<uint8_t>&&, bool isKeyFrame, int64_t timestamp);
    void notifyEncoderDescription(Vector<uint8_t>&&);

    // Must be called on the VideoDecoder thread, or within postTaskCallback.
    void close() { m_isClosed = true; }
    void addDuration(int64_t timestamp, uint64_t duration) { m_timestampToDuration.add(timestamp + 1, duration); }

private:
    RemoteVideoEncoderCallbacks(VideoEncoder::DescriptionCallback&&, VideoEncoder::OutputCallback&&, VideoEncoder::PostTaskCallback&&);

    VideoEncoder::DescriptionCallback m_descriptionCallback;
    VideoEncoder::OutputCallback m_outputCallback;
    VideoEncoder::PostTaskCallback m_postTaskCallback;
    bool m_isClosed { false };
    HashMap<int64_t, uint64_t> m_timestampToDuration;
};

class RemoteVideoEncoder : public WebCore::VideoEncoder {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RemoteVideoEncoder(LibWebRTCCodecs::Encoder&, const VideoEncoder::Config&, Ref<RemoteVideoEncoderCallbacks>&&);
    ~RemoteVideoEncoder();

private:
    void encode(RawFrame&&, bool shouldGenerateKeyFrame, EncodeCallback&&) final;
    void flush(Function<void()>&&) final;
    void reset() final;
    void close() final;

    LibWebRTCCodecs::Encoder& m_internalEncoder;
    Ref<RemoteVideoEncoderCallbacks> m_callbacks;
};

RemoteVideoCodecFactory::RemoteVideoCodecFactory(WebProcess& process)
{
    ASSERT(isMainRunLoop());
    // We make sure to create libWebRTCCodecs() as it might be called from multiple threads.
    process.libWebRTCCodecs();
    VideoDecoder::setCreatorCallback(RemoteVideoCodecFactory::createDecoder);
    VideoEncoder::setCreatorCallback(RemoteVideoCodecFactory::createEncoder);
}

RemoteVideoCodecFactory::~RemoteVideoCodecFactory()
{
}

void RemoteVideoCodecFactory::createDecoder(const String& codec, const VideoDecoder::Config& config, VideoDecoder::CreateCallback&& createCallback, VideoDecoder::OutputCallback&& outputCallback, VideoDecoder::PostTaskCallback&& postTaskCallback)
{
    LibWebRTCCodecs::initializeIfNeeded();

    auto type = WebProcess::singleton().libWebRTCCodecs().videoCodecTypeFromWebCodec(codec);
    if (!type) {
        VideoDecoder::createLocalDecoder(codec, config, WTFMove(createCallback), WTFMove(outputCallback), WTFMove(postTaskCallback));
        return;
    }
    WebProcess::singleton().libWebRTCCodecs().createDecoderAndWaitUntilReady(*type, [width = config.width, height = config.height, description = Vector<uint8_t> { config.description }, createCallback = WTFMove(createCallback), outputCallback = WTFMove(outputCallback), postTaskCallback = WTFMove(postTaskCallback)](auto& internalDecoder) mutable {
        if (description.size())
            WebProcess::singleton().libWebRTCCodecs().setDecoderFormatDescription(internalDecoder, description.data(), description.size(), width, height);

        auto callbacks = RemoteVideoDecoderCallbacks::create(WTFMove(outputCallback), WTFMove(postTaskCallback));
        UniqueRef<VideoDecoder> decoder = makeUniqueRef<RemoteVideoDecoder>(internalDecoder, callbacks.copyRef(), width, height);
        callbacks->postTask([createCallback = WTFMove(createCallback), decoder = WTFMove(decoder)]() mutable {
            createCallback(WTFMove(decoder));
        });
    });
}

void RemoteVideoCodecFactory::createEncoder(const String& codec, const WebCore::VideoEncoder::Config& config, WebCore::VideoEncoder::CreateCallback&& createCallback, WebCore::VideoEncoder::DescriptionCallback&& descriptionCallback, WebCore::VideoEncoder::OutputCallback&& outputCallback, WebCore::VideoEncoder::PostTaskCallback&& postTaskCallback)
{
    LibWebRTCCodecs::initializeIfNeeded();

    auto type = WebProcess::singleton().libWebRTCCodecs().videoCodecTypeFromWebCodec(codec);
    if (!type) {
        VideoEncoder::createLocalEncoder(codec, config, WTFMove(createCallback), WTFMove(descriptionCallback), WTFMove(outputCallback), WTFMove(postTaskCallback));
        return;
    }
    WebProcess::singleton().libWebRTCCodecs().createEncoderAndWaitUntilReady(*type, { }, config.isRealtime, config.useAnnexB, [config, createCallback = WTFMove(createCallback), descriptionCallback = WTFMove(descriptionCallback), outputCallback = WTFMove(outputCallback), postTaskCallback = WTFMove(postTaskCallback)](auto& internalEncoder) mutable {
        auto callbacks = RemoteVideoEncoderCallbacks::create(WTFMove(descriptionCallback), WTFMove(outputCallback), WTFMove(postTaskCallback));
        UniqueRef<VideoEncoder> encoder = makeUniqueRef<RemoteVideoEncoder>(internalEncoder, config, callbacks.copyRef());
        callbacks->postTask([createCallback = WTFMove(createCallback), encoder = WTFMove(encoder)]() mutable {
            createCallback(WTFMove(encoder));
        });
    });
}

RemoteVideoDecoder::RemoteVideoDecoder(LibWebRTCCodecs::Decoder& decoder, Ref<RemoteVideoDecoderCallbacks>&& callbacks, uint16_t width, uint16_t height)
    : m_internalDecoder(decoder)
    , m_callbacks(WTFMove(callbacks))
    , m_width(width)
    , m_height(height)
{
    WebProcess::singleton().libWebRTCCodecs().registerDecodedVideoFrameCallback(m_internalDecoder, [callbacks = m_callbacks](auto&& videoFrame, auto timestamp) {
        callbacks->notifyDecodingResult(WTFMove(videoFrame), timestamp);
    });
}

RemoteVideoDecoder::~RemoteVideoDecoder()
{
    WebProcess::singleton().libWebRTCCodecs().releaseDecoder(m_internalDecoder);
}

void RemoteVideoDecoder::decode(EncodedFrame&& frame, DecodeCallback&& callback)
{
    if (frame.duration)
        m_callbacks->addDuration(frame.timestamp, *frame.duration);
    WebProcess::singleton().libWebRTCCodecs().decodeFrame(m_internalDecoder, frame.timestamp, frame.data.data(), frame.data.size(), m_width, m_height);
    callback({ });
}

void RemoteVideoDecoder::flush(Function<void()>&& callback)
{
    WebProcess::singleton().libWebRTCCodecs().flushDecoder(m_internalDecoder, [callback = WTFMove(callback), callbacks = m_callbacks]() mutable {
        callbacks->postTask([callback = WTFMove(callback)]() mutable {
            callback();
        });
    });
}

void RemoteVideoDecoder::reset()
{
    close();
}

void RemoteVideoDecoder::close()
{
    m_callbacks->close();
}

RemoteVideoDecoderCallbacks::RemoteVideoDecoderCallbacks(VideoDecoder::OutputCallback&& outputCallback, VideoDecoder::PostTaskCallback&& postTaskCallback)
    : m_outputCallback(WTFMove(outputCallback))
    , m_postTaskCallback(WTFMove(postTaskCallback))
{
}

void RemoteVideoDecoderCallbacks::notifyDecodingResult(RefPtr<WebCore::VideoFrame>&& frame, int64_t timestamp)
{
    m_postTaskCallback([protectedThis = Ref { *this }, frame = WTFMove(frame), timestamp]() mutable {
        if (protectedThis->m_isClosed)
            return;

        if (!frame) {
            protectedThis->m_outputCallback(makeUnexpected("Decoder failure"_s));
            return;
        }
        auto duration = protectedThis->m_timestampToDuration.take(timestamp + 1);
        protectedThis->m_outputCallback(VideoDecoder::DecodedFrame { frame.releaseNonNull(), static_cast<int64_t>(timestamp), duration });
    });
}

RemoteVideoEncoder::RemoteVideoEncoder(LibWebRTCCodecs::Encoder& encoder, const VideoEncoder::Config& config, Ref<RemoteVideoEncoderCallbacks>&& callbacks)
    : m_internalEncoder(encoder)
    , m_callbacks(WTFMove(callbacks))
{
    WebProcess::singleton().libWebRTCCodecs().initializeEncoder(m_internalEncoder, config.width, config.height, 0, 0, 0, 0);
    WebProcess::singleton().libWebRTCCodecs().setEncodeRates(m_internalEncoder, config.bitRate, config.frameRate);

    WebProcess::singleton().libWebRTCCodecs().registerEncodedVideoFrameCallback(m_internalEncoder, [callbacks = m_callbacks](auto&& encodedChunk, bool isKeyFrame, auto timestamp) {
        callbacks->notifyEncodedChunk(Vector<uint8_t> { encodedChunk }, isKeyFrame, timestamp);
    });
    WebProcess::singleton().libWebRTCCodecs().registerEncoderDescriptionCallback(m_internalEncoder, [callbacks = m_callbacks](auto&& description) {
        callbacks->notifyEncoderDescription(Vector<uint8_t> { description });
    });
}

RemoteVideoEncoder::~RemoteVideoEncoder()
{
    WebProcess::singleton().libWebRTCCodecs().releaseEncoder(m_internalEncoder);
}

void RemoteVideoEncoder::encode(RawFrame&& rawFrame, bool shouldGenerateKeyFrame, EncodeCallback&& callback)
{
    if (rawFrame.duration)
        m_callbacks->addDuration(rawFrame.timestamp, *rawFrame.duration);

    WebProcess::singleton().libWebRTCCodecs().encodeFrame(m_internalEncoder, rawFrame.frame.get(), rawFrame.timestamp, shouldGenerateKeyFrame);
    callback({ });
}

void RemoteVideoEncoder::flush(Function<void()>&& callback)
{
    WebProcess::singleton().libWebRTCCodecs().flushEncoder(m_internalEncoder, [callback = WTFMove(callback), callbacks = m_callbacks]() mutable {
        callbacks->postTask([callback = WTFMove(callback)]() mutable {
            callback();
        });
    });
}

void RemoteVideoEncoder::reset()
{
    close();
}

void RemoteVideoEncoder::close()
{
    m_callbacks->close();
}

RemoteVideoEncoderCallbacks::RemoteVideoEncoderCallbacks(VideoEncoder::DescriptionCallback&& descriptionCallback, VideoEncoder::OutputCallback&& outputCallback, VideoEncoder::PostTaskCallback&& postTaskCallback)
    : m_descriptionCallback(WTFMove(descriptionCallback))
    , m_outputCallback(WTFMove(outputCallback))
    , m_postTaskCallback(WTFMove(postTaskCallback))
{
}

void RemoteVideoEncoderCallbacks::notifyEncodedChunk(Vector<uint8_t>&& data, bool isKeyFrame, int64_t timestamp)
{
    m_postTaskCallback([protectedThis = Ref { *this }, data = WTFMove(data), isKeyFrame, timestamp]() mutable {
        if (protectedThis->m_isClosed)
            return;

        // FIXME: Remove from the map.
        auto duration = protectedThis->m_timestampToDuration.get(timestamp + 1);
        protectedThis->m_outputCallback({ WTFMove(data), isKeyFrame, static_cast<int64_t>(timestamp), duration });
    });
}

void RemoteVideoEncoderCallbacks::notifyEncoderDescription(Vector<uint8_t>&& description)
{
    m_postTaskCallback([protectedThis = Ref { *this }, description = WTFMove(description)]() mutable {
        if (protectedThis->m_isClosed)
            return;

        protectedThis->m_descriptionCallback(VideoEncoder::ActiveConfiguration { { }, { }, { }, { }, { }, WTFMove(description), { } });
    });
}

}

#endif // USE(LIBWEBRTC) && PLATFORM(COCOA) && ENABLE(GPU_PROCESS)
