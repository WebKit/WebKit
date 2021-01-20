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

#pragma once

#if ENABLE(WEB_RTC)

#include "RTCRtpTransformBackend.h"
#include <wtf/Ref.h>

namespace WebCore {

class RTCRtpTransformBackend;

class RTCRtpScriptTransformerContext
    : public RefCounted<RTCRtpScriptTransformerContext> {
public:
    static Ref<RTCRtpScriptTransformerContext> create(Ref<RTCRtpTransformBackend>&&);
    ~RTCRtpScriptTransformerContext() = default;

    using Side = RTCRtpTransformBackend::Side;
    Side side() const { return m_backend->side(); }

    using MediaType = RTCRtpTransformBackend::MediaType;
    MediaType mediaType() const { return m_backend->mediaType(); }

    ExceptionOr<void> requestKeyFrame();

private:
    explicit RTCRtpScriptTransformerContext(Ref<RTCRtpTransformBackend>&&);

    Ref<RTCRtpTransformBackend> m_backend;
};

inline Ref<RTCRtpScriptTransformerContext> RTCRtpScriptTransformerContext::create(Ref<RTCRtpTransformBackend>&& backend)
{
    return adoptRef(*new RTCRtpScriptTransformerContext(WTFMove(backend)));
}

inline RTCRtpScriptTransformerContext::RTCRtpScriptTransformerContext(Ref<RTCRtpTransformBackend>&& backend)
    : m_backend(WTFMove(backend))
{
}

inline ExceptionOr<void> RTCRtpScriptTransformerContext::requestKeyFrame()
{
    switch (mediaType()) {
    case MediaType::Audio:
        return Exception { InvalidStateError, "Cannot request key frame on audio sender"_s };
    case MediaType::Video:
        m_backend->requestKeyFrame();
        return { };
    default:
        return { };
    }
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
