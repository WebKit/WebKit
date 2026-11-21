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

#if ENABLE(VIRTUAL_AUDIO_DEVICES)
#include "GStreamerVirtualAudioDeviceManager.h"
#endif

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

// Virtual audio devices require the mixer to output buffers of a fixed number of frames, at their
// own rate. Returns the rate and the number of frames per buffer.
static std::optional<std::pair<int, int>> deviceOutputBufferConfiguration(const GRefPtr<GstDevice>& device)
{
#if ENABLE(VIRTUAL_AUDIO_DEVICES)
    if (auto* configuration = GStreamerVirtualAudioDeviceManager::configuration(device.get()))
        return std::pair { static_cast<int>(configuration->sampleRate), static_cast<int>(configuration->framesPerBuffer) };
#else
    UNUSED_PARAM(device);
#endif
    return std::nullopt;
}

static bool configureOutputBuffers(GstBin* pipeline, GstElement* mixer, GstElement* audioSink, const GRefPtr<GstDevice>& device)
{
    auto configuration = deviceOutputBufferConfiguration(device);
    if (!configuration)
        return false;

    auto [rate, framesPerBuffer] = *configuration;

    // Make the mixer output the device rate, the buffers would no longer have the requested
    // number of frames if they were resampled afterwards.
    auto* capsFilter = makeGStreamerElement("capsfilter"_s);
    if (!capsFilter) {
        GST_ERROR_OBJECT(pipeline, "Failed to create the capsfilter of the mixer output");
        return false;
    }
    GRefPtr caps = adoptGRef(gst_caps_new_simple("audio/x-raw", "rate", G_TYPE_INT, rate, nullptr));
    g_object_set(capsFilter, "caps", caps.get(), nullptr);
    gst_bin_add(pipeline, capsFilter);
    if (!gst_element_link_many(mixer, capsFilter, audioSink, nullptr)) {
        GST_ERROR_OBJECT(pipeline, "Failed to link the mixer to the device sink through %" GST_PTR_FORMAT, capsFilter);
        gst_element_unlink(mixer, capsFilter);
        gst_element_set_state(capsFilter, GST_STATE_NULL);
        gst_bin_remove(pipeline, capsFilter);
        return false;
    }

    // Only configured once linked: when linking fails, the mixer is linked directly to the sink
    // and keeps its default output buffers.
    g_object_set(mixer, "output-buffer-duration-fraction", framesPerBuffer, rate, nullptr);
    GST_DEBUG_OBJECT(pipeline, "Mixer configured to output buffers of %d frames at %d Hz", framesPerBuffer, rate);
    return true;
}

