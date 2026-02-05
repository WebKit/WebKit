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

#include "ContextDestructionObserverInlines.h"
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

Ref<GStreamerRtpSenderBackend> GStreamerRtpSenderBackend::create(WeakPtr<GStreamerPeerConnectionBackend>&& backend, GRefPtr<GstWebRTCRTPSender>&& rtcSender)
{
    return adoptRef(*new GStreamerRtpSenderBackend(WTF::move(backend), WTF::move(rtcSender)));
}

Ref<GStreamerRtpSenderBackend> GStreamerRtpSenderBackend::create(WeakPtr<GStreamerPeerConnectionBackend>&& backend, GRefPtr<GstWebRTCRTPSender>&& rtcSender, Source&& source, GUniquePtr<GstStructure>&& initData)
{
    return adoptRef(*new GStreamerRtpSenderBackend(WTF::move(backend), WTF::move(rtcSender), WTF::move(source), WTF::move(initData)));
}

GStreamerRtpSenderBackend::GStreamerRtpSenderBackend(WeakPtr<GStreamerPeerConnectionBackend>&& backend, GRefPtr<GstWebRTCRTPSender>&& rtcSender)
    : m_peerConnectionBackend(WTF::move(backend))
    , m_rtcSender(WTF::move(rtcSender))
{
    ensureDebugCategoryIsRegistered();
    GST_DEBUG_OBJECT(m_rtcSender.get(), "constructed without associated source");
}

GStreamerRtpSenderBackend::GStreamerRtpSenderBackend(WeakPtr<GStreamerPeerConnectionBackend>&& backend, GRefPtr<GstWebRTCRTPSender>&& rtcSender, Source&& source, GUniquePtr<GstStructure>&& initData)
    : m_peerConnectionBackend(WTF::move(backend))
    , m_rtcSender(WTF::move(rtcSender))
    , m_source(WTF::move(source))
    , m_initData(WTF::move(initData))
{
    ensureDebugCategoryIsRegistered();
    GST_DEBUG_OBJECT(m_rtcSender.get(), "constructed with associated source with init data: %" GST_PTR_FORMAT, m_initData.get());
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
    m_source = WTF::move(source);
    ASSERT(hasSource());

    if (!m_currentParameters && !m_initData)
        return;

    GUniquePtr<GstStructure> parameters(gst_structure_copy(m_currentParameters ? m_currentParameters.get() : m_initData.get()));
    switchOn(m_source, [&](Ref<RealtimeOutgoingAudioSourceGStreamer>& source) {
        source->setParameters(WTF::move(parameters));
    }, [&](Ref<RealtimeOutgoingVideoSourceGStreamer>& source) {
        source->setParameters(WTF::move(parameters));
    }, [](std::nullptr_t&) {
    });
}

void GStreamerRtpSenderBackend::takeSource(GStreamerRtpSenderBackend& backend)
{
    ASSERT(backend.hasSource());
    GST_DEBUG_OBJECT(m_rtcSender.get(), "Taking source from %" GST_PTR_FORMAT, backend.rtcSender());
    setSource(WTF::move(backend.m_source));
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
    switchOn(m_source, [&](Ref<RealtimeOutgoingAudioSourceGStreamer>& source) {
        source->stop([self = RefPtr { this }] {
            self->clearSource();
        });
    }, [&](Ref<RealtimeOutgoingVideoSourceGStreamer>& source) {
        source->stop([self = RefPtr { this }] {
            self->clearSource();
        });
    }, [&](std::nullptr_t&) {
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

    m_rtcSender = nullptr;
}

bool GStreamerRtpSenderBackend::replaceTrack(RTCRtpSender&, MediaStreamTrack* track)
{
    GST_DEBUG_OBJECT(m_rtcSender.get(), "Replacing sender track with track %p", track);

    RefPtr peerConnectionBackend = m_peerConnectionBackend.get();
    if (!peerConnectionBackend)
        return false;

    peerConnectionBackend->setReconfiguring(true);
    // FIXME: We might want to set the reconfiguring flag back to false once the webrtcbin sink pad
    // has renegotiated its caps. Perhaps a pad probe can be used for this.

    RefPtr newTrack = track;
    switchOn(m_source, [&](Ref<RealtimeOutgoingAudioSourceGStreamer>& source) {
        source->replaceTrack(newTrack);
    }, [&](Ref<RealtimeOutgoingVideoSourceGStreamer>& source) {
        source->replaceTrack(newTrack);
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

    auto kind = ""_s;
    switchOn(m_source, [&](Ref<RealtimeOutgoingAudioSourceGStreamer>&) {
        kind = "audio"_s;
    }, [&](Ref<RealtimeOutgoingVideoSourceGStreamer>&) {
        kind = "video"_s;
    }, [](const std::nullptr_t&) {
    });

    auto newParameters = fromRTCSendParameters(parameters, kind);
    if (newParameters.hasException()) {
        promise.reject(newParameters.releaseException());
        return;
    }
    switchOn(m_source, [&](Ref<RealtimeOutgoingAudioSourceGStreamer>& source) {
        source->setParameters(newParameters.releaseReturnValue());
    }, [&](Ref<RealtimeOutgoingVideoSourceGStreamer>& source) {
        source->setParameters(newParameters.releaseReturnValue());
    }, [](const std::nullptr_t&) {
    });

    if (!parameters.encodings.isEmpty()) {
        const auto& encoding = parameters.encodings.first();
        auto priorityType = fromRTCPriorityType(encoding.priority);
        g_object_set(m_rtcSender.get(), "priority", priorityType, nullptr);
    }
    promise.resolve();
}

std::unique_ptr<RTCDTMFSenderBackend> GStreamerRtpSenderBackend::createDTMFBackend()
{
    return makeUnique<GStreamerDTMFSenderBackend>(audioSourceWeak());
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
    return makeUnique<GStreamerDtlsTransportBackend>(WTF::move(transport));
}

void GStreamerRtpSenderBackend::dispatchBitrateRequest(uint32_t bitrate)
{
    switchOn(m_source, [&](Ref<RealtimeOutgoingAudioSourceGStreamer>& source) {
        source->dispatchBitrateRequest(bitrate);
    }, [&](Ref<RealtimeOutgoingVideoSourceGStreamer>& source) {
        source->dispatchBitrateRequest(bitrate);
    }, [](const std::nullptr_t&) { });
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)
