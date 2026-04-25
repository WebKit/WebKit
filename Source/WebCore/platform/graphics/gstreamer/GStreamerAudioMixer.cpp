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
}

GStreamerAudioMixer::MixerPipeline& GStreamerAudioMixer::ensureMixerPipeline(DataMutexLocker<StreamingMembers>& locker, const String& deviceId, const GRefPtr<GstDevice>& device)
{
    auto result = locker->m_pipelines.find(deviceId);
    if (result != locker->m_pipelines.end()) {
        // Cancel any pending teardown since a new producer is registering.
        result->value->teardownTimer.stop();
        return *result->value;
    }

    auto pipelineName = makeString("webkitaudiomixer-"_s, deviceId);
    GRefPtr<GstElement> pipeline = gst_element_factory_make("pipeline", pipelineName.utf8().data());
    registerActivePipeline(pipeline);
    connectSimpleBusMessageCallback(pipeline.get());

    auto* mixer = makeGStreamerElement("audiomixer"_s);

    // Use a device-specific sink if a GstDevice is provided, otherwise fall back to autoaudiosink.
    GstElement* audioSink;
    if (device)
        audioSink = gst_device_create_element(device.get(), "audio-output-sink");
    else
        audioSink = createAutoAudioSink({ });

    gst_bin_add_many(GST_BIN_CAST(pipeline.get()), mixer, audioSink, nullptr);
    gst_element_link(mixer, audioSink);
    gst_element_set_state(pipeline.get(), GST_STATE_READY);

    GST_DEBUG_OBJECT(pipeline.get(), "Created mixer pipeline for device '%s'.", deviceId.utf8().data());
    auto mp = std::unique_ptr<MixerPipeline>(new MixerPipeline {
        WTF::move(pipeline),
        GRefPtr<GstElement>(mixer),
        RunLoop::Timer(RunLoop::mainSingleton(), "GStreamerAudioMixer::TeardownTimer"_s, [deviceId] { GStreamerAudioMixer::singleton().teardownPipeline(deviceId); })
    });
    auto addResult = locker->m_pipelines.set(deviceId, WTF::move(mp));
    return *addResult.iterator->value;
}

void GStreamerAudioMixer::teardownPipeline(const String& deviceId)
{
    DataMutexLocker locker { m_streamingMembers };
    auto it = locker->m_pipelines.find(deviceId);
    RELEASE_ASSERT(it != locker->m_pipelines.end());

    auto& mp = *it->value;
    ASSERT(!mp.mixer->numsinkpads);
    GST_DEBUG_OBJECT(mp.pipeline.get(), "Teardown timeout reached, destroying idle pipeline for device '%s'.", deviceId.utf8().data());
    unregisterPipeline(mp.pipeline);
    gst_element_set_state(mp.pipeline.get(), GST_STATE_NULL);
    locker->m_pipelines.remove(deviceId);
}

void GStreamerAudioMixer::ensureState(GstStateChange stateChange, const String& deviceId)
{
    auto resolvedDeviceId = deviceId.isEmpty() ? "default"_s : deviceId;
    DataMutexLocker locker { m_streamingMembers };
    auto it = locker->m_pipelines.find(resolvedDeviceId);
    RELEASE_ASSERT(it != locker->m_pipelines.end());
    auto& mp = *it->value;

    GST_DEBUG_OBJECT(mp.pipeline.get(), "Handling %s transition (%u mixer pads).", gst_state_change_get_name(stateChange), mp.mixer->numsinkpads);

    switch (stateChange) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
        gst_element_set_state(mp.pipeline.get(), GST_STATE_PAUSED);
        break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
        gst_element_set_state(mp.pipeline.get(), GST_STATE_PLAYING);
        break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
        if (mp.mixer->numsinkpads == 1)
            gst_element_set_state(mp.pipeline.get(), GST_STATE_PAUSED);
        break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
        if (mp.mixer->numsinkpads == 1)
            gst_element_set_state(mp.pipeline.get(), GST_STATE_READY);
        break;
    case GST_STATE_CHANGE_READY_TO_NULL:
        if (mp.mixer->numsinkpads == 1)
            mp.teardownTimer.startOneShot(s_teardownTimeout);
        break;
    default:
        break;
    }
}

