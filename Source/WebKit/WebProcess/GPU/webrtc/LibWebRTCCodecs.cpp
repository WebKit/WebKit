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
#include "LibWebRTCCodecs.h"

#if USE(LIBWEBRTC) && PLATFORM(COCOA) && ENABLE(GPU_PROCESS)

#include "DataReference.h"
#include "GPUProcessConnection.h"
#include "LibWebRTCCodecsProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/LibWebRTCMacros.h>
#include <WebCore/RealtimeVideoUtilities.h>
#include <WebCore/RemoteVideoSample.h>
#include <pal/cf/CoreMediaSoftLink.h>
#include <webrtc/sdk/WebKit/WebKitEncoder.h>
#include <webrtc/sdk/WebKit/WebKitUtilities.h>
#include <wtf/MainThread.h>

namespace WebKit {
using namespace WebCore;

static webrtc::WebKitVideoDecoder createVideoDecoder(const webrtc::SdpVideoFormat& format)
{
    if (format.name != "H264")
        return nullptr;

    return WebProcess::singleton().libWebRTCCodecs().createDecoder();
}

static int32_t releaseVideoDecoder(webrtc::WebKitVideoDecoder decoder)
{
    return WebProcess::singleton().libWebRTCCodecs().releaseDecoder(*static_cast<LibWebRTCCodecs::Decoder*>(decoder));
}

static int32_t decodeVideoFrame(webrtc::WebKitVideoDecoder decoder, uint32_t timeStamp, const uint8_t* data, size_t size)
{
    return WebProcess::singleton().libWebRTCCodecs().decodeFrame(*static_cast<LibWebRTCCodecs::Decoder*>(decoder), timeStamp, data, size);
}

static int32_t registerDecodeCompleteCallback(webrtc::WebKitVideoDecoder decoder, void* decodedImageCallback)
{
    WebProcess::singleton().libWebRTCCodecs().registerDecodeFrameCallback(*static_cast<LibWebRTCCodecs::Decoder*>(decoder), decodedImageCallback);
    return 0;
}

static webrtc::WebKitVideoEncoder createVideoEncoder(const webrtc::SdpVideoFormat& format)
{
    if (format.name != "H264")
        return nullptr;

    return WebProcess::singleton().libWebRTCCodecs().createEncoder(format.parameters);
}

static int32_t releaseVideoEncoder(webrtc::WebKitVideoEncoder encoder)
{
    return WebProcess::singleton().libWebRTCCodecs().releaseEncoder(*static_cast<LibWebRTCCodecs::Encoder*>(encoder));
}

static int32_t initializeVideoEncoder(webrtc::WebKitVideoEncoder encoder, const webrtc::VideoCodec& codec)
{
    return WebProcess::singleton().libWebRTCCodecs().initializeEncoder(*static_cast<LibWebRTCCodecs::Encoder*>(encoder), codec.width, codec.height, codec.startBitrate, codec.maxBitrate, codec.minBitrate, codec.maxFramerate);
}

static inline MediaSample::VideoRotation toMediaSampleVideoRotation(webrtc::VideoRotation rotation)
{
    switch (rotation) {
    case webrtc::kVideoRotation_0:
        return MediaSample::VideoRotation::None;
    case webrtc::kVideoRotation_180:
        return MediaSample::VideoRotation::UpsideDown;
    case webrtc::kVideoRotation_90:
        return MediaSample::VideoRotation::Right;
    case webrtc::kVideoRotation_270:
        return MediaSample::VideoRotation::Left;
    }
    ASSERT_NOT_REACHED();
    return MediaSample::VideoRotation::None;
}

static int32_t encodeVideoFrame(webrtc::WebKitVideoEncoder encoder, const webrtc::VideoFrame& frame, bool shouldEncodeAsKeyFrame)
{
    RetainPtr<CVPixelBufferRef> newPixelBuffer;
    auto pixelBuffer = webrtc::pixelBufferFromFrame(frame, [&newPixelBuffer](size_t width, size_t height) -> CVPixelBufferRef {
        auto pixelBufferPool = WebProcess::singleton().libWebRTCCodecs().pixelBufferPool(width, height);
        if (!pixelBufferPool)
            return nullptr;

        newPixelBuffer = WebCore::createPixelBufferFromPool(pixelBufferPool);
        return newPixelBuffer.get();
    });

    if (!pixelBuffer)
        return WEBRTC_VIDEO_CODEC_ERROR;

    auto sample = RemoteVideoSample::create(pixelBuffer, MediaTime(frame.timestamp_us(), 1000000), toMediaSampleVideoRotation(frame.rotation()));
    return WebProcess::singleton().libWebRTCCodecs().encodeFrame(*static_cast<LibWebRTCCodecs::Encoder*>(encoder), *sample, shouldEncodeAsKeyFrame);
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

void LibWebRTCCodecs::setCallbacks(bool useGPUProcess)
{
    ASSERT(isMainThread());

    if (!useGPUProcess) {
        webrtc::setVideoDecoderCallbacks(nullptr, nullptr, nullptr, nullptr);
        webrtc::setVideoEncoderCallbacks(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        return;
    }
    // Let's create WebProcess libWebRTCCodecs since it may be called from various threads.
    WebProcess::singleton().libWebRTCCodecs();
    webrtc::setVideoDecoderCallbacks(createVideoDecoder, releaseVideoDecoder, decodeVideoFrame, registerDecodeCompleteCallback);
    webrtc::setVideoEncoderCallbacks(createVideoEncoder, releaseVideoEncoder, initializeVideoEncoder, encodeVideoFrame, registerEncodeCompleteCallback, setEncodeRatesCallback);
}

LibWebRTCCodecs::Decoder* LibWebRTCCodecs::createDecoder()
{
    auto decoder = makeUnique<Decoder>();
    auto* result = decoder.get();
    decoder->identifier = RTCDecoderIdentifier::generateThreadSafe();

    callOnMainRunLoop([this, decoder = WTFMove(decoder)]() mutable {
        decoder->connection = &WebProcess::singleton().ensureGPUProcessConnection().connection();

        auto decoderIdentifier = decoder->identifier;
        decoder->connection->send(Messages::LibWebRTCCodecsProxy::CreateDecoder { decoderIdentifier }, 0);

        ASSERT(!m_decoders.contains(decoderIdentifier));
        m_decoders.add(decoderIdentifier, WTFMove(decoder));
    });
    return result;
}

int32_t LibWebRTCCodecs::releaseDecoder(Decoder& decoder)
{
    LockHolder holder(decoder.decodedImageCallbackLock);
    decoder.decodedImageCallback = nullptr;

    callOnMainRunLoop([this, decoderIdentifier = decoder.identifier] {
        ASSERT(m_decoders.contains(decoderIdentifier));

        m_decoders.remove(decoderIdentifier);
        WebProcess::singleton().ensureGPUProcessConnection().connection().send(Messages::LibWebRTCCodecsProxy::ReleaseDecoder { decoderIdentifier }, 0);
    });
    return 0;
}

int32_t LibWebRTCCodecs::decodeFrame(Decoder& decoder, uint32_t timeStamp, const uint8_t* data, size_t size)
{
    if (!decoder.connection || decoder.hasError) {
        decoder.hasError = false;
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    decoder.connection->send(Messages::LibWebRTCCodecsProxy::DecodeFrame { decoder.identifier, timeStamp, IPC::DataReference { data, size } }, 0);
    return WEBRTC_VIDEO_CODEC_OK;
}

void LibWebRTCCodecs::registerDecodeFrameCallback(Decoder& decoder, void* decodedImageCallback)
{
    LockHolder holder(decoder.decodedImageCallbackLock);
    decoder.decodedImageCallback = decodedImageCallback;
}

void LibWebRTCCodecs::failedDecoding(RTCDecoderIdentifier decoderIdentifier)
{
    ASSERT(isMainThread());

    if (auto* decoder = m_decoders.get(decoderIdentifier))
        decoder->hasError = true;
}

void LibWebRTCCodecs::completedDecoding(RTCDecoderIdentifier decoderIdentifier, uint32_t timeStamp, WebCore::RemoteVideoSample&& remoteSample)
{
    ASSERT(isMainThread());

    // FIXME: Do error logging.
    auto* decoder = m_decoders.get(decoderIdentifier);
    if (!decoder)
        return;

    auto locker = tryHoldLock(decoder->decodedImageCallbackLock);
    if (!locker)
        return;

    if (!decoder->decodedImageCallback)
        return;

    if (!m_imageTransferSession || m_imageTransferSession->pixelFormat() != remoteSample.videoFormat())
        m_imageTransferSession = WebCore::ImageTransferSessionVT::create(remoteSample.videoFormat());

    if (!m_imageTransferSession) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto pixelBuffer = m_imageTransferSession->createPixelBuffer(remoteSample.surface(), remoteSample.size());
    if (!pixelBuffer) {
        ASSERT_NOT_REACHED();
        return;
    }

    webrtc::RemoteVideoDecoder::decodeComplete(decoder->decodedImageCallback, timeStamp, pixelBuffer.get(), remoteSample.time().toDouble());
}

LibWebRTCCodecs::Encoder* LibWebRTCCodecs::createEncoder(const std::map<std::string, std::string>& formatParameters)
{
    auto encoder = makeUnique<Encoder>();
    auto* result = encoder.get();
    encoder->identifier = RTCEncoderIdentifier::generateThreadSafe();

    Vector<std::pair<String, String>> parameters;
    for (auto& keyValue : formatParameters)
        parameters.append(std::make_pair(String::fromUTF8(keyValue.first.data(), keyValue.first.length()), String::fromUTF8(keyValue.second.data(), keyValue.second.length())));

    callOnMainRunLoop([this, encoder = WTFMove(encoder), parameters = WTFMove(parameters)]() mutable {
        auto encoderIdentifier = encoder->identifier;
        ASSERT(!m_encoders.contains(encoderIdentifier));

        WebProcess::singleton().ensureGPUProcessConnection().connection().send(Messages::LibWebRTCCodecsProxy::CreateEncoder { encoderIdentifier, parameters }, 0);
        m_encoders.add(encoderIdentifier, WTFMove(encoder));
    });
    return result;
}

int32_t LibWebRTCCodecs::releaseEncoder(Encoder& encoder)
{
    LockHolder holder(encoder.encodedImageCallbackLock);
    encoder.encodedImageCallback = nullptr;

    callOnMainRunLoop([this, encoderIdentifier = encoder.identifier] {
        ASSERT(m_encoders.contains(encoderIdentifier));

        m_encoders.remove(encoderIdentifier);
        WebProcess::singleton().ensureGPUProcessConnection().connection().send(Messages::LibWebRTCCodecsProxy::ReleaseEncoder { encoderIdentifier }, 0);
    });
    return 0;
}

int32_t LibWebRTCCodecs::initializeEncoder(Encoder& encoder, uint16_t width, uint16_t height, unsigned startBitRate, unsigned maxBitRate, unsigned minBitRate, uint32_t maxFrameRate)
{
    callOnMainRunLoop([this, encoderIdentifier = encoder.identifier, width, height, startBitRate, maxBitRate, minBitRate, maxFrameRate] {
        if (auto* encoder = m_encoders.get(encoderIdentifier)) {
            auto& connection = WebProcess::singleton().ensureGPUProcessConnection().connection();
            connection.send(Messages::LibWebRTCCodecsProxy::InitializeEncoder { encoderIdentifier, width, height, startBitRate, maxBitRate, minBitRate, maxFrameRate }, 0);
            // We set encoder->connection here so that InitializeEncoder is sent before any EncodeFrame message.
            encoder->connection = &connection;
        }
    });
    return 0;
}

int32_t LibWebRTCCodecs::encodeFrame(Encoder& encoder, const WebCore::RemoteVideoSample& frame, bool shouldEncodeAsKeyFrame)
{
    if (!encoder.connection)
        return WEBRTC_VIDEO_CODEC_ERROR;

    encoder.connection->send(Messages::LibWebRTCCodecsProxy::EncodeFrame { encoder.identifier, frame, shouldEncodeAsKeyFrame }, 0);
    return WEBRTC_VIDEO_CODEC_OK;
}

void LibWebRTCCodecs::registerEncodeFrameCallback(Encoder& encoder, void* encodedImageCallback)
{
    LockHolder holder(encoder.encodedImageCallbackLock);
    encoder.encodedImageCallback = encodedImageCallback;
}

void LibWebRTCCodecs::setEncodeRates(Encoder& encoder, uint32_t bitRate, uint32_t frameRate)
{
    if (!encoder.connection) {
        callOnMainRunLoop([this, encoderIdentifier = encoder.identifier, bitRate, frameRate] {
            UNUSED_PARAM(this);
            ASSERT(m_encoders.contains(encoderIdentifier));
            ASSERT(m_encoders.get(encoderIdentifier)->connection);
            WebProcess::singleton().ensureGPUProcessConnection().connection().send(Messages::LibWebRTCCodecsProxy::SetEncodeRates { encoderIdentifier, bitRate, frameRate }, 0);
        });
        return;
    }
    encoder.connection->send(Messages::LibWebRTCCodecsProxy::SetEncodeRates { encoder.identifier, bitRate, frameRate }, 0);
}

void LibWebRTCCodecs::completedEncoding(RTCEncoderIdentifier identifier, IPC::DataReference&& data, const webrtc::WebKitEncodedFrameInfo& info, webrtc::WebKitRTPFragmentationHeader&& fragmentationHeader)
{
    ASSERT(isMainThread());

    // FIXME: Do error logging.
    auto* encoder = m_encoders.get(identifier);
    if (!encoder)
        return;

    auto locker = tryHoldLock(encoder->encodedImageCallbackLock);
    if (!locker)
        return;

    if (!encoder->encodedImageCallback)
        return;

    webrtc::RemoteVideoEncoder::encodeComplete(encoder->encodedImageCallback, const_cast<uint8_t*>(data.data()), data.size(), info, fragmentationHeader.value());
}

CVPixelBufferPoolRef LibWebRTCCodecs::pixelBufferPool(size_t width, size_t height)
{
    if (!m_pixelBufferPool || m_pixelBufferPoolWidth != width || m_pixelBufferPoolHeight != height) {
        m_pixelBufferPool = createPixelBufferPool(width, height);
        m_pixelBufferPoolWidth = width;
        m_pixelBufferPoolHeight = height;
    }
    return m_pixelBufferPool.get();
}

}

#endif
