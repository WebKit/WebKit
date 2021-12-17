/*
 * Copyright (C) 2020 Igalia S.L
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "GStreamerAudioMixer.h"

#if USE(GSTREAMER)

#include "GStreamerCommon.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

GST_DEBUG_CATEGORY_STATIC(webkit_media_gst_audio_mixer_debug);
#define GST_CAT_DEFAULT webkit_media_gst_audio_mixer_debug

bool GStreamerAudioMixer::isAvailable()
{
    return webkitGstCheckVersion(1, 18, 0) && isGStreamerPluginAvailable("inter") && isGStreamerPluginAvailable("audiomixer");
}

GStreamerAudioMixer& GStreamerAudioMixer::singleton()
{
    static NeverDestroyed<GStreamerAudioMixer> sharedInstance;
    return sharedInstance;
}

GStreamerAudioMixer::GStreamerAudioMixer()
{
    GST_DEBUG_CATEGORY_INIT(webkit_media_gst_audio_mixer_debug, "webkitaudiomixer", 0, "WebKit GStreamer audio mixer");
    m_pipeline = gst_element_factory_make("pipeline", "webkitaudiomixer");
    connectSimpleBusMessageCallback(m_pipeline.get());

    m_mixer = makeGStreamerElement("audiomixer", nullptr);
    auto* audioSink = createAutoAudioSink({ });

    gst_bin_add_many(GST_BIN_CAST(m_pipeline.get()), m_mixer.get(), audioSink, nullptr);
    gst_element_link(m_mixer.get(), audioSink);
    gst_element_set_state(m_pipeline.get(), GST_STATE_READY);
}

void GStreamerAudioMixer::ensureState(GstStateChange stateChange)
{
    GST_DEBUG_OBJECT(m_pipeline.get(), "Handling %s transition (%u mixer pads)", gst_state_change_get_name(stateChange), m_mixer->numsinkpads);

    switch (stateChange) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
        gst_element_set_state(m_pipeline.get(), GST_STATE_PAUSED);
        break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
        gst_element_set_state(m_pipeline.get(), GST_STATE_PLAYING);
        break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
        if (m_mixer->numsinkpads == 1)
            gst_element_set_state(m_pipeline.get(), GST_STATE_PAUSED);
        break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
        if (m_mixer->numsinkpads == 1)
            gst_element_set_state(m_pipeline.get(), GST_STATE_READY);
        break;
    case GST_STATE_CHANGE_READY_TO_NULL:
        if (m_mixer->numsinkpads == 1)
            gst_element_set_state(m_pipeline.get(), GST_STATE_NULL);
        break;
    default:
        break;
    }
}

GRefPtr<GstPad> GStreamerAudioMixer::registerProducer(GstElement* interaudioSink)
{
    GstElement* src = makeGStreamerElement("interaudiosrc", nullptr);
    g_object_set(src, "channel", GST_ELEMENT_NAME(interaudioSink), nullptr);
    g_object_set(interaudioSink, "channel", GST_ELEMENT_NAME(interaudioSink), nullptr);

    GstElement* audioResample = makeGStreamerElement("audioresample", nullptr);
    gst_bin_add_many(GST_BIN_CAST(m_pipeline.get()), src, audioResample, nullptr);
    gst_element_link(src, audioResample);

    bool shouldStart = !m_mixer->numsinkpads;

    auto mixerPad = adoptGRef(gst_element_request_pad_simple(m_mixer.get(), "sink_%u"));
    auto srcPad = adoptGRef(gst_element_get_static_pad(audioResample, "src"));
    gst_pad_link(srcPad.get(), mixerPad.get());

    if (shouldStart)
        gst_element_set_state(m_pipeline.get(), GST_STATE_READY);
    else {
        gst_element_sync_state_with_parent(src);
        gst_element_sync_state_with_parent(audioResample);
    }

    GST_DEBUG_OBJECT(m_pipeline.get(), "Registered audio producer %" GST_PTR_FORMAT, mixerPad.get());
    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN_CAST(m_pipeline.get()), GST_DEBUG_GRAPH_SHOW_ALL, "audio-mixer-after-producer-registration");
    return mixerPad;
}

void GStreamerAudioMixer::unregisterProducer(const GRefPtr<GstPad>& mixerPad)
{
    GST_DEBUG_OBJECT(m_pipeline.get(), "Unregistering audio producer %" GST_PTR_FORMAT, mixerPad.get());

    auto peer = adoptGRef(gst_pad_get_peer(mixerPad.get()));
    auto audioResample = adoptGRef(gst_pad_get_parent_element(peer.get()));
    auto resamplePeerPad = adoptGRef(gst_element_get_static_pad(audioResample.get(), "sink"));
    auto resamplePeer = adoptGRef(gst_pad_get_peer(resamplePeerPad.get()));
    auto interaudioSrc = adoptGRef(gst_pad_get_parent_element(resamplePeer.get()));
    GST_LOG_OBJECT(m_pipeline.get(), "interaudiosrc: %" GST_PTR_FORMAT, interaudioSrc.get());

    gst_element_set_locked_state(interaudioSrc.get(), true);
    gst_element_set_state(interaudioSrc.get(), GST_STATE_NULL);
    gst_element_set_state(audioResample.get(), GST_STATE_NULL);

    gst_pad_unlink(peer.get(), mixerPad.get());
    gst_element_unlink(interaudioSrc.get(), audioResample.get());

    gst_element_release_request_pad(m_mixer.get(), mixerPad.get());

    gst_bin_remove_many(GST_BIN_CAST(m_pipeline.get()), interaudioSrc.get(), audioResample.get(), nullptr);

    if (!m_mixer->numsinkpads)
        gst_element_set_state(m_pipeline.get(), GST_STATE_NULL);

    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN_CAST(m_pipeline.get()), GST_DEBUG_GRAPH_SHOW_ALL, "audio-mixer-after-producer-unregistration");
}

}
#endif
