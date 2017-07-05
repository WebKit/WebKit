/*
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEB_RTC)

#include "RTCRtpParameters.h"
#include "RTCRtpSenderReceiverBase.h"

namespace WebCore {


class RTCRtpReceiver : public RTCRtpSenderReceiverBase {
public:
    class Backend {
    public:
        virtual ~Backend() { }
        virtual RTCRtpParameters getParameters() { return { }; }
    };

    static Ref<RTCRtpReceiver> create(Ref<MediaStreamTrack>&& track, Backend* backend = nullptr)
    {
        return adoptRef(*new RTCRtpReceiver(WTFMove(track), backend));
    }

    void stop();

    // FIXME: We should pass a UniqueRef here.
    void setBackend(std::unique_ptr<Backend>&& backend) { m_backend = WTFMove(backend); }

    bool isDispatched() const { return m_isDispatched; }
    void setDispatched(bool isDispatched) { m_isDispatched = isDispatched; }
    RTCRtpParameters getParameters() { return m_backend ? m_backend->getParameters() : RTCRtpParameters(); }

private:
    explicit RTCRtpReceiver(Ref<MediaStreamTrack>&&, Backend*);

    bool m_isDispatched { false };
    std::unique_ptr<Backend> m_backend;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