GRefPtr<GstPad> GStreamerAudioMixer::registerProducer(GstElement* interaudioSink, std::optional<int> forcedSampleRate, const String& deviceId, const GRefPtr<GstDevice>& device)
{
    auto resolvedDeviceId = deviceId.isEmpty() ? "default"_s : deviceId;
    DataMutexLocker locker { m_streamingMembers };
    auto& mp = ensureMixerPipeline(locker, resolvedDeviceId, device);

    GstElement* src = makeGStreamerElement("interaudiosrc"_s, unsafeSpan(GST_ELEMENT_NAME(interaudioSink)));

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

    gst_bin_add_many(GST_BIN_CAST(mp.pipeline.get()), src, bin, nullptr);
    gst_element_link(src, bin);

    bool shouldStart = !mp.mixer->numsinkpads;

    auto mixerPad = adoptGRef(gst_element_request_pad_simple(mp.mixer.get(), "sink_%u"));
    auto srcPad = adoptGRef(gst_element_get_static_pad(bin, "src"));
    gst_pad_link(srcPad.get(), mixerPad.get());

    if (shouldStart)
        gst_element_set_state(mp.pipeline.get(), GST_STATE_READY);
    else
        gst_bin_sync_children_states(GST_BIN_CAST(mp.pipeline.get()));

    locker->m_padToDeviceId.set(mixerPad.get(), resolvedDeviceId);

    GST_DEBUG_OBJECT(mp.pipeline.get(), "Registered audio producer %" GST_PTR_FORMAT, mixerPad.get());
    dumpBinToDotFile(mp.pipeline.get(), "audio-mixer-after-producer-registration"_s);
    return mixerPad;
}

void GStreamerAudioMixer::unregisterProducer(const GRefPtr<GstPad>& mixerPad)
{
    DataMutexLocker locker { m_streamingMembers };
    auto it = locker->m_padToDeviceId.find(mixerPad.get());
    RELEASE_ASSERT(it != locker->m_padToDeviceId.end());
    auto deviceId = it->value;

    auto pipelineIt = locker->m_pipelines.find(deviceId);
    RELEASE_ASSERT(pipelineIt != locker->m_pipelines.end());
    auto& mp = *pipelineIt->value;

    GST_DEBUG_OBJECT(mp.pipeline.get(), "Unregistering audio producer %" GST_PTR_FORMAT " from device '%s'.", mixerPad.get(), deviceId.utf8().data());

    auto peer = adoptGRef(gst_pad_get_peer(mixerPad.get()));
    auto bin = adoptGRef(gst_pad_get_parent_element(peer.get()));
    auto sinkPad = adoptGRef(gst_element_get_static_pad(bin.get(), "sink"));
    auto srcPad = adoptGRef(gst_pad_get_peer(sinkPad.get()));
    auto interaudioSrc = adoptGRef(gst_pad_get_parent_element(srcPad.get()));
    GST_LOG_OBJECT(mp.pipeline.get(), "interaudiosrc: %" GST_PTR_FORMAT, interaudioSrc.get());

    gstElementLockAndSetState(interaudioSrc.get(), GST_STATE_NULL);
    gstElementLockAndSetState(bin.get(), GST_STATE_NULL);
    gst_pad_unlink(peer.get(), mixerPad.get());
    gst_element_unlink(interaudioSrc.get(), bin.get());

    gst_element_release_request_pad(mp.mixer.get(), mixerPad.get());

    gst_bin_remove_many(GST_BIN_CAST(mp.pipeline.get()), interaudioSrc.get(), bin.get(), nullptr);

    locker->m_padToDeviceId.remove(mixerPad.get());

    // When no more producers remain, move to PAUSED and schedule deferred teardown.
    if (!mp.mixer->numsinkpads) {
        gst_element_set_state(mp.pipeline.get(), GST_STATE_PAUSED);
        mp.teardownTimer.startOneShot(s_teardownTimeout);
        GST_DEBUG_OBJECT(mp.pipeline.get(), "No producers left for device '%s', paused pipeline; teardown in %.0f seconds.", deviceId.utf8().data(), s_teardownTimeout.seconds());
        dumpBinToDotFile(mp.pipeline.get(), "audio-mixer-after-producer-unregistration"_s);
    }
}

void GStreamerAudioMixer::configureSourcePeriodTime(CStringView sourceName, uint64_t periodTime)
{
    DataMutexLocker locker { m_streamingMembers };
    for (auto& [deviceId, mp] : locker->m_pipelines) {
        auto src = adoptGRef(gst_bin_get_by_name(GST_BIN_CAST(mp->pipeline.get()), sourceName.utf8()));
        if (!src)
            continue;

        g_object_set(src.get(), "period-time", periodTime, nullptr);
        return;
    }
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif
