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
#include "RealtimeOutgoingAudioSourceGStreamer.h"
#include "RealtimeOutgoingVideoSourceGStreamer.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

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
    switchOn(m_source, [](Ref<RealtimeOutgoingAudioSourceGStreamer>& source) {
        source->start();
    }, [](Ref<RealtimeOutgoingVideoSourceGStreamer>& source) {
        source->start();
    }, [](std::nullptr_t&) {
    });
}

WARN_UNUSED_RETURN GRefPtr<GstElement> GStreamerRtpSenderBackend::stopSource()
{
    switchOn(m_source, [](Ref<RealtimeOutgoingAudioSourceGStreamer>& source) {
        source->stop();
        return GRefPtr<GstElement>(source->bin());
    }, [](Ref<RealtimeOutgoingVideoSourceGStreamer>& source) {
        source->stop();
        return GRefPtr<GstElement>(source->bin());
    }, [](std::nullptr_t&) {
        return GRefPtr<GstElement>(nullptr);
    });
    return nullptr;
}

bool GStreamerRtpSenderBackend::replaceTrack(RTCRtpSender& sender, MediaStreamTrack* track)
{
    if (!track) {
        auto stoppedSource = stopSource();
        return true;
    }

    if (sender.track()) {
        switchOn(m_source, [&](Ref<RealtimeOutgoingAudioSourceGStreamer>& source) {
            ASSERT(track->source().type() == RealtimeMediaSource::Type::Audio);
            source->stop();
            source->setSource(track->privateTrack());
            source->start();
        }, [&](Ref<RealtimeOutgoingVideoSourceGStreamer>& source) {
            ASSERT(track->source().type() == RealtimeMediaSource::Type::Video);
            source->stop();
            source->setSource(track->privateTrack());
            source->start();
        }, [](std::nullptr_t&) {
        });
    }

    m_peerConnectionBackend->setSenderSourceFromTrack(*this, *track);
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