GStreamerAudioMixer::MixerPipeline& GStreamerAudioMixer::ensureMixerPipeline(DataMutexLocker<StreamingMembers>& locker, const String& deviceId, const GRefPtr<GstDevice>& device)
{
    auto result = locker->m_pipelines.find(deviceId);
    if (result != locker->m_pipelines.end()) {
        auto& mp = *result->value;
        // The GstDevice of a device ID can be replaced, for instance when a device is unplugged
        // and plugged again: the sink of an idle pipeline would render on the previous one.
        if (!device || mp.device == device || mp.mixer->numsinkpads) {
            if (device && mp.device != device)
                GST_WARNING_OBJECT(mp.pipeline.get(), "Device %" GST_PTR_FORMAT " changed while the pipeline is in use, keeping %" GST_PTR_FORMAT, device.get(), mp.device.get());
            // Cancel any pending teardown since a new producer is registering.
            mp.teardownTimer.stop();
            return mp;
        }

        GST_DEBUG_OBJECT(mp.pipeline.get(), "Device of '%s' changed, recreating the idle pipeline.", deviceId.utf8());
        destroyPipeline(locker, deviceId);
    }

    auto pipelineName = makeString("webkitaudiomixer-"_s, deviceId);
    GRefPtr<GstElement> pipeline = gst_element_factory_make("pipeline", pipelineName.utf8().legacyCStringPointer());
    registerActivePipeline(pipeline);
    connectSimpleBusMessageCallback(pipeline.get());

    auto* mixer = makeGStreamerElement("audiomixer"_s);

    // Use a device-specific sink if a GstDevice is provided, otherwise fall back to autoaudiosink.
    GstElement* audioSink;
    if (device) {
        audioSink = gst_device_create_element(device.get(), "audio-output-sink");
        if (!audioSink) {
            // Don't fall back to the default output: the audio of a device (for instance a virtual
            // device implemented by the application) must not be rendered on another device.
            GST_ERROR_OBJECT(pipeline.get(), "Failed to create the sink of device %" GST_PTR_FORMAT ", the audio rendered on it is discarded", device.get());
            audioSink = makeGStreamerElement("fakesink"_s, "audio-output-sink"_s);
            RELEASE_ASSERT(audioSink);
            g_object_set(audioSink, "sync", TRUE, nullptr);
        }
    } else
        audioSink = createAutoAudioSink({ });

    gst_bin_add_many(GST_BIN_CAST(pipeline.get()), mixer, audioSink, nullptr);
    if (!configureOutputBuffers(GST_BIN_CAST(pipeline.get()), mixer, audioSink, device) && !gst_element_link(mixer, audioSink))
        GST_ERROR_OBJECT(pipeline.get(), "Failed to link the mixer to %" GST_PTR_FORMAT ", the audio rendered on it is discarded", audioSink);
    gst_element_set_state(pipeline.get(), GST_STATE_READY);

    GST_DEBUG_OBJECT(pipeline.get(), "Created mixer pipeline for device '%s'.", deviceId.utf8());
    auto mp = std::unique_ptr<MixerPipeline>(new MixerPipeline {
        WTF::move(pipeline),
        GRefPtr<GstElement>(mixer),
        RunLoop::Timer(RunLoop::mainSingleton(), "GStreamerAudioMixer::TeardownTimer"_s, [deviceId] { GStreamerAudioMixer::singleton().teardownPipeline(deviceId); }),
        SharedLatency::create(0),
        0,
        device
    });

    // The pipeline sends the latency it configures upstream from its sinks, keep it for the
    // producers.
    GRefPtr mixerSrcPad = adoptGRef(gst_element_get_static_pad(mixer, "src"));
    mp->latencyProbeId = gst_pad_add_probe(mixerSrcPad.get(), GST_PAD_PROBE_TYPE_EVENT_UPSTREAM, [](GstPad*, GstPadProbeInfo* info, gpointer userData) -> GstPadProbeReturn {
        auto* event = GST_PAD_PROBE_INFO_EVENT(info);
        if (GST_EVENT_TYPE(event) == GST_EVENT_LATENCY) {
            GstClockTime latency;
            gst_event_parse_latency(event, &latency);
            static_cast<MixerPipeline*>(userData)->latency->store(GST_CLOCK_TIME_IS_VALID(latency) ? latency : 0);
        }
        return GST_PAD_PROBE_OK;
    }, mp.get(), nullptr);

    auto addResult = locker->m_pipelines.set(deviceId, WTF::move(mp));
    return *addResult.iterator->value;
}

void GStreamerAudioMixer::teardownPipeline(const String& deviceId)
{
    DataMutexLocker locker { m_streamingMembers };
    GST_DEBUG("Teardown timeout reached for device '%s'.", deviceId.utf8());
    destroyPipeline(locker, deviceId);
}

