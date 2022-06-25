/*
 * Copyright (C) 2018 Apple Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEB_RTC) && USE(LIBWEBRTC)

#include "LibWebRTCMacros.h"
#include "RTCRtpReceiverBackend.h"
#include <webrtc/api/scoped_refptr.h>

namespace webrtc {
class RtpReceiverInterface;
}

namespace WebCore {
class Document;
class RealtimeMediaSource;

class LibWebRTCRtpReceiverBackend final : public RTCRtpReceiverBackend {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit LibWebRTCRtpReceiverBackend(rtc::scoped_refptr<webrtc::RtpReceiverInterface>&&);
    ~LibWebRTCRtpReceiverBackend();

    webrtc::RtpReceiverInterface* rtcReceiver() { return m_rtcReceiver.get(); }

    Ref<RealtimeMediaSource> createSource(Document&);

private:
    RTCRtpParameters getParameters() final;
    Vector<RTCRtpContributingSource> getContributingSources() const final;
    Vector<RTCRtpSynchronizationSource> getSynchronizationSources() const final;
    Ref<RTCRtpTransformBackend> rtcRtpTransformBackend() final;
    std::unique_ptr<RTCDtlsTransportBackend> dtlsTransportBackend() final;

    rtc::scoped_refptr<webrtc::RtpReceiverInterface> m_rtcReceiver;
    RefPtr<RTCRtpTransformBackend> m_transformBackend;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(LIBWEBRTC)
