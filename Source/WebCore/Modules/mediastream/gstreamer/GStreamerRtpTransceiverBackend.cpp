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
#include "NotImplemented.h"
#include "RTCRtpCodecCapability.h"
#include <wtf/glib/GUniquePtr.h>

GST_DEBUG_CATEGORY_EXTERN(webkit_webrtc_endpoint_debug);
#define GST_CAT_DEFAULT webkit_webrtc_endpoint_debug

namespace WebCore {

GStreamerRtpTransceiverBackend::GStreamerRtpTransceiverBackend(GRefPtr<GstWebRTCRTPTransceiver>&& rtcTransceiver)
    : m_rtcTransceiver(WTFMove(rtcTransceiver))
{
    // FIXME: Enable FEC once flexfec is available. ULPFEC is a disaster, see also
    // https://gitlab.freedesktop.org/gstreamer/gstreamer/-/issues/1407 ...
    g_object_set(m_rtcTransceiver.get(), "do-nack", TRUE, nullptr);
}

std::unique_ptr<GStreamerRtpReceiverBackend> GStreamerRtpTransceiverBackend::createReceiverBackend()
{
    GRefPtr<GstWebRTCRTPReceiver> receiver;
    g_object_get(m_rtcTransceiver.get(), "receiver", &receiver.outPtr(), nullptr);
    return WTF::makeUnique<GStreamerRtpReceiverBackend>(WTFMove(receiver));
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
    return String::fromLatin1(mid.get());
}

void GStreamerRtpTransceiverBackend::stop()
{
    // FIXME: webrtcbin transceivers can't be stopped yet.
    notImplemented();
}

bool GStreamerRtpTransceiverBackend::stopped() const
{
    // FIXME: webrtcbin transceivers can't be stopped yet.
    return false;
}

static inline WARN_UNUSED_RETURN ExceptionOr<GstCaps*> toRtpCodecCapability(const RTCRtpCodecCapability& codec, int payloadType)
{
    if (!codec.mimeType.startsWith("video/"_s) && !codec.mimeType.startsWith("audio/"_s))
        return Exception { InvalidModificationError, "RTCRtpCodecCapability bad mimeType"_s };

    auto components = codec.mimeType.split('/');
    const auto mediaType = components[0];
    const auto codecName = components[1];

    auto* caps = gst_caps_new_simple("application/x-rtp", "media", G_TYPE_STRING, mediaType.ascii().data(), "encoding-name", G_TYPE_STRING, codecName.ascii().data(), "clock-rate", G_TYPE_INT, codec.clockRate, "payload", G_TYPE_INT, payloadType, nullptr);
    if (codec.channels)
        gst_caps_set_simple(caps, "channels", G_TYPE_INT, *codec.channels, nullptr);

    if (!codec.sdpFmtpLine.isEmpty()) {
        // Forward each fmtp attribute as codec-<fmtp-name> in the caps so that the downstream
        // webrtcvideoencoder can take those into account when configuring the encoder. For instance
        // VP9 profile 2 requires a 10bit pixel input format, so a conversion might be needed just
        // before encoding. This is taken care of in the webrtcvideoencoder itself.
        for (auto& attribute : codec.sdpFmtpLine.split(';')) {
            auto components = attribute.split('=');
            auto field = makeString(codecName.convertToASCIILowercase(), '-', components[0]);
            gst_caps_set_simple(caps, field.ascii().data(), G_TYPE_STRING, components[1].ascii().data(), nullptr);
        }
    }

    GST_DEBUG("Codec capability: %" GST_PTR_FORMAT, caps);
    return caps;
}

ExceptionOr<void> GStreamerRtpTransceiverBackend::setCodecPreferences(const Vector<RTCRtpCodecCapability>& codecs)
{
    auto gstCodecs = adoptGRef(gst_caps_new_empty());
    int payloadType = 96;
    for (auto& codec : codecs) {
        auto result = toRtpCodecCapability(codec, payloadType);
        if (result.hasException())
            return result.releaseException();
        payloadType++;
        gst_caps_append(gstCodecs.get(), result.releaseReturnValue());
    }
    g_object_set(m_rtcTransceiver.get(), "codec-preferences", gstCodecs.get(), nullptr);
    return { };
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)