void GStreamerAudioMixer::destroyPipeline(DataMutexLocker<StreamingMembers>& locker, const String& deviceId)
{
    auto it = locker->m_pipelines.find(deviceId);
    RELEASE_ASSERT(it != locker->m_pipelines.end());

    auto& mp = *it->value;
    ASSERT(!mp.mixer->numsinkpads);
    GST_DEBUG_OBJECT(mp.pipeline.get(), "Destroying idle pipeline for device '%s'.", deviceId.utf8());
    unregisterPipeline(mp.pipeline);
    gst_element_set_state(mp.pipeline.get(), GST_STATE_NULL);
    GRefPtr mixerSrcPad = adoptGRef(gst_element_get_static_pad(mp.mixer.get(), "src"));
    gst_pad_remove_probe(mixerSrcPad.get(), mp.latencyProbeId);
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

GStreamerAudioMixer::Producer GStreamerAudioMixer::registerProducer(GstElement* interaudioSink, std::optional<int> forcedSampleRate, std::optional<GstClockTime> periodTime, const String& deviceId, const GRefPtr<GstDevice>& device)
{
    auto resolvedDeviceId = deviceId.isEmpty() ? "default"_s : deviceId;

    // The mixer of a device requiring a fixed buffer size runs at the device rate.
    if (auto configuration = deviceOutputBufferConfiguration(device))
        forcedSampleRate = configuration->first;

    DataMutexLocker locker { m_streamingMembers };
    auto& mp = ensureMixerPipeline(locker, resolvedDeviceId, device);

    GstElement* src = makeGStreamerElement("interaudiosrc"_s, String::fromLatin1(unsafeSpan(GST_ELEMENT_NAME(interaudioSink))));

    g_object_set(src, "channel", GST_ELEMENT_NAME(interaudioSink), nullptr);
    if (periodTime)
        g_object_set(src, "period-time", static_cast<guint64>(*periodTime), nullptr);
    g_object_set(interaudioSink, "channel", GST_ELEMENT_NAME(interaudioSink), nullptr);

    auto bin = gst_bin_new(nullptr);
    auto audioResample = makeGStreamerElement("audioresample"_s);
    auto audioConvert = makeGStreamerElement("audioconvert"_s);
    gst_bin_add_many(GST_BIN_CAST(bin), audioResample, audioConvert, nullptr);
    gst_element_link(audioConvert, audioResample);

    if (forcedSampleRate) {
        auto capsfilter = gst_element_factory_make("capsfilter", nullptr);
        GRefPtr caps = adoptGRef(gst_caps_new_simple("audio/x-raw", "rate", G_TYPE_INT, *forcedSampleRate, nullptr));
        g_object_set(capsfilter, "caps", caps.get(), nullptr);
        gst_bin_add(GST_BIN_CAST(bin), capsfilter);
        gst_element_link(audioResample, capsfilter);
    }

    if (GRefPtr pad = adoptGRef(gst_bin_find_unlinked_pad(GST_BIN_CAST(bin), GST_PAD_SRC)))
        gst_element_add_pad(GST_ELEMENT_CAST(bin), gst_ghost_pad_new("src", pad.get()));
    if (GRefPtr pad = adoptGRef(gst_bin_find_unlinked_pad(GST_BIN_CAST(bin), GST_PAD_SINK)))
        gst_element_add_pad(GST_ELEMENT_CAST(bin), gst_ghost_pad_new("sink", pad.get()));

    gst_bin_add_many(GST_BIN_CAST(mp.pipeline.get()), src, bin, nullptr);
    gst_element_link(src, bin);

    bool shouldStart = !mp.mixer->numsinkpads;

    GRefPtr mixerPad = adoptGRef(gst_element_request_pad_simple(mp.mixer.get(), "sink_%u"));
    GRefPtr srcPad = adoptGRef(gst_element_get_static_pad(bin, "src"));
    gst_pad_link(srcPad.get(), mixerPad.get());

    if (shouldStart)
        gst_element_set_state(mp.pipeline.get(), GST_STATE_READY);
    else
        gst_bin_sync_children_states(GST_BIN_CAST(mp.pipeline.get()));

    locker->m_padToDeviceId.set(mixerPad.get(), resolvedDeviceId);

    GST_DEBUG_OBJECT(mp.pipeline.get(), "Registered audio producer %" GST_PTR_FORMAT, mixerPad.get());
    dumpBinToDotFile(mp.pipeline.get(), "audio-mixer-after-producer-registration"_s);
    return { WTF::move(mixerPad), mp.latency };
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

    GST_DEBUG_OBJECT(mp.pipeline.get(), "Unregistering audio producer %" GST_PTR_FORMAT " from device '%s'.", mixerPad.get(), deviceId.utf8());

    GRefPtr peer = adoptGRef(gst_pad_get_peer(mixerPad.get()));
    GRefPtr bin = adoptGRef(gst_pad_get_parent_element(peer.get()));
    GRefPtr sinkPad = adoptGRef(gst_element_get_static_pad(bin.get(), "sink"));
    GRefPtr srcPad = adoptGRef(gst_pad_get_peer(sinkPad.get()));
    GRefPtr interaudioSrc = adoptGRef(gst_pad_get_parent_element(srcPad.get()));
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
        GST_DEBUG_OBJECT(mp.pipeline.get(), "No producers left for device '%s', paused pipeline; teardown in %.0f seconds.", deviceId.utf8(), s_teardownTimeout.seconds());
        dumpBinToDotFile(mp.pipeline.get(), "audio-mixer-after-producer-unregistration"_s);
    }
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif
