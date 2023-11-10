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

#include "JSDOMPromiseDeferred.h"
#include "LibWebRTCDTMFSenderBackend.h"
#include "LibWebRTCDtlsTransportBackend.h"
#include "LibWebRTCPeerConnectionBackend.h"
#include "LibWebRTCRtpSenderTransformBackend.h"
#include "LibWebRTCUtils.h"
#include "RTCPeerConnection.h"
#include "RTCRtpSender.h"
#include "RTCRtpTransformBackend.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

LibWebRTCRtpSenderBackend::LibWebRTCRtpSenderBackend(LibWebRTCPeerConnectionBackend& backend, rtc::scoped_refptr<webrtc::RtpSenderInterface>&& rtcSender)
    : m_peerConnectionBackend(backend)
    , m_rtcSender(WTFMove(rtcSender))
{
}

LibWebRTCRtpSenderBackend::LibWebRTCRtpSenderBackend(LibWebRTCPeerConnectionBackend& backend, rtc::scoped_refptr<webrtc::RtpSenderInterface>&& rtcSender, Source&& source)
    : m_peerConnectionBackend(backend)
    , m_rtcSender(WTFMove(rtcSender))
    , m_source(WTFMove(source))
{
    startSource();
}

LibWebRTCRtpSenderBackend::~LibWebRTCRtpSenderBackend()
{
    stopSource();
}

void LibWebRTCRtpSenderBackend::startSource()
{
    // We asynchronously start the sources to guarantee media goes through the transform if a transform is set when creating the track.
    callOnMainThread([this, weakThis = WeakPtr { *this }, source = m_source]() mutable {
        if (!weakThis)
            return;
        switchOn(source, [this](Ref<RealtimeOutgoingAudioSource>& source) {
            if (auto* currentSource = std::get_if<Ref<RealtimeOutgoingAudioSource>>(&m_source); currentSource && currentSource->ptr() == source.ptr())
                source->start();
        }, [this](Ref<RealtimeOutgoingVideoSource>& source) {
            if (auto* currentSource = std::get_if<Ref<RealtimeOutgoingVideoSource>>(&m_source); currentSource && currentSource->ptr() == source.ptr())
                source->start();
        }, [](std::nullptr_t&) {
        });
    });
}

void LibWebRTCRtpSenderBackend::stopSource()
{
    switchOn(m_source, [](Ref<RealtimeOutgoingAudioSource>& source) {
        source->stop();
    }, [](Ref<RealtimeOutgoingVideoSource>& source) {
        source->stop();
    }, [](std::nullptr_t&) {
    });
    m_source = nullptr;
}

bool LibWebRTCRtpSenderBackend::replaceTrack(RTCRtpSender& sender, MediaStreamTrack* track)
{
    if (!track) {
        stopSource();
        return true;
    }

    if (sender.track()) {
        switchOn(m_source, [&](Ref<RealtimeOutgoingAudioSource>& source) {
            ASSERT(track->source().type() == RealtimeMediaSource::Type::Audio);
            source->stop();
            source->setSource(track->privateTrack());
            source->start();
        }, [&](Ref<RealtimeOutgoingVideoSource>& source) {
            ASSERT(track->source().type() == RealtimeMediaSource::Type::Video);
            source->stop();
            source->setSource(track->privateTrack());
            source->start();
        }, [](std::nullptr_t&) {
            ASSERT_NOT_REACHED();
        });
    }

    m_peerConnectionBackend->setSenderSourceFromTrack(*this, *track);
    return true;
}

RTCRtpSendParameters LibWebRTCRtpSenderBackend::getParameters() const
{
    if (!m_rtcSender)
        return { };

    m_currentParameters = m_rtcSender->GetParameters();
    return toRTCRtpSendParameters(*m_currentParameters);
}

