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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RTCRtpSFrameTransform.h"

#if ENABLE(WEB_RTC)

#include "CryptoKeyRaw.h"
#include "Logging.h"
#include "RTCRtpSFrameTransformer.h"
#include "RTCRtpTransformBackend.h"
#include "RTCRtpTransformableFrame.h"

namespace WebCore {

RTCRtpSFrameTransform::RTCRtpSFrameTransform()
    : m_transformer(RTCRtpSFrameTransformer::create())
{
}

RTCRtpSFrameTransform::~RTCRtpSFrameTransform()
{
}

void RTCRtpSFrameTransform::setEncryptionKey(CryptoKey& key, Optional<uint64_t> keyId, DOMPromiseDeferred<void>&& promise)
{
    auto algorithm = key.algorithm();
    if (!WTF::holds_alternative<CryptoKeyAlgorithm>(algorithm)) {
        promise.reject(Exception { TypeError, "Invalid key"_s });
        return;
    }

    if (WTF::get<CryptoKeyAlgorithm>(algorithm).name != "HKDF") {
        promise.reject(Exception { TypeError, "Only HKDF is supported"_s });
        return;
    }

    auto& rawKey = downcast<CryptoKeyRaw>(key);
    promise.settle(m_transformer->setEncryptionKey(rawKey.key(), keyId));
}

uint64_t RTCRtpSFrameTransform::counterForTesting() const
{
    return m_transformer->counter();
}

void RTCRtpSFrameTransform::initializeTransformer(RTCRtpTransformBackend& backend, Side side)
{
    m_isAttached = true;
    m_transformer->setIsSending(side == Side::Sender);
    m_transformer->setIsProcessingAudio(backend.mediaType() == RTCRtpTransformBackend::MediaType::Audio);

    backend.setTransformableFrameCallback([transformer = m_transformer, backend = makeRef(backend)](auto&& frame) {
        auto chunk = frame->data();
        auto result = transformer->transform(chunk.data, chunk.size);

        if (result.hasException()) {
            RELEASE_LOG_ERROR(WebRTC, "RTCRtpSFrameTransform failed transforming a frame");
            return;
        }

        frame->setData({ result.returnValue().data(), result.returnValue().size() });

        backend->processTransformedFrame(WTFMove(frame.get()));
    });
}


void RTCRtpSFrameTransform::initializeBackendForReceiver(RTCRtpTransformBackend& backend)
{
    initializeTransformer(backend, Side::Receiver);
}

void RTCRtpSFrameTransform::initializeBackendForSender(RTCRtpTransformBackend& backend)
{
    initializeTransformer(backend, Side::Sender);
}

void RTCRtpSFrameTransform::willClearBackend(RTCRtpTransformBackend& backend)
{
    backend.clearTransformableFrameCallback();
    m_isAttached = false;
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
