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
    return isGStreamerPluginAvailable("inter"_s) && isGStreamerPluginAvailable("audiomixer"_s);
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
    registerActivePipeline(m_pipeline);
    connectSimpleBusMessageCallback(m_pipeline.get());

    m_mixer = makeGStreamerElement("audiomixer"_s);
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
        if (m_mixer->numsinkpads == 1) {
            unregisterPipeline(m_pipeline);
            gst_element_set_state(m_pipeline.get(), GST_STATE_NULL);
        }
        break;
    default:
        break;
    }
}

GRefPtr<GstPad> GStreamerAudioMixer::registerProducer(GstElement* interaudioSink, std::optional<int> forcedSampleRate)
{
    auto name = StringView::fromLatin1(GST_ELEMENT_NAME(interaudioSink));
    GstElement* src = makeGStreamerElement("interaudiosrc"_s, name.toStringWithoutCopying());

    g_object_set(src, "channel", GST_ELEMENT_NAME(interaudioSink), nullptr);
    g_object_set(interaudioSink, "channel", GST_ELEMENT_NAME(interaudioSink), nullptr);

    auto bin = gst_bin_new(nullptr);
    auto audioResample = makeGStreamerElement("audioresample"_s);
    auto audioConvert = makeGStreamerElement("audioconvert"_s);
    gst_bin_add_many(GST_BIN_CAST(bin), audioResample, audioConvert, nullptr);
    gst_element_link(audioConvert, audioResample);

    if (forcedSampleRate) {
        auto capsfilter = gst_element_factory_make("capsfilter", nullptr);
        auto caps = adoptGRef(gst_caps_new_simple("audio/x-raw", "rate", G_TYPE_INT, *forcedSampleRate, nullptr));
        g_object_set(capsfilter, "caps", caps.get(), nullptr);
        gst_bin_add(GST_BIN_CAST(bin), capsfilter);
        gst_element_link(audioResample, capsfilter);
    }

    if (auto pad = adoptGRef(gst_bin_find_unlinked_pad(GST_BIN_CAST(bin), GST_PAD_SRC)))
        gst_element_add_pad(GST_ELEMENT_CAST(bin), gst_ghost_pad_new("src", pad.get()));
    if (auto pad = adoptGRef(gst_bin_find_unlinked_pad(GST_BIN_CAST(bin), GST_PAD_SINK)))
        gst_element_add_pad(GST_ELEMENT_CAST(bin), gst_ghost_pad_new("sink", pad.get()));

    gst_bin_add_many(GST_BIN_CAST(m_pipeline.get()), src, bin, nullptr);
    gst_element_link(src, bin);

    bool shouldStart = !m_mixer->numsinkpads;

    auto mixerPad = adoptGRef(gst_element_request_pad_simple(m_mixer.get(), "sink_%u"));
    auto srcPad = adoptGRef(gst_element_get_static_pad(bin, "src"));
    gst_pad_link(srcPad.get(), mixerPad.get());

    if (shouldStart)
        gst_element_set_state(m_pipeline.get(), GST_STATE_READY);
    else
        gst_bin_sync_children_states(GST_BIN_CAST(m_pipeline.get()));

    GST_DEBUG_OBJECT(m_pipeline.get(), "Registered audio producer %" GST_PTR_FORMAT, mixerPad.get());
    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN_CAST(m_pipeline.get()), GST_DEBUG_GRAPH_SHOW_ALL, "audio-mixer-after-producer-registration");
    return mixerPad;
}

void GStreamerAudioMixer::unregisterProducer(const GRefPtr<GstPad>& mixerPad)
{
    GST_DEBUG_OBJECT(m_pipeline.get(), "Unregistering audio producer %" GST_PTR_FORMAT, mixerPad.get());

    auto peer = adoptGRef(gst_pad_get_peer(mixerPad.get()));
    auto bin = adoptGRef(gst_pad_get_parent_element(peer.get()));
    auto sinkPad = adoptGRef(gst_element_get_static_pad(bin.get(), "sink"));
    auto srcPad = adoptGRef(gst_pad_get_peer(sinkPad.get()));
    auto interaudioSrc = adoptGRef(gst_pad_get_parent_element(srcPad.get()));
    GST_LOG_OBJECT(m_pipeline.get(), "interaudiosrc: %" GST_PTR_FORMAT, interaudioSrc.get());

    gstElementLockAndSetState(interaudioSrc.get(), GST_STATE_NULL);
    gstElementLockAndSetState(bin.get(), GST_STATE_NULL);
    gst_pad_unlink(peer.get(), mixerPad.get());
    gst_element_unlink(interaudioSrc.get(), bin.get());

    gst_element_release_request_pad(m_mixer.get(), mixerPad.get());

    gst_bin_remove_many(GST_BIN_CAST(m_pipeline.get()), interaudioSrc.get(), bin.get(), nullptr);

    if (!m_mixer->numsinkpads)
        gst_element_set_state(m_pipeline.get(), GST_STATE_NULL);

    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN_CAST(m_pipeline.get()), GST_DEBUG_GRAPH_SHOW_ALL, "audio-mixer-after-producer-unregistration");
}

void GStreamerAudioMixer::configureSourcePeriodTime(StringView sourceName, uint64_t periodTime)
{
    auto src = adoptGRef(gst_bin_get_by_name(GST_BIN_CAST(m_pipeline.get()), sourceName.toStringWithoutCopying().ascii().data()));
    if (!src) [[unlikely]]
        return;

    g_object_set(src.get(), "period-time", periodTime, nullptr);
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif
