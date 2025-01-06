/*
 *  Copyright (C) 2024 Igalia S.L. All rights reserved.
 *  Copyright (C) 2024 Metrological Group B.V.
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
#include "GStreamerRTPPacketizer.h"

#if USE(GSTREAMER_WEBRTC)

#include "GStreamerCommon.h"
#include <gst/rtp/rtp.h>
#include <wtf/PrintStream.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

GST_DEBUG_CATEGORY(webkit_webrtc_rtp_packetizer_debug);
#define GST_CAT_DEFAULT webkit_webrtc_rtp_packetizer_debug

GStreamerRTPPacketizer::GStreamerRTPPacketizer(GRefPtr<GstElement>&& encoder, GRefPtr<GstElement>&& payloader, GUniquePtr<GstStructure>&& encodingParameters)
    : m_encoder(WTFMove(encoder))
    , m_payloader(WTFMove(payloader))
    , m_encodingParameters(WTFMove(encodingParameters))
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtc_rtp_packetizer_debug, "webkitwebrtcrtppacketizer", 0, "WebKit WebRTC RTP Packetizer");
    });

    static Atomic<uint64_t> counter = 0;
    m_bin = gst_bin_new(makeString("rtp-packetizer-"_s, counter.exchangeAdd(1)).ascii().data());

    m_inputQueue = gst_element_factory_make("queue", nullptr);
    m_outputQueue = gst_element_factory_make("queue", nullptr);
    m_capsFilter = gst_element_factory_make("capsfilter", nullptr);
    m_valve = gst_element_factory_make("valve", nullptr);
    gst_bin_add_many(GST_BIN_CAST(m_bin.get()), m_inputQueue.get(), m_encoder.get(), m_payloader.get(), m_capsFilter.get(), m_outputQueue.get(), m_valve.get(), nullptr);

    auto sinkPad = adoptGRef(gst_element_get_static_pad(m_inputQueue.get(), "sink"));
    gst_element_add_pad(m_bin.get(), gst_ghost_pad_new("sink", sinkPad.get()));
    auto srcPad = adoptGRef(gst_element_get_static_pad(m_valve.get(), "src"));
    gst_element_add_pad(m_bin.get(), gst_ghost_pad_new("src", srcPad.get()));

    m_stats.reset(gst_structure_new_empty("stats"));

    if (m_encodingParameters)
        applyEncodingParameters(m_encodingParameters.get());
}

GStreamerRTPPacketizer::~GStreamerRTPPacketizer() = default;

void GStreamerRTPPacketizer::configureExtensions()
{
    m_lastExtensionId = 0;

    GValue extensions = G_VALUE_INIT;
    g_object_get_property(G_OBJECT(m_payloader.get()), "extensions", &extensions);
    m_lastExtensionId = gst_value_array_get_size(&extensions) + 1;
    g_value_unset(&extensions);

    if (!m_midExtension) {
        m_midExtension = adoptGRef(gst_rtp_header_extension_create_from_uri(GST_RTP_HDREXT_BASE "sdes:mid"));
        gst_rtp_header_extension_set_id(m_midExtension.get(), m_lastExtensionId);
        GST_DEBUG_OBJECT(m_bin.get(), "Created mid extension %" GST_PTR_FORMAT, m_midExtension.get());
        m_lastExtensionId++;
        g_signal_emit_by_name(m_payloader.get(), "add-extension", m_midExtension.get());
    }

    // Invalidate rid and re-cache it from encoding parameters.
    m_rid = emptyString();
    auto rid = rtpStreamId();
    m_rid = rid;

    if (!m_ridExtension) {
        m_ridExtension = adoptGRef(gst_rtp_header_extension_create_from_uri(GST_RTP_HDREXT_BASE "sdes:rtp-stream-id"));
        gst_rtp_header_extension_set_id(m_ridExtension.get(), m_lastExtensionId);
        m_lastExtensionId++;
        if (!rid.isEmpty())
            g_object_set(m_ridExtension.get(), "rid", rid.utf8().data(), nullptr);
        g_signal_emit_by_name(m_payloader.get(), "add-extension", m_ridExtension.get());
    }

    auto extension = adoptGRef(gst_rtp_header_extension_create_from_uri(GST_RTP_HDREXT_BASE "sdes:repaired-rtp-stream-id"));
    gst_rtp_header_extension_set_id(extension.get(), m_lastExtensionId);
    m_lastExtensionId++;
    if (!rid.isEmpty())
        g_object_set(extension.get(), "rid", rid.utf8().data(), nullptr);
    g_signal_emit_by_name(m_payloader.get(), "add-extension", extension.get());
}

void GStreamerRTPPacketizer::ensureMidExtension(const String& mid)
{
    m_mid = mid;
    if (m_midExtension) {
        g_object_set(m_midExtension.get(), "mid", mid.utf8().data(), nullptr);
        GST_DEBUG_OBJECT(m_bin.get(), "Existing mid extension %" GST_PTR_FORMAT " updated with mid %s", m_midExtension.get(), mid.utf8().data());
        return;
    }

    GValue extensions = G_VALUE_INIT;
    g_object_get_property(G_OBJECT(m_payloader.get()), "extensions", &extensions);
    auto totalExtensions = gst_value_array_get_size(&extensions);
    auto midURI = StringView::fromLatin1(GST_RTP_HDREXT_BASE "sdes:mid");
    for (unsigned i = 0; i < totalExtensions; i++) {
        const auto extension = GST_RTP_HEADER_EXTENSION_CAST(g_value_get_object(gst_value_array_get_value(&extensions, i)));
        auto uri = StringView::fromLatin1(gst_rtp_header_extension_get_uri(extension));
        if (uri != midURI)
            continue;

        m_midExtension = extension;
        GST_DEBUG_OBJECT(m_bin.get(), "Using mid extension %" GST_PTR_FORMAT, m_midExtension.get());
        g_object_set(extension, "mid", mid.utf8().data(), nullptr);
        GST_DEBUG_OBJECT(m_bin.get(), "Existing mid extension updated with mid %s", mid.utf8().data());
        break;
    }
    g_value_unset(&extensions);
    if (m_midExtension)
        return;

    GST_DEBUG_OBJECT(m_bin.get(), "Adding mid extension for mid %s", mid.ascii().data());
    m_midExtension = adoptGRef(gst_rtp_header_extension_create_from_uri(GST_RTP_HDREXT_BASE "sdes:mid"));
    gst_rtp_header_extension_set_id(m_midExtension.get(), totalExtensions + 1);
    GST_DEBUG_OBJECT(m_bin.get(), "Created mid extension %" GST_PTR_FORMAT, m_midExtension.get());
    g_object_set(m_midExtension.get(), "mid", mid.utf8().data(), nullptr);
    g_signal_emit_by_name(m_payloader.get(), "add-extension", m_midExtension.get());
}

GUniquePtr<GstStructure> GStreamerRTPPacketizer::rtpParameters() const
{
    GRefPtr<GstCaps> caps;
    g_object_get(m_capsFilter.get(), "caps", &caps.outPtr(), nullptr);
    if (gst_caps_is_any(caps.get()) || !gst_caps_get_size(caps.get()))
        return nullptr;
    return GUniquePtr<GstStructure>(gst_structure_copy(gst_caps_get_structure(caps.get(), 0)));
}

String GStreamerRTPPacketizer::rtpStreamId() const
{
    if (!m_rid.isEmpty())
        return m_rid;

    if (!m_encodingParameters)
        return emptyString();

    if (auto rid = gstStructureGetString(m_encodingParameters.get(), "rid"_s))
        return rid.toString();

    return emptyString();
}

int GStreamerRTPPacketizer::payloadType() const
{
    int payloadType;
    g_object_get(m_payloader.get(), "pt", &payloadType, nullptr);
    return payloadType;
}

unsigned GStreamerRTPPacketizer::currentSequenceNumberOffset() const
{
    unsigned result;
    g_object_get(m_payloader.get(), "seqnum-offset", &result, nullptr);
    return result;
}

void GStreamerRTPPacketizer::setSequenceNumberOffset(unsigned number)
{
    g_object_set(m_payloader.get(), "seqnum-offset", G_TYPE_UINT, number, nullptr);
}

struct ExtensionIdHolder {
    int extensionId { 0 };
};
WEBKIT_DEFINE_ASYNC_DATA_STRUCT(ExtensionIdHolder);

int GStreamerRTPPacketizer::findLastExtensionId(const GstCaps* caps)
{
    auto holder = createExtensionIdHolder();
    auto rtpStructure = gst_caps_get_structure(caps, 0);
    gstStructureForeach(rtpStructure, [&](auto id, const auto) -> bool {
        auto name = gstIdToString(id);
        if (!name.startsWith("extmap-"_s))
            return true;

        auto identifier = WTF::parseInteger<int>(name.substring(7));
        if (UNLIKELY(!identifier))
            return true;

        holder->extensionId = std::max(holder->extensionId, *identifier);
        return true;
    });
    int result = holder->extensionId;
    destroyExtensionIdHolder(holder);
    return result;
}

std::optional<std::pair<unsigned, GstStructure*>> GStreamerRTPPacketizer::stats() const
{
    GRefPtr<GstCaps> caps;
    g_object_get(m_capsFilter.get(), "caps", &caps.outPtr(), nullptr);
    if (!caps || gst_caps_is_empty(caps.get()) || gst_caps_is_any(caps.get()))
        return std::nullopt;

    auto structure = gst_caps_get_structure(caps.get(), 0);
    auto ssrc = gstStructureGet<unsigned>(structure, "ssrc"_s);
    if (!ssrc)
        return std::nullopt;

    return { { *ssrc, m_stats.get() } };
}

struct RTPPacketizerHolder {
    RefPtr<GStreamerRTPPacketizer> packetizer;
};
WEBKIT_DEFINE_ASYNC_DATA_STRUCT(RTPPacketizerHolder)

void GStreamerRTPPacketizer::startUpdatingStats()
{
    GST_DEBUG_OBJECT(m_bin.get(), "Starting buffer monitoring for stats gathering");
    auto holder = createRTPPacketizerHolder();
    holder->packetizer = this;
    auto pad = adoptGRef(gst_element_get_static_pad(m_encoder.get(), "src"));
    m_statsPadProbeId = gst_pad_add_probe(pad.get(), GST_PAD_PROBE_TYPE_BUFFER, [](GstPad*, GstPadProbeInfo*, gpointer userData) -> GstPadProbeReturn {
        auto packetizer = static_cast<RTPPacketizerHolder*>(userData)->packetizer;
        packetizer->updateStats();
        packetizer->updateStatsFromRTPExtensions();
        return GST_PAD_PROBE_OK;
    }, holder, reinterpret_cast<GDestroyNotify>(destroyRTPPacketizerHolder));
}

void GStreamerRTPPacketizer::updateStatsFromRTPExtensions()
{

    if (!m_mid.isEmpty())
        gst_structure_set(m_stats.get(), "mid", G_TYPE_STRING, m_mid.ascii().data(), nullptr);
    else if (gst_structure_has_field(m_stats.get(), "mid"))
        gst_structure_remove_field(m_stats.get(), "mid");

    if (!m_rid.isEmpty())
        gst_structure_set(m_stats.get(), "rid", G_TYPE_STRING, m_rid.ascii().data(), nullptr);
    else if (gst_structure_has_field(m_stats.get(), "rid"))
        gst_structure_remove_field(m_stats.get(), "rid");
}

void GStreamerRTPPacketizer::stopUpdatingStats()
{
    if (!m_statsPadProbeId)
        return;

    GST_DEBUG_OBJECT(m_bin.get(), "Stopping buffer monitoring for stats gathering");
    auto pad = adoptGRef(gst_element_get_static_pad(m_encoder.get(), "src"));
    gst_pad_remove_probe(pad.get(), m_statsPadProbeId);
    m_statsPadProbeId = 0;
}

void GStreamerRTPPacketizer::applyEncodingParameters(const GstStructure* encodingParameters) const
{
    ASSERT(encodingParameters);

    configure(encodingParameters);

    auto isActive = gstStructureGet<bool>(encodingParameters, "active"_s).value_or(true);
    GST_DEBUG_OBJECT(m_bin.get(), "Packetizer is active: %s", boolForPrinting(isActive));
    g_object_set(m_valve.get(), "drop", !isActive, nullptr);
    if (isActive)
        return;

    auto srcPad = adoptGRef(gst_element_get_static_pad(m_bin.get(), "src"));
    if (!srcPad)
        return;

    auto peer = adoptGRef(gst_pad_get_peer(srcPad.get()));
    gst_pad_send_event(peer.get(), gst_event_new_flush_start());
    gst_pad_send_event(peer.get(), gst_event_new_flush_stop(FALSE));
}

void GStreamerRTPPacketizer::reconfigure(GUniquePtr<GstStructure>&& encodingParameters)
{
    GST_DEBUG_OBJECT(m_bin.get(), "Re-configuring for encoding parameters: %" GST_PTR_FORMAT, encodingParameters.get());
    if (!encodingParameters)
        return;

    applyEncodingParameters(encodingParameters.get());
    m_encodingParameters = WTFMove(encodingParameters);
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
