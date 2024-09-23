/*
 *  Copyright (C) 2017-2022 Igalia S.L. All rights reserved.
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
#include "RealtimeOutgoingMediaSourceGStreamer.h"

#if USE(GSTREAMER_WEBRTC)

#include "GStreamerCommon.h"
#include "GStreamerMediaStreamSource.h"
#include "MediaStreamTrack.h"

#define GST_USE_UNSTABLE_API
#include <gst/webrtc/webrtc.h>
#undef GST_USE_UNSTABLE_API

#include <wtf/UUID.h>
#include <wtf/text/MakeString.h>

GST_DEBUG_CATEGORY(webkit_webrtc_outgoing_media_debug);
#define GST_CAT_DEFAULT webkit_webrtc_outgoing_media_debug

namespace WebCore {

RealtimeOutgoingMediaSourceGStreamer::RealtimeOutgoingMediaSourceGStreamer(Type type, const RefPtr<UniqueSSRCGenerator>& ssrcGenerator, const String& mediaStreamId, MediaStreamTrack& track)
    : m_type(type)
    , m_mediaStreamId(mediaStreamId)
    , m_trackId(track.id())
    , m_ssrcGenerator(ssrcGenerator)
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtc_outgoing_media_debug, "webkitwebrtcoutgoingmedia", 0, "WebKit WebRTC outgoing media");
    });

    m_bin = gst_bin_new(nullptr);

    if (track.isCanvas()) {
        m_liveSync = makeGStreamerElement("livesync", nullptr);
        if (!m_liveSync) {
            GST_WARNING_OBJECT(m_bin.get(), "GStreamer element livesync not found. Canvas streaming to PeerConnection will not work as expected, falling back to identity element.");
            m_liveSync = gst_element_factory_make("identity", nullptr);
        }
    } else
        m_liveSync = gst_element_factory_make("identity", nullptr);

    // Both livesync and identity have a single-segment property, so no need for checks here.
    g_object_set(m_liveSync.get(), "single-segment", TRUE, nullptr);

    m_preEncoderQueue = gst_element_factory_make("queue", nullptr);
    m_postEncoderQueue = gst_element_factory_make("queue", nullptr);
    m_capsFilter = gst_element_factory_make("capsfilter", nullptr);

    gst_bin_add_many(GST_BIN_CAST(m_bin.get()), m_liveSync.get(), m_preEncoderQueue.get(), m_postEncoderQueue.get(), m_capsFilter.get(), nullptr);

    auto srcPad = adoptGRef(gst_element_get_static_pad(m_capsFilter.get(), "src"));
    gst_element_add_pad(m_bin.get(), gst_ghost_pad_new("src", srcPad.get()));

    m_track = &track.privateTrack();
    initializeFromTrack();
}

RealtimeOutgoingMediaSourceGStreamer::~RealtimeOutgoingMediaSourceGStreamer()
{
    teardown();
}

const GRefPtr<GstCaps>& RealtimeOutgoingMediaSourceGStreamer::allowedCaps() const
{
    if (m_allowedCaps)
        return m_allowedCaps;

    auto sdpMsIdLine = makeString(m_mediaStreamId, ' ', m_trackId);
    m_allowedCaps = capsFromRtpCapabilities(m_ssrcGenerator, rtpCapabilities(), [&sdpMsIdLine](GstStructure* structure) {
        gst_structure_set(structure, "a-msid", G_TYPE_STRING, sdpMsIdLine.utf8().data(), nullptr);
    });

    GST_DEBUG_OBJECT(m_bin.get(), "Allowed caps: %" GST_PTR_FORMAT, m_allowedCaps.get());
    return m_allowedCaps;
}

void RealtimeOutgoingMediaSourceGStreamer::start()
{
    if (!m_isStopped) {
        GST_DEBUG_OBJECT(m_bin.get(), "Source already started");
        return;
    }

    GST_DEBUG_OBJECT(m_bin.get(), "Starting outgoing source");
    m_track->addObserver(*this);
    m_isStopped = false;

    if (m_transceiver) {
        auto pad = adoptGRef(gst_element_get_static_pad(m_liveSync.get(), "src"));
        if (!gst_pad_is_linked(pad.get())) {
            GST_DEBUG_OBJECT(m_bin.get(), "Codec preferences haven't changed before startup, ensuring source is linked");
            codecPreferencesChanged();
        }
    }

    gst_element_link(m_outgoingSource.get(), m_liveSync.get());
    gst_element_sync_state_with_parent(m_bin.get());
}

void RealtimeOutgoingMediaSourceGStreamer::stop()
{
    GST_DEBUG_OBJECT(m_bin.get(), "Stopping outgoing source");
    m_isStopped = true;
    stopOutgoingSource();
    m_track = nullptr;
}

void RealtimeOutgoingMediaSourceGStreamer::flush()
{
    gst_element_send_event(m_outgoingSource.get(), gst_event_new_flush_start());
    gst_element_send_event(m_outgoingSource.get(), gst_event_new_flush_stop(FALSE));
}

void RealtimeOutgoingMediaSourceGStreamer::stopOutgoingSource()
{
    if (!m_track)
        return;

    GST_DEBUG_OBJECT(m_bin.get(), "Stopping outgoing source %" GST_PTR_FORMAT, m_outgoingSource.get());
    m_track->removeObserver(*this);

    if (!m_outgoingSource)
        return;

    webkitMediaStreamSrcSignalEndOfStream(WEBKIT_MEDIA_STREAM_SRC(m_outgoingSource.get()));

    gst_element_set_locked_state(m_outgoingSource.get(), TRUE);

    gst_element_unlink(m_outgoingSource.get(), m_liveSync.get());

    gst_element_set_state(m_outgoingSource.get(), GST_STATE_NULL);
    gst_bin_remove(GST_BIN_CAST(m_bin.get()), m_outgoingSource.get());
    gst_element_set_locked_state(m_outgoingSource.get(), FALSE);
    m_outgoingSource.clear();
}

void RealtimeOutgoingMediaSourceGStreamer::sourceMutedChanged()
{
    if (!m_track)
        return;
    ASSERT(m_muted != m_track->muted());
    m_muted = m_track->muted();
    GST_DEBUG_OBJECT(m_bin.get(), "Mute state changed to %s", boolForPrinting(m_muted));
}

void RealtimeOutgoingMediaSourceGStreamer::sourceEnabledChanged()
{
    if (!m_track)
        return;

    m_enabled = m_track->enabled();
    GST_DEBUG_OBJECT(m_bin.get(), "Enabled state changed to %s", boolForPrinting(m_enabled));
}

void RealtimeOutgoingMediaSourceGStreamer::initializeFromTrack()
{
    if (!m_track)
        return;
    m_muted = m_track->muted();
    m_enabled = m_track->enabled();
    GST_DEBUG_OBJECT(m_bin.get(), "Initializing from track, muted: %s, enabled: %s", boolForPrinting(m_muted), boolForPrinting(m_enabled));

    if (m_outgoingSource)
        return;

    m_outgoingSource = webkitMediaStreamSrcNew();
    GST_DEBUG_OBJECT(m_bin.get(), "Created outgoing source %" GST_PTR_FORMAT, m_outgoingSource.get());
    gst_bin_add(GST_BIN_CAST(m_bin.get()), m_outgoingSource.get());
    webkitMediaStreamSrcAddTrack(WEBKIT_MEDIA_STREAM_SRC(m_outgoingSource.get()), m_track.get());
    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN_CAST(m_bin.get()), GST_DEBUG_GRAPH_SHOW_ALL, "outgoing-media-track-initialized");
}

void RealtimeOutgoingMediaSourceGStreamer::link()
{
    GST_DEBUG_OBJECT(m_bin.get(), "Linking webrtcbin pad %" GST_PTR_FORMAT, m_webrtcSinkPad.get());
    gst_element_link(m_postEncoderQueue.get(), m_capsFilter.get());

    auto srcPad = adoptGRef(gst_element_get_static_pad(m_bin.get(), "src"));
    gst_pad_link(srcPad.get(), m_webrtcSinkPad.get());
}

void RealtimeOutgoingMediaSourceGStreamer::setSinkPad(GRefPtr<GstPad>&& pad)
{
    GST_DEBUG_OBJECT(m_bin.get(), "Associating with webrtcbin pad %" GST_PTR_FORMAT, pad.get());
    m_webrtcSinkPad = WTFMove(pad);

    if (m_transceiver)
        g_signal_handlers_disconnect_by_data(m_transceiver.get(), this);

    g_object_get(m_webrtcSinkPad.get(), "transceiver", &m_transceiver.outPtr(), nullptr);
    g_signal_connect_swapped(m_transceiver.get(), "notify::codec-preferences", G_CALLBACK(+[](RealtimeOutgoingMediaSourceGStreamer* source) {
        source->codecPreferencesChanged();
    }), this);
    g_object_get(m_transceiver.get(), "sender", &m_sender.outPtr(), nullptr);
}

GUniquePtr<GstStructure> RealtimeOutgoingMediaSourceGStreamer::parameters()
{
    if (!m_parameters) {
        auto transactionId = createVersion4UUIDString();
        m_parameters.reset(gst_structure_new("send-parameters", "transaction-id", G_TYPE_STRING, transactionId.ascii().data(), nullptr));

        GUniquePtr<GstStructure> encodingParameters(gst_structure_new("encoding-parameters", "active", G_TYPE_BOOLEAN, TRUE, nullptr));

        if (m_payloader) {
            uint32_t ssrc;
            g_object_get(m_payloader.get(), "ssrc", &ssrc, nullptr);
            gst_structure_set(encodingParameters.get(), "ssrc", G_TYPE_UINT, ssrc, nullptr);
        }
        fillEncodingParameters(encodingParameters);

        GValue encodingsValue = G_VALUE_INIT;
        g_value_init(&encodingsValue, GST_TYPE_LIST);
        GValue value = G_VALUE_INIT;
        g_value_init(&value, GST_TYPE_STRUCTURE);
        gst_value_set_structure(&value, encodingParameters.get());
        gst_value_list_append_value(&encodingsValue, &value);
        g_value_unset(&value);
        gst_structure_take_value(m_parameters.get(), "encodings", &encodingsValue);
    }
    return GUniquePtr<GstStructure>(gst_structure_copy(m_parameters.get()));
}

void RealtimeOutgoingMediaSourceGStreamer::teardown()
{
    if (m_transceiver)
        g_signal_handlers_disconnect_by_data(m_transceiver.get(), this);

    stopOutgoingSource();

    if (GST_IS_PAD(m_webrtcSinkPad.get())) {
        auto srcPad = adoptGRef(gst_element_get_static_pad(m_bin.get(), "src"));
        if (gst_pad_unlink(srcPad.get(), m_webrtcSinkPad.get())) {
            GST_DEBUG_OBJECT(m_bin.get(), "Removing webrtcbin pad %" GST_PTR_FORMAT, m_webrtcSinkPad.get());
            if (auto parent = adoptGRef(gst_pad_get_parent_element(m_webrtcSinkPad.get())))
                gst_element_release_request_pad(parent.get(), m_webrtcSinkPad.get());
        }
    }

    gst_element_set_locked_state(m_bin.get(), TRUE);
    gst_element_set_state(m_bin.get(), GST_STATE_NULL);
    if (auto pipeline = adoptGRef(gst_element_get_parent(m_bin.get())))
        gst_bin_remove(GST_BIN_CAST(pipeline.get()), m_bin.get());
    gst_element_set_locked_state(m_bin.get(), FALSE);

    m_bin.clear();
    m_liveSync.clear();
    m_valve.clear();
    m_preEncoderQueue.clear();
    m_encoder.clear();
    m_payloader.clear();
    m_postEncoderQueue.clear();
    m_capsFilter.clear();
    m_allowedCaps.clear();
    m_transceiver.clear();
    m_sender.clear();
    m_webrtcSinkPad.clear();
    m_parameters.reset();
}

void RealtimeOutgoingMediaSourceGStreamer::unlinkPayloader()
{
    PayloaderState state;
    g_object_get(m_payloader.get(), "seqnum", &state.seqnum, nullptr);
    if (state.seqnum < 65535)
        state.seqnum++;
    m_payloaderState = state;

    if (m_type == RealtimeOutgoingMediaSourceGStreamer::Type::Audio)
        gst_element_set_state(m_encoder.get(), GST_STATE_NULL);
    gst_element_set_state(m_payloader.get(), GST_STATE_NULL);
    if (m_type == RealtimeOutgoingMediaSourceGStreamer::Type::Audio) {
        gst_element_unlink_many(m_preEncoderQueue.get(), m_encoder.get(), m_payloader.get(), m_postEncoderQueue.get(), nullptr);
        gst_bin_remove_many(GST_BIN_CAST(m_bin.get()), m_payloader.get(), m_encoder.get(), nullptr);
        m_encoder.clear();
    } else {
        gst_element_unlink_many(m_encoder.get(), m_payloader.get(), m_postEncoderQueue.get(), nullptr);
        gst_bin_remove(GST_BIN_CAST(m_bin.get()), m_payloader.get());
    }
    m_payloader.clear();
}

void RealtimeOutgoingMediaSourceGStreamer::codecPreferencesChanged()
{
    if (m_padBlockedProbe)
        return;

    GRefPtr<GstCaps> codecPreferences;
    g_object_get(m_transceiver.get(), "codec-preferences", &codecPreferences.outPtr(), nullptr);
    GST_DEBUG_OBJECT(m_bin.get(), "Codec preferences changed on transceiver %" GST_PTR_FORMAT " to: %" GST_PTR_FORMAT, m_transceiver.get(), codecPreferences.get());

    if (m_payloader) {
        // We have a linked encoder/payloader, so to replace the audio encoder and audio/video
        // payloader we need to block upstream data flow, send an EOS event to the first element we
        // want to remove (encoder for audio, payloader for video) and wait it reaches the payloader
        // source pad. Then we can unlink/clean-up elements.
        auto srcPad = adoptGRef(gst_element_get_static_pad(m_preEncoderQueue.get(), "src"));
        m_padBlockedProbe = gst_pad_add_probe(srcPad.get(), GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM, reinterpret_cast<GstPadProbeCallback>(+[](GstPad* pad, GstPadProbeInfo* info, gpointer userData) -> GstPadProbeReturn {
            gst_pad_remove_probe(pad, GST_PAD_PROBE_INFO_ID(info));

            auto self = reinterpret_cast<RealtimeOutgoingMediaSourceGStreamer*>(userData);
            auto srcPad = adoptGRef(gst_element_get_static_pad(self->m_payloader.get(), "src"));
            gst_pad_add_probe(srcPad.get(), static_cast<GstPadProbeType>(GST_PAD_PROBE_TYPE_BLOCK | GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM), reinterpret_cast<GstPadProbeCallback>(+[](GstPad* pad, GstPadProbeInfo* info, gpointer userData) -> GstPadProbeReturn {
                if (GST_EVENT_TYPE(GST_PAD_PROBE_INFO_DATA(info)) != GST_EVENT_EOS)
                    return GST_PAD_PROBE_OK;

                gst_pad_remove_probe(pad, GST_PAD_PROBE_INFO_ID(info));

                auto self = reinterpret_cast<RealtimeOutgoingMediaSourceGStreamer*>(userData);
                self->unlinkPayloader();
                self->m_padBlockedProbe = 0;
                self->codecPreferencesChanged();
                return GST_PAD_PROBE_DROP;
            }), userData, nullptr);

            auto head = self->m_encoder.get();
            if (self->m_type == RealtimeOutgoingMediaSourceGStreamer::Type::Video)
                head = self->m_payloader.get();
            auto sinkPad = adoptGRef(gst_element_get_static_pad(head, "sink"));
            gst_pad_send_event(sinkPad.get(), gst_event_new_eos());

            return GST_PAD_PROBE_OK;
        }), this, nullptr);
        return;
    }

    if (!setPayloadType(codecPreferences)) {
        GST_ERROR_OBJECT(m_bin.get(), "Unable to link encoder to webrtcbin");
        return;
    }

    gst_bin_sync_children_states(GST_BIN_CAST(m_bin.get()));
    gst_element_sync_state_with_parent(m_bin.get());
    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN_CAST(m_bin.get()), GST_DEBUG_GRAPH_SHOW_ALL, "outgoing-media-new-codec-prefs");
    m_isStopped = false;
}

void RealtimeOutgoingMediaSourceGStreamer::replaceTrack(RefPtr<MediaStreamTrackPrivate>&& newTrack)
{
    m_track->removeObserver(*this);
    webkitMediaStreamSrcReplaceTrack(WEBKIT_MEDIA_STREAM_SRC(m_outgoingSource.get()), RefPtr(newTrack));
    m_track = WTFMove(newTrack);
    m_track->addObserver(*this);
    flush();
    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN_CAST(m_bin.get()), GST_DEBUG_GRAPH_SHOW_ALL, "outgoing-media-replaced-track");
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
