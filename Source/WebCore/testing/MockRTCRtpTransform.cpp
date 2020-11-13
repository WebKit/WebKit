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
#include "MockRTCRtpTransform.h"

#if ENABLE(WEB_RTC)

#include "RTCRtpTransformableFrame.h"
#include <wtf/Vector.h>

namespace WebCore {

class MockRTCRtpTransformer : public ThreadSafeRefCounted<MockRTCRtpTransformer, WTF::DestructionThread::Main> {
public:
    static Ref<MockRTCRtpTransformer> create(Ref<RTCRtpTransformBackend>&& backend)
    {
        auto transformer = adoptRef(*new MockRTCRtpTransformer(WTFMove(backend)));
        transformer->m_backend->setTransformableFrameCallback([transformer](auto&& frame) {
            transformer->transform(WTFMove(frame));
        });
        return transformer;
    }

    void clear()
    {
        m_backend->clearTransformableFrameCallback();
        m_backend = nullptr;
    }

    void transform(RTCRtpTransformableFrame&& frame)
    {
        m_isProcessing = true;

        auto data = frame.data();
        Vector<uint8_t> transformedData;
        size_t start = m_isAudio ? 0 : 100;
        for (size_t cptr = 0; cptr < start; ++cptr)
            transformedData.append(data.data[cptr]);
        for (size_t cptr = start; cptr < data.size; ++cptr)
            transformedData.append(256 - data.data[cptr]);
        frame.setData({ transformedData.data(), data.size });

        m_backend->processTransformedFrame(WTFMove(frame));
    }

    bool isProcessing() const { return m_isProcessing; }

private:
    explicit MockRTCRtpTransformer(Ref<RTCRtpTransformBackend>&& backend)
        : m_backend(WTFMove(backend))
        , m_isSender(m_backend->side() == RTCRtpTransformBackend::Side::Sender)
        , m_isAudio(m_backend->mediaType() == RTCRtpTransformBackend::MediaType::Audio)
    {
    }

    RefPtr<RTCRtpTransformBackend> m_backend;
    bool m_isSender;
    bool m_isAudio;
    bool m_isProcessing { false };
};

MockRTCRtpTransform::MockRTCRtpTransform()
{
}

MockRTCRtpTransform::~MockRTCRtpTransform()
{
}

bool MockRTCRtpTransform::isProcessing() const
{
    return m_transformer ? m_transformer->isProcessing() : false;
}

void MockRTCRtpTransform::initializeBackendForReceiver(RTCRtpTransformBackend& backend)
{
    m_transformer = MockRTCRtpTransformer::create(backend);
}

void MockRTCRtpTransform::initializeBackendForSender(RTCRtpTransformBackend& backend)
{
    m_transformer = MockRTCRtpTransformer::create(backend);
}

void MockRTCRtpTransform::willClearBackend(RTCRtpTransformBackend&)
{
    m_transformer->clear();
    m_transformer = nullptr;
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
