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
#include "LibWebRTCRtpSenderBackend.h"

#if ENABLE(WEB_RTC) && USE(LIBWEBRTC)

#include "LibWebRTCPeerConnectionBackend.h"
#include "LibWebRTCUtils.h"
#include "RTCPeerConnection.h"
#include "RTCRtpSender.h"
#include "RuntimeEnabledFeatures.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

template<typename Source>
static inline bool updateTrackSource(Source& source, MediaStreamTrack* track)
{
    if (!track) {
        source.stop();
        return true;
    }
    return source.setSource(track->privateTrack());
}

void LibWebRTCRtpSenderBackend::replaceTrack(ScriptExecutionContext& context, RTCRtpSender& sender, RefPtr<MediaStreamTrack>&& track, DOMPromiseDeferred<void>&& promise)
{
    if (!m_peerConnectionBackend) {
        promise.reject(Exception { InvalidStateError, "No WebRTC backend"_s });
        return;
    }

    auto* currentTrack = sender.track();

    ASSERT(!track || !currentTrack || currentTrack->source().type() == track->source().type());
    if (currentTrack) {
    switch (currentTrack->source().type()) {
    case RealtimeMediaSource::Type::None:
        ASSERT_NOT_REACHED();
        promise.reject(InvalidModificationError);
        break;
    case RealtimeMediaSource::Type::Audio:
        if (!updateTrackSource(*audioSource(), track.get())) {
            promise.reject(InvalidModificationError);
            return;
        }
        break;
    case RealtimeMediaSource::Type::Video:
        if (!updateTrackSource(*videoSource(), track.get())) {
            promise.reject(InvalidModificationError);
            return;
        }
        break;
    }
    }

    // FIXME: Remove this postTask once this whole function is executed as part of the RTCPeerConnection operation queue.
    context.postTask([protectedSender = makeRef(sender), promise = WTFMove(promise), track = WTFMove(track), this](ScriptExecutionContext&) mutable {
        if (protectedSender->isStopped())
            return;

        if (!track) {
            protectedSender->setTrackToNull();
            promise.resolve();
            return;
        }

        bool hasTrack = protectedSender->track();
        protectedSender->setTrack(track.releaseNonNull());

        if (hasTrack) {
            promise.resolve();
            return;
        }

        if (RuntimeEnabledFeatures::sharedFeatures().webRTCUnifiedPlanEnabled()) {
            m_source = nullptr;
            m_peerConnectionBackend->setSenderSourceFromTrack(*this, *protectedSender->track());
            promise.resolve();
            return;
        }

        auto result = m_peerConnectionBackend->addTrack(*protectedSender->track(), { });
        if (result.hasException()) {
            promise.reject(result.releaseException());
            return;
        }
        promise.resolve();
    });
}

RTCRtpSendParameters LibWebRTCRtpSenderBackend::getParameters() const
{
    if (!m_rtcSender)
        return { };

    return toRTCRtpSendParameters(m_rtcSender->GetParameters());
}

void LibWebRTCRtpSenderBackend::setParameters(const RTCRtpSendParameters& parameters, DOMPromiseDeferred<void>&& promise)
{
    if (!m_rtcSender) {
        promise.reject(NotSupportedError);
        return;
    }
    auto error = m_rtcSender->SetParameters(fromRTCRtpSendParameters(parameters));
    if (!error.ok()) {
        promise.reject(Exception { InvalidStateError, error.message() });
        return;
    }
    promise.resolve();
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(LIBWEBRTC)
