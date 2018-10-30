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

#include "config.h"
#include "LibWebRTCRtpReceiverBackend.h"

#include "LibWebRTCUtils.h"

#if ENABLE(WEB_RTC) && USE(LIBWEBRTC)

namespace WebCore {

RTCRtpParameters LibWebRTCRtpReceiverBackend::getParameters()
{
    return toRTCRtpParameters(m_rtcReceiver->GetParameters());
}

static inline void fillRTCRtpContributingSource(RTCRtpContributingSource& source, const webrtc::RtpSource& rtcSource)
{
    source.timestamp = rtcSource.timestamp_ms();
    source.source = rtcSource.source_id();
    if (rtcSource.audio_level())
        source.audioLevel = *rtcSource.audio_level();
}

static inline RTCRtpContributingSource toRTCRtpContributingSource(const webrtc::RtpSource& rtcSource)
{
    RTCRtpContributingSource source;
    fillRTCRtpContributingSource(source, rtcSource);
    return source;
}

static inline RTCRtpSynchronizationSource toRTCRtpSynchronizationSource(const webrtc::RtpSource& rtcSource)
{
    RTCRtpSynchronizationSource source;
    fillRTCRtpContributingSource(source, rtcSource);
    return source;
}

Vector<RTCRtpContributingSource> LibWebRTCRtpReceiverBackend::getContributingSources() const
{
    Vector<RTCRtpContributingSource> sources;
    for (auto& rtcSource : m_rtcReceiver->GetSources()) {
        if (rtcSource.source_type() == webrtc::RtpSourceType::CSRC)
            sources.append(toRTCRtpContributingSource(rtcSource));
    }
    return sources;
}

Vector<RTCRtpSynchronizationSource> LibWebRTCRtpReceiverBackend::getSynchronizationSources() const
{
    Vector<RTCRtpSynchronizationSource> sources;
    for (auto& rtcSource : m_rtcReceiver->GetSources()) {
        if (rtcSource.source_type() == webrtc::RtpSourceType::SSRC)
            sources.append(toRTCRtpSynchronizationSource(rtcSource));
    }
    return sources;
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(LIBWEBRTC)
