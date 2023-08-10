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

GST_DEBUG_CATEGORY(webkit_webrtc_outgoing_media_debug);
#define GST_CAT_DEFAULT webkit_webrtc_outgoing_media_debug

namespace WebCore {

RealtimeOutgoingMediaSourceGStreamer::RealtimeOutgoingMediaSourceGStreamer(const RefPtr<UniqueSSRCGenerator>& ssrcGenerator, const String& mediaStreamId, MediaStreamTrack& track)
    : m_mediaStreamId(mediaStreamId)
    , m_trackId(track.id())
    , m_ssrcGenerator(ssrcGenerator)
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtc_outgoing_media_debug, "webkitwebrtcoutgoingmedia", 0, "WebKit WebRTC outgoing media");
    });

    m_bin = gst_bin_new(nullptr);

    m_inputSelector = gst_element_factory_make("input-selector", nullptr);
    gst_util_set_object_arg(G_OBJECT(m_inputSelector.get()), "sync-mode", "clock");

    m_preEncoderQueue = gst_element_factory_make("queue", nullptr);
    m_postEncoderQueue = gst_element_factory_make("queue", nullptr);
    m_capsFilter = gst_element_factory_make("capsfilter", nullptr);

    gst_bin_add_many(GST_BIN_CAST(m_bin.get()), m_inputSelector.get(), m_preEncoderQueue.get(), m_postEncoderQueue.get(), m_capsFilter.get(), nullptr);

    auto srcPad = adoptGRef(gst_element_get_static_pad(m_capsFilter.get(), "src"));
    gst_element_add_pad(m_bin.get(), gst_ghost_pad_new("src", srcPad.get()));

    setSource(track.privateTrack());
}

RealtimeOutgoingMediaSourceGStreamer::~RealtimeOutgoingMediaSourceGStreamer()
{
    teardown();

    if (m_transceiver)
        g_signal_handlers_disconnect_by_data(m_transceiver.get(), this);

    if (m_fallbackSource) {
        gst_element_set_locked_state(m_fallbackSource.get(), TRUE);
        gst_element_set_state(m_fallbackSource.get(), GST_STATE_READY);
        gst_element_unlink(m_fallbackSource.get(), m_inputSelector.get());
        gst_element_set_state(m_fallbackSource.get(), GST_STATE_NULL);
        gst_element_release_request_pad(m_inputSelector.get(), m_fallbackPad.get());
        gst_element_set_locked_state(m_fallbackSource.get(), FALSE);
    }

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

void RealtimeOutgoingMediaSourceGStreamer::setSource(Ref<MediaStreamTrackPrivate>&& newSource)
{
    if (m_source && !m_initialSettings)
        m_initialSettings = m_source.value()->settings();

    GST_DEBUG_OBJECT(m_bin.get(), "Setting source to %s", newSource->id().utf8().data());

    if (m_source.has_value())
        stopOutgoingSource();

    m_source = WTFMove(newSource);
    initializeFromTrack();
}

void RealtimeOutgoingMediaSourceGStreamer::start()
{
    if (!m_isStopped) {
        GST_DEBUG_OBJECT(m_bin.get(), "Source already started");
        return;
    }

    GST_DEBUG_OBJECT(m_bin.get(), "Starting outgoing source");
    m_source.value()->addObserver(*this);
    m_isStopped = false;

    if (m_transceiver) {
        auto selectorSrcPad = adoptGRef(gst_element_get_static_pad(m_inputSelector.get(), "src"));
        if (!gst_pad_is_linked(selectorSrcPad.get())) {
            GST_DEBUG_OBJECT(m_bin.get(), "Codec preferences haven't changed before startup, ensuring source is linked");
            GRefPtr<GstCaps> codecPreferences;
            g_object_get(m_transceiver.get(), "codec-preferences", &codecPreferences.outPtr(), nullptr);
            callOnMainThreadAndWait([&] {
                codecPreferencesChanged(codecPreferences);
            });
        }
    }

    linkOutgoingSource();
    gst_element_sync_state_with_parent(m_bin.get());
}

void RealtimeOutgoingMediaSourceGStreamer::stop()
{
    GST_DEBUG_OBJECT(m_bin.get(), "Stopping outgoing source");
    m_isStopped = true;
    if (!m_source)
        return;

    connectFallbackSource();

    stopOutgoingSource();
    m_source.reset();
}

void RealtimeOutgoingMediaSourceGStreamer::flush()
{
    gst_element_send_event(m_outgoingSource.get(), gst_event_new_flush_start());
    gst_element_send_event(m_outgoingSource.get(), gst_event_new_flush_stop(FALSE));
}

void RealtimeOutgoingMediaSourceGStreamer::stopOutgoingSource()
{
    if (!m_source)
        return;

    GST_DEBUG_OBJECT(m_bin.get(), "Stopping outgoing source %" GST_PTR_FORMAT, m_outgoingSource.get());
    m_source.value()->removeObserver(*this);
    webkitMediaStreamSrcSignalEndOfStream(WEBKIT_MEDIA_STREAM_SRC(m_outgoingSource.get()));

    gst_element_set_locked_state(m_outgoingSource.get(), TRUE);

    unlinkOutgoingSource();

    gst_element_set_state(m_outgoingSource.get(), GST_STATE_NULL);
    gst_bin_remove(GST_BIN_CAST(m_bin.get()), m_outgoingSource.get());
    gst_element_set_locked_state(m_outgoingSource.get(), FALSE);
    m_outgoingSource.clear();
}

void RealtimeOutgoingMediaSourceGStreamer::sourceMutedChanged()
{
    if (!m_source)
        return;
    ASSERT(m_muted != m_source.value()->muted());
    m_muted = m_source.value()->muted();
    GST_DEBUG_OBJECT(m_bin.get(), "Mute state changed to %s", boolForPrinting(m_muted));
}

void RealtimeOutgoingMediaSourceGStreamer::sourceEnabledChanged()
{
    if (!m_source)
        return;

    m_enabled = m_source.value()->enabled();
    GST_DEBUG_OBJECT(m_bin.get(), "Enabled state changed to %s", boolForPrinting(m_enabled));
}

void RealtimeOutgoingMediaSourceGStreamer::initializeFromTrack()
{
    m_muted = m_source.value()->muted();
    m_enabled = m_source.value()->enabled();
    GST_DEBUG_OBJECT(m_bin.get(), "Initializing from track, muted: %s, enabled: %s", boolForPrinting(m_muted), boolForPrinting(m_enabled));

    if (m_outgoingSource)
        return;

    m_outgoingSource = webkitMediaStreamSrcNew();
    GST_DEBUG_OBJECT(m_bin.get(), "Created outgoing source %" GST_PTR_FORMAT, m_outgoingSource.get());
    gst_bin_add(GST_BIN_CAST(m_bin.get()), m_outgoingSource.get());
    webkitMediaStreamSrcAddTrack(WEBKIT_MEDIA_STREAM_SRC(m_outgoingSource.get()), m_source->ptr(), true);
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
    g_signal_connect_swapped(m_transceiver.get(), "notify::codec-preferences", G_CALLBACK(+[](RealtimeOutgoingMediaSourceGStreamer* source, GParamSpec*, GstWebRTCRTPTransceiver* transceiver) {
        GRefPtr<GstCaps> codecPreferences;
        g_object_get(transceiver, "codec-preferences", &codecPreferences.outPtr(), nullptr);
        callOnMainThreadAndWait([&] {
            source->codecPreferencesChanged(codecPreferences);
        });
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

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
