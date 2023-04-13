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

GStreamerRtpSenderBackend::GStreamerRtpSenderBackend(GStreamerPeerConnectionBackend& backend, GRefPtr<GstWebRTCRTPSender>&& rtcSender, GUniquePtr<GstStructure>&& initData)
    : m_peerConnectionBackend(WeakPtr { &backend })
    , m_rtcSender(WTFMove(rtcSender))
    , m_initData(WTFMove(initData))
{
    ensureDebugCategoryIsRegistered();
}

GStreamerRtpSenderBackend::GStreamerRtpSenderBackend(GStreamerPeerConnectionBackend& backend, GRefPtr<GstWebRTCRTPSender>&& rtcSender, Source&& source, GUniquePtr<GstStructure>&& initData)
    : m_peerConnectionBackend(WeakPtr { &backend })
    , m_rtcSender(WTFMove(rtcSender))
    , m_source(WTFMove(source))
    , m_initData(WTFMove(initData))
{
    ensureDebugCategoryIsRegistered();
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
        if (replace) {
            source->stop();
            source->setSource(track->privateTrack());
            source->flush();
        }
        source->start();
    }, [&](Ref<RealtimeOutgoingVideoSourceGStreamer>& source) {
        ASSERT(track->source().type() == RealtimeMediaSource::Type::Video);
        if (replace) {
            source->stop();
            source->setSource(track->privateTrack());
            source->flush();
        }
        source->start();
    }, [&](std::nullptr_t&) {
        GST_DEBUG_OBJECT(m_rtcSender.get(), "No outgoing source yet");
    });

    return true;
}

RTCRtpSendParameters GStreamerRtpSenderBackend::getParameters() const
{
    return toRTCRtpSendParameters(m_initData.get());
}

void GStreamerRtpSenderBackend::setParameters(const RTCRtpSendParameters&, DOMPromiseDeferred<void>&& promise)
{
    if (!m_rtcSender) {
        promise.reject(NotSupportedError);
        return;
    }

    notImplemented();
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
    GRefPtr<GstWebRTCDTLSTransport> transport;
    g_object_get(m_rtcSender.get(), "transport", &transport.outPtr(), nullptr);
    return transport ? makeUnique<GStreamerDtlsTransportBackend>(transport) : nullptr;
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)
