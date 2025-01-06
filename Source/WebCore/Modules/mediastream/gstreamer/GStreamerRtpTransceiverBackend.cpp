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
#include "GStreamerRtpTransceiverBackend.h"

#if ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)

#include "GStreamerRtpReceiverBackend.h"
#include "GStreamerRtpSenderBackend.h"
#include "GStreamerWebRTCUtils.h"
#include "RTCRtpCodecCapability.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/glib/GUniquePtr.h>

GST_DEBUG_CATEGORY(webkit_webrtc_transceiver_debug);
#define GST_CAT_DEFAULT webkit_webrtc_transceiver_debug

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GStreamerRtpTransceiverBackend);

GStreamerRtpTransceiverBackend::GStreamerRtpTransceiverBackend(GRefPtr<GstWebRTCRTPTransceiver>&& rtcTransceiver)
    : m_rtcTransceiver(WTFMove(rtcTransceiver))
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtc_transceiver_debug, "webkitwebrtcrtptransceiver", 0, "WebKit WebRTC RTP transceiver");
    });

    GstWebRTCKind kind;
    g_object_get(m_rtcTransceiver.get(), "kind", &kind, nullptr);

    // FIXME: The ulp/red encoders drop MID extension headers. See also:
    // https://gitlab.freedesktop.org/gstreamer/gstreamer/-/issues/923
    // gst_util_set_object_arg(G_OBJECT(m_rtcTransceiver.get()), "fec-type", "ulp-red");

    // Enable nack only for video transceivers, so that RTX payloads are not signaled in SDP
    // offer/answer. Those are confusing some media servers... Internally webrtcbin will always
    // setup RTX, RED and FEC anyway.
    if (kind != GST_WEBRTC_KIND_VIDEO)
        return;

    g_object_set(m_rtcTransceiver.get(), "do-nack", TRUE, nullptr);
}

std::unique_ptr<GStreamerRtpReceiverBackend> GStreamerRtpTransceiverBackend::createReceiverBackend()
{
    return WTF::makeUnique<GStreamerRtpReceiverBackend>(GRefPtr(m_rtcTransceiver));
}

std::unique_ptr<GStreamerRtpSenderBackend> GStreamerRtpTransceiverBackend::createSenderBackend(GStreamerPeerConnectionBackend& backend, GStreamerRtpSenderBackend::Source&& source, GUniquePtr<GstStructure>&& initData)
{
    GRefPtr<GstWebRTCRTPSender> sender;
    g_object_get(m_rtcTransceiver.get(), "sender", &sender.outPtr(), nullptr);
    return WTF::makeUnique<GStreamerRtpSenderBackend>(backend, WTFMove(sender), WTFMove(source), WTFMove(initData));
}

RTCRtpTransceiverDirection GStreamerRtpTransceiverBackend::direction() const
{
    GstWebRTCRTPTransceiverDirection gstDirection;
    g_object_get(m_rtcTransceiver.get(), "direction", &gstDirection, nullptr);
    return toRTCRtpTransceiverDirection(gstDirection);
}

std::optional<RTCRtpTransceiverDirection> GStreamerRtpTransceiverBackend::currentDirection() const
{
    GstWebRTCRTPTransceiverDirection gstDirection;
    g_object_get(m_rtcTransceiver.get(), "current-direction", &gstDirection, nullptr);
    if (!gstDirection)
        return std::nullopt;
    return toRTCRtpTransceiverDirection(gstDirection);
}

void GStreamerRtpTransceiverBackend::setDirection(RTCRtpTransceiverDirection direction)
{
    auto gstDirection = fromRTCRtpTransceiverDirection(direction);
#ifndef GST_DISABLE_GST_DEBUG
    GUniquePtr<char> directionString(g_enum_to_string(GST_TYPE_WEBRTC_RTP_TRANSCEIVER_DIRECTION, gstDirection));
    GST_DEBUG_OBJECT(m_rtcTransceiver.get(), "Setting direction to %s", directionString.get());
#endif
    g_object_set(m_rtcTransceiver.get(), "direction", gstDirection, nullptr);
}

String GStreamerRtpTransceiverBackend::mid()
{
    GUniqueOutPtr<char> mid;
    g_object_get(m_rtcTransceiver.get(), "mid", &mid.outPtr(), nullptr);
    return String::fromUTF8(mid.get());
}

void GStreamerRtpTransceiverBackend::stop()
{
    // Ideally we should also stop webrtcbin transceivers but it's not supported yet.
    m_isStopped = true;
}