static bool validateModifiedParameters(const RTCRtpSendParameters& newParameters, const RTCRtpSendParameters& oldParameters)
{
    if (oldParameters.transactionId != newParameters.transactionId)
        return false;

    if (oldParameters.encodings.size() != newParameters.encodings.size())
        return false;

    for (size_t i = 0; i < oldParameters.encodings.size(); ++i) {
        if (oldParameters.encodings[i].rid != newParameters.encodings[i].rid)
            return false;
    }

    if (oldParameters.headerExtensions.size() != newParameters.headerExtensions.size())
        return false;

    for (size_t i = 0; i < oldParameters.headerExtensions.size(); ++i) {
        const auto& oldExtension = oldParameters.headerExtensions[i];
        const auto& newExtension = newParameters.headerExtensions[i];
        if (oldExtension.uri != newExtension.uri || oldExtension.id != newExtension.id)
            return false;
    }

    if (oldParameters.rtcp.cname != newParameters.rtcp.cname)
        return false;

    if (!!oldParameters.rtcp.reducedSize != !!newParameters.rtcp.reducedSize)
        return false;

    if (oldParameters.rtcp.reducedSize && *oldParameters.rtcp.reducedSize != *newParameters.rtcp.reducedSize)
        return false;

    if (oldParameters.codecs.size() != newParameters.codecs.size())
        return false;

    for (size_t i = 0; i < oldParameters.codecs.size(); ++i) {
        const auto& oldCodec = oldParameters.codecs[i];
        const auto& newCodec = newParameters.codecs[i];
        if (oldCodec.payloadType != newCodec.payloadType
            || oldCodec.mimeType != newCodec.mimeType
            || oldCodec.clockRate != newCodec.clockRate
            || oldCodec.channels != newCodec.channels
            || oldCodec.sdpFmtpLine != newCodec.sdpFmtpLine)
            return false;
    }

    return true;
}

void LibWebRTCRtpSenderBackend::setParameters(const RTCRtpSendParameters& parameters, DOMPromiseDeferred<void>&& promise)
{
    if (!m_rtcSender) {
        promise.reject(ExceptionCode::NotSupportedError);
        return;
    }

    if (!m_currentParameters) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "getParameters must be called before setParameters"_s });
        return;
    }

    if (!validateModifiedParameters(parameters, toRTCRtpSendParameters(*m_currentParameters))) {
        promise.reject(ExceptionCode::InvalidModificationError, "parameters are not valid"_s);
        return;
    }

    auto rtcParameters = WTFMove(*m_currentParameters);
    updateRTCRtpSendParameters(parameters, rtcParameters);
    m_currentParameters = std::nullopt;

    auto error = m_rtcSender->SetParameters(rtcParameters);
    if (!error.ok()) {
        promise.reject(toException(error));
        return;
    }
    promise.resolve();
}

std::unique_ptr<RTCDTMFSenderBackend> LibWebRTCRtpSenderBackend::createDTMFBackend()
{
    return makeUnique<LibWebRTCDTMFSenderBackend>(m_rtcSender->GetDtmfSender());
}

Ref<RTCRtpTransformBackend> LibWebRTCRtpSenderBackend::rtcRtpTransformBackend()
{
    if (!m_transformBackend)
        m_transformBackend = LibWebRTCRtpSenderTransformBackend::create(m_rtcSender);
    return *m_transformBackend;
}

std::unique_ptr<RTCDtlsTransportBackend> LibWebRTCRtpSenderBackend::dtlsTransportBackend()
{
    auto backend = m_rtcSender->dtls_transport();
    return backend ? makeUnique<LibWebRTCDtlsTransportBackend>(WTFMove(backend)) : nullptr;
}

void LibWebRTCRtpSenderBackend::setMediaStreamIds(const FixedVector<String>& streamIds)
{
    std::vector<std::string> ids;
    for (auto& id : streamIds)
        ids.push_back(id.utf8().data());
    m_rtcSender->SetStreams(ids);
}

RealtimeOutgoingVideoSource* LibWebRTCRtpSenderBackend::videoSource()
{
    return WTF::switchOn(m_source,
        [](Ref<RealtimeOutgoingVideoSource>& source) { return source.ptr(); },
        [](const auto&) -> RealtimeOutgoingVideoSource* { return nullptr; }
    );
}

bool LibWebRTCRtpSenderBackend::hasSource() const
{
    return WTF::switchOn(m_source,
        [](const std::nullptr_t&) { return false; },
        [](const auto&) { return true; }
    );
}

void LibWebRTCRtpSenderBackend::setSource(Source&& source)
{
    stopSource();
    m_source = std::exchange(source, nullptr);
    startSource();
}

void LibWebRTCRtpSenderBackend::takeSource(LibWebRTCRtpSenderBackend& backend)
{
    ASSERT(backend.hasSource());
    setSource(WTFMove(backend.m_source));
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(LIBWEBRTC)
