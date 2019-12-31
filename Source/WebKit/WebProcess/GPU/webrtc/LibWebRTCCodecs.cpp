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

#include "GPUProcessConnection.h"
#include "LibWebRTCCodecsProxyMessages.h"
#include "SharedBufferDataReference.h"
#include "WebProcess.h"
#include <WebCore/LibWebRTCMacros.h>
#include <WebCore/RemoteVideoSample.h>
#include <pal/cf/CoreMediaSoftLink.h>
#include <webrtc/sdk/WebKit/WebKitUtilities.h>
#include <wtf/MainThread.h>

namespace WebKit {

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

void LibWebRTCCodecs::setVideoDecoderCallbacks(bool useGPUProcess)
{
    ASSERT(isMainThread());

    if (!useGPUProcess) {
        webrtc::setVideoDecoderCallbacks(nullptr, nullptr, nullptr, nullptr);
        return;
    }
    // Let's create WebProcess libWebRTCCodecs since it may be called from various threads.
    WebProcess::singleton().libWebRTCCodecs();
    webrtc::setVideoDecoderCallbacks(createVideoDecoder, releaseVideoDecoder, decodeVideoFrame, registerDecodeCompleteCallback);
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

        ASSERT(!m_decodeCallbacks.contains(decoderIdentifier));
        m_decodeCallbacks.add(decoderIdentifier, WTFMove(decoder));
    });
    return result;
}

int32_t LibWebRTCCodecs::releaseDecoder(Decoder& decoder)
{
    LockHolder holder(decoder.decodedImageCallbackLock);
    decoder.decodedImageCallback = nullptr;

    callOnMainRunLoop([this, decoderIdentifier = decoder.identifier] {
        ASSERT(m_decodeCallbacks.contains(decoderIdentifier));

        m_decodeCallbacks.remove(decoderIdentifier);
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

    if (auto* decoder = m_decodeCallbacks.get(decoderIdentifier))
        decoder->hasError = true;
}

void LibWebRTCCodecs::completedDecoding(RTCDecoderIdentifier decoderIdentifier, uint32_t timeStamp, WebCore::RemoteVideoSample&& remoteSample)
{
    ASSERT(isMainThread());

    // FIXME: Do error logging.
    auto* decoder = m_decodeCallbacks.get(decoderIdentifier);
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

}

#endif