bool GStreamerRtpTransceiverBackend::stopped() const
{
    // Ideally this should be queried on webrtcbin, but its transceivers can't be stopped yet.
    return m_isStopped;
}

static inline WARN_UNUSED_RETURN ExceptionOr<GRefPtr<GstCaps>> toRtpCodecCapability(const RTCRtpCodecCapability& codec, int& dynamicPayloadType, const String& msid)
{
    if (!codec.mimeType.startsWith("video/"_s) && !codec.mimeType.startsWith("audio/"_s))
        return Exception { ExceptionCode::InvalidModificationError, "RTCRtpCodecCapability bad mimeType"_s };

    auto components = codec.mimeType.split('/');
    const auto mediaType = components[0];
    const auto codecName = components[1];

    int payloadType = payloadTypeForEncodingName(codecName).value_or(dynamicPayloadType++);
    auto caps = adoptGRef(gst_caps_new_simple("application/x-rtp", "media", G_TYPE_STRING, mediaType.ascii().data(), "encoding-name", G_TYPE_STRING, codecName.ascii().data(), "clock-rate", G_TYPE_INT, codec.clockRate, "payload", G_TYPE_INT, payloadType, nullptr));
    if (codec.channels)
        gst_caps_set_simple(caps.get(), "channels", G_TYPE_INT, *codec.channels, nullptr);

    if (!codec.sdpFmtpLine.isEmpty()) {
        // Forward each fmtp attribute as <fmtp-name> in the caps so that the downstream
        // webkitvideoencoder can take those into account when configuring the encoder. For instance
        // VP9 profile 2 requires a 10bit pixel input format, so a conversion might be needed just
        // before encoding. This is taken care of in the webkitvideoencoder itself.
        for (auto& attribute : codec.sdpFmtpLine.split(';')) {
            auto components = attribute.split('=');
            gst_caps_set_simple(caps.get(), components[0].ascii().data(), G_TYPE_STRING, components[1].ascii().data(), nullptr);
        }
    }

    if (!msid.isEmpty())
        gst_caps_set_simple(caps.get(), "a-msid", G_TYPE_STRING, msid.ascii().data(), nullptr);

    GST_DEBUG("Codec capability: %" GST_PTR_FORMAT, caps.get());
    return caps;
}

ExceptionOr<void> GStreamerRtpTransceiverBackend::setCodecPreferences(const Vector<RTCRtpCodecCapability>& codecs)
{
    GRefPtr<GstCaps> currentCaps;
    g_object_get(m_rtcTransceiver.get(), "codec-preferences", &currentCaps.outPtr(), nullptr);
    GST_TRACE_OBJECT(m_rtcTransceiver.get(), "Current codec preferences: %" GST_PTR_FORMAT, currentCaps.get());
    String msid;
    HashMap<String, String> extensions;
    if (gst_caps_get_size(currentCaps.get()) > 0) {
        auto structure = gst_caps_get_structure(currentCaps.get(), 0);
        if (auto msIdValue = gstStructureGetString(structure, "a-msid"_s))
            msid = msIdValue.toString();

        gstStructureForeach(structure, [&](auto id, const auto& value) -> bool {
            auto key = gstIdToString(id);
            if (!key.startsWith("extmap-"_s))
                return true;

            extensions.add(key.toString(), String::fromLatin1(g_value_get_string(value)));
            return true;
        });
    }

    auto gstCodecs = adoptGRef(gst_caps_new_empty());
    int dynamicPayloadType = 96;
    for (auto& codec : codecs) {
        auto result = toRtpCodecCapability(codec, dynamicPayloadType, msid);
        if (result.hasException())
            return result.releaseException();

        auto codecCaps = result.releaseReturnValue();

        // Restore extensions data on the first codec. It might be useful to do in the others too.
        if (!extensions.isEmpty()) {
            for (auto& [extensionId, url] : extensions)
                gst_caps_set_simple(codecCaps.get(), extensionId.ascii().data(), G_TYPE_STRING, url.ascii().data(), nullptr);
            extensions.clear();
        }
        gst_caps_append(gstCodecs.get(), codecCaps.leakRef());
    }
    GST_DEBUG_OBJECT(m_rtcTransceiver.get(), "Setting codec preferences to %" GST_PTR_FORMAT, gstCodecs.get());
    g_object_set(m_rtcTransceiver.get(), "codec-preferences", gstCodecs.get(), nullptr);
    return { };
}

void GStreamerRtpTransceiverBackend::tearDown()
{
    m_rtcTransceiver.clear();
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)
