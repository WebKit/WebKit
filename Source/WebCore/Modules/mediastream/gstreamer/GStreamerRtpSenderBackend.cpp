/*
 *  Copyright (C) 2019-2022 Igalia S.L. All rights reserved.
 *  Copyright (C) 2022 Metrological Group B.V.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "GStreamerRtpSenderBackend.h"

#if ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)

#include "GStreamerDTMFSenderBackend.h"
#include "GStreamerDtlsTransportBackend.h"
#include "GStreamerPeerConnectionBackend.h"
#include "GStreamerRtpSenderTransformBackend.h"
#include "GStreamerWebRTCUtils.h"
#include "JSDOMPromiseDeferred.h"
#include "NotImplemented.h"
#include "RTCPeerConnection.h"
#include "RTCRtpSender.h"
#include "ScriptExecutionContext.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

GST_DEBUG_CATEGORY(webkit_webrtc_rtp_sender_debug);
#define GST_CAT_DEFAULT webkit_webrtc_rtp_sender_debug

static void ensureDebugCategoryIsRegistered()
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtc_rtp_sender_debug, "webkitwebrtcrtpsender", 0, "WebKit WebRTC RTP sender");
    });
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(GStreamerRtpSenderBackend);

GStreamerRtpSenderBackend::GStreamerRtpSenderBackend(GStreamerPeerConnectionBackend& backend, GRefPtr<GstWebRTCRTPSender>&& rtcSender)
    : m_peerConnectionBackend(WeakPtr { &backend })
    , m_rtcSender(WTFMove(rtcSender))
{
    ensureDebugCategoryIsRegistered();
    GST_DEBUG_OBJECT(m_rtcSender.get(), "constructed without associated source");
}

GStreamerRtpSenderBackend::GStreamerRtpSenderBackend(GStreamerPeerConnectionBackend& backend, GRefPtr<GstWebRTCRTPSender>&& rtcSender, Source&& source, GUniquePtr<GstStructure>&& initData)
    : m_peerConnectionBackend(WeakPtr { &backend })
    , m_rtcSender(WTFMove(rtcSender))
    , m_source(WTFMove(source))
    , m_initData(WTFMove(initData))
{
    ensureDebugCategoryIsRegistered();
    GST_DEBUG_OBJECT(m_rtcSender.get(), "constructed with associated source");
}

void GStreamerRtpSenderBackend::clearSource()
{
    ASSERT(hasSource());
    GST_DEBUG_OBJECT(m_rtcSender.get(), "Clearing source");
    m_source = nullptr;
}

void GStreamerRtpSenderBackend::setSource(Source&& source)
{
    ASSERT(!hasSource());
    GST_DEBUG_OBJECT(m_rtcSender.get(), "Setting source");
    m_source = WTFMove(source);
    ASSERT(hasSource());
}

void GStreamerRtpSenderBackend::takeSource(GStreamerRtpSenderBackend& backend)
{
    ASSERT(backend.hasSource());
    GST_DEBUG_OBJECT(m_rtcSender.get(), "Taking source from %" GST_PTR_FORMAT, backend.rtcSender());
    setSource(WTFMove(backend.m_source));
}

template<typename Source>
static inline bool updateTrackSource(Source& source, MediaStreamTrack* track)
{
    if (!track) {
        source.stop();
        return true;
    }
    return source.setTrack(track->privateTrack());
}

void GStreamerRtpSenderBackend::startSource()
{
    GST_DEBUG_OBJECT(m_rtcSender.get(), "Starting source");
    switchOn(m_source, [](Ref<RealtimeOutgoingAudioSourceGStreamer>& source) {
        source->start();
    }, [](Ref<RealtimeOutgoingVideoSourceGStreamer>& source) {
        source->start();
    }, [](std::nullptr_t&) {
    });
}

void GStreamerRtpSenderBackend::stopSource()
{
    GST_DEBUG_OBJECT(m_rtcSender.get(), "Stopping source");
    switchOn(m_source, [](Ref<RealtimeOutgoingAudioSourceGStreamer>& source) {
        source->stop();
    }, [](Ref<RealtimeOutgoingVideoSourceGStreamer>& source) {
        source->stop();
    }, [](std::nullptr_t&) {
    });
}

void GStreamerRtpSenderBackend::tearDown()
{
    WTF::switchOn(m_source, [](Ref<RealtimeOutgoingVideoSourceGStreamer>& source) {
        source->teardown();
    }, [](Ref<RealtimeOutgoingAudioSourceGStreamer>& source) {
        source->teardown();
    }, [&](std::nullptr_t&) {
    });
}

bool GStreamerRtpSenderBackend::replaceTrack(RTCRtpSender& sender, MediaStreamTrack* track)
{
    GST_DEBUG_OBJECT(m_rtcSender.get(), "Replacing sender track with track %p", track);
    if (!track) {
        stopSource();
        return true;
    }

    m_peerConnectionBackend->setReconfiguring(true);
    // FIXME: We might want to set the reconfiguring flag back to false once the webrtcbin sink pad
    // has renegotiated its caps. Perhaps a pad probe can be used for this.

    bool replace = true;
    if (!sender.track()) {
        m_source = m_peerConnectionBackend->createLinkedSourceForTrack(*track);
        replace = false;
    }

    switchOn(m_source, [&](Ref<RealtimeOutgoingAudioSourceGStreamer>& source) {
        ASSERT(track->source().type() == RealtimeMediaSource::Type::Audio);
        if (replace)
            source->replaceTrack(&track->privateTrack());
        source->start();
    }, [&](Ref<RealtimeOutgoingVideoSourceGStreamer>& source) {
        ASSERT(track->source().type() == RealtimeMediaSource::Type::Video);
        if (replace)
            source->replaceTrack(&track->privateTrack());
        source->start();
    }, [&](std::nullptr_t&) {
        GST_DEBUG_OBJECT(m_rtcSender.get(), "No outgoing source yet");
    });

    return true;
}

RTCRtpSendParameters GStreamerRtpSenderBackend::getParameters() const
{
    switchOn(m_source, [&](const Ref<RealtimeOutgoingAudioSourceGStreamer>& source) {
        m_currentParameters = source->parameters();
    }, [&](const Ref<RealtimeOutgoingVideoSourceGStreamer>& source) {
        m_currentParameters = source->parameters();
    }, [](const std::nullptr_t&) {
    });

    GST_DEBUG_OBJECT(m_rtcSender.get(), "Current parameters: %" GST_PTR_FORMAT, m_currentParameters.get());
    if (!m_currentParameters)
        return toRTCRtpSendParameters(m_initData.get());

    return toRTCRtpSendParameters(m_currentParameters.get());
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

void GStreamerRtpSenderBackend::setParameters(const RTCRtpSendParameters& parameters, DOMPromiseDeferred<void>&& promise)
{
    if (!hasSource()) {
        promise.reject(ExceptionCode::NotSupportedError);
        return;
    }

    if (!m_currentParameters) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "getParameters must be called before setParameters"_s });
        return;
    }

    if (!validateModifiedParameters(parameters, toRTCRtpSendParameters(m_currentParameters.get()))) {
        promise.reject(ExceptionCode::InvalidModificationError, "parameters are not valid"_s);
        return;
    }

    auto newParameters(fromRTCSendParameters(parameters));
    switchOn(m_source, [&](Ref<RealtimeOutgoingAudioSourceGStreamer>& source) {
        source->setParameters(WTFMove(newParameters));
    }, [&](Ref<RealtimeOutgoingVideoSourceGStreamer>& source) {
        source->setParameters(WTFMove(newParameters));
    }, [](const std::nullptr_t&) {
    });

    promise.resolve();
}

std::unique_ptr<RTCDTMFSenderBackend> GStreamerRtpSenderBackend::createDTMFBackend()
{
    return makeUnique<GStreamerDTMFSenderBackend>();
}

Ref<RTCRtpTransformBackend> GStreamerRtpSenderBackend::rtcRtpTransformBackend()
{
    return GStreamerRtpSenderTransformBackend::create(m_rtcSender);
}

void GStreamerRtpSenderBackend::setMediaStreamIds(const FixedVector<String>&)
{
    notImplemented();
}

std::unique_ptr<RTCDtlsTransportBackend> GStreamerRtpSenderBackend::dtlsTransportBackend()
{
    if (!m_rtcSender)
        return nullptr;

    GRefPtr<GstWebRTCDTLSTransport> transport;
    g_object_get(m_rtcSender.get(), "transport", &transport.outPtr(), nullptr);
    if (!transport)
        return nullptr;
    return makeUnique<GStreamerDtlsTransportBackend>(WTFMove(transport));
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)
