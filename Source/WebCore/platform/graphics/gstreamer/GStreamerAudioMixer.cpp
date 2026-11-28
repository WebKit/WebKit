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
#include "VirtualAudioDevice.h"
#include <wtf/MainThread.h>
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
// own rate.
static const VirtualAudioDeviceConfiguration* virtualAudioDeviceConfiguration(const GRefPtr<GstDevice>& device)
{
#if ENABLE(VIRTUAL_AUDIO_DEVICES)
    return GStreamerVirtualAudioDeviceManager::configuration(device.get());
#else
    UNUSED_PARAM(device);
    return nullptr;
#endif
}

static GstElement* makeRateCapsFilter(int rate)
{
    auto* capsFilter = makeGStreamerElement("capsfilter"_s);
    GRefPtr caps = adoptGRef(gst_caps_new_simple("audio/x-raw", "rate", G_TYPE_INT, rate, nullptr));
    g_object_set(capsFilter, "caps", caps.get(), nullptr);
    return capsFilter;
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
            // The teardown timer isn't stopped here: this can run on a streaming thread, and the
            // timer belongs to the main thread. teardownPipeline() keeps the pipelines in use.
            return mp;
        }

        GST_DEBUG_OBJECT(mp.pipeline.get(), "Device of '%s' changed, recreating the idle pipeline.", makeUTF8CString(deviceId));
        // The MixerPipeline owns a main thread RunLoop::Timer, it must be destroyed there.
        callOnMainThread([idlePipeline = takeIdlePipeline(locker, deviceId)] { });
    }

    auto pipelineName = makeString("webkitaudiomixer-"_s, deviceId);
    GRefPtr<GstElement> pipeline = gst_element_factory_make("pipeline", makeUTF8CString(pipelineName).legacyCStringPointer());
    registerActivePipeline(pipeline);
    connectSimpleBusMessageCallback(pipeline.get());

    // The producer pipelines run on the system clock, the mixer pipeline must run at the same pace:
    // on the clock of an audio sink, the interaudiosrc would read faster or slower than the
    // producers write, and the audio would drift away from the video. The audio sink follows the
    // system clock instead, it compensates the drift of its device.
    GRefPtr clock = adoptGRef(gst_system_clock_obtain());
    gst_pipeline_use_clock(GST_PIPELINE_CAST(pipeline.get()), clock.get());

    auto* mixer = makeGStreamerElement("audiomixer"_s);

    // Use a device-specific sink if a GstDevice is provided, otherwise fall back to autoaudiosink.
    GstElement* audioSink;
    if (device) {
        audioSink = gst_device_create_element(device.get(), "audio-output-sink");
        if (!audioSink) {
            // Don't fall back to the default output: the audio of a device (for instance a virtual
            // device implemented by the application) must not be rendered on another device.
            GST_ERROR_OBJECT(pipeline.get(), "Failed to create the sink of device %" GST_PTR_FORMAT ", the audio rendered on it is discarded", device.get());
            ASSERT_NOT_REACHED_WITH_MESSAGE("Failed to create the sink of the device, the audio rendered on it is discarded");
            audioSink = makeGStreamerElement("fakesink"_s, "audio-output-sink"_s);
            g_object_set(audioSink, "sync", TRUE, nullptr);
        }
    } else
        audioSink = createAutoAudioSink({ });

    gst_bin_add_many(GST_BIN_CAST(pipeline.get()), mixer, audioSink, nullptr);
    bool isLinked;
    if (auto* configuration = virtualAudioDeviceConfiguration(device)) {
        // Make the mixer output buffers of the device size, at the device rate: the buffers would
        // no longer have the requested number of frames if they were resampled afterwards.
        int rate = configuration->sampleRate;
        int framesPerBuffer = configuration->framesPerBuffer;
        g_object_set(mixer, "output-buffer-duration-fraction", framesPerBuffer, rate, nullptr);
        auto* capsFilter = makeRateCapsFilter(rate);
        gst_bin_add(GST_BIN_CAST(pipeline.get()), capsFilter);
        isLinked = gst_element_link_many(mixer, capsFilter, audioSink, nullptr);
        GST_DEBUG_OBJECT(pipeline.get(), "Mixer configured to output buffers of %d frames at %d Hz", framesPerBuffer, rate);
    } else
        isLinked = gst_element_link(mixer, audioSink);
    if (!isLinked) {
        GST_ERROR_OBJECT(pipeline.get(), "Failed to link the mixer to %" GST_PTR_FORMAT ", the audio rendered on it is discarded", audioSink);
        ASSERT_NOT_REACHED_WITH_MESSAGE("Failed to link the mixer to the audio sink, the audio rendered on it is discarded");
    }
    gst_element_set_state(pipeline.get(), GST_STATE_READY);

    GST_DEBUG_OBJECT(pipeline.get(), "Created mixer pipeline for device '%s'.", makeUTF8CString(deviceId));
    auto mp = std::unique_ptr<MixerPipeline>(new MixerPipeline {
        .pipeline = WTF::move(pipeline),
        .mixer = GRefPtr<GstElement>(mixer),
        // The timer fires and is destroyed on the main thread, it gets its own copy of the ID.
        .teardownTimer = RunLoop::Timer(RunLoop::mainSingleton(), "GStreamerAudioMixer::TeardownTimer"_s, [deviceId = deviceId.isolatedCopy()] {
            GStreamerAudioMixer::singleton().teardownPipeline(deviceId);
        }),
        .device = device,
    });

    // The pipeline sends the latency it configures upstream from its sinks, keep it for the
    // producers.
    GRefPtr mixerSrcPad = adoptGRef(gst_element_get_static_pad(mixer, "src"));
    gst_pad_add_probe(mixerSrcPad.get(), GST_PAD_PROBE_TYPE_EVENT_UPSTREAM, [](GstPad*, GstPadProbeInfo* info, gpointer userData) -> GstPadProbeReturn {
        auto* event = GST_PAD_PROBE_INFO_EVENT(info);
        if (GST_EVENT_TYPE(event) == GST_EVENT_LATENCY) {
            GstClockTime latency;
            gst_event_parse_latency(event, &latency);
            (*static_cast<SharedLatency*>(userData))->store(GST_CLOCK_TIME_IS_VALID(latency) ? latency : 0);
        }
        return GST_PAD_PROBE_OK;
    }, new SharedLatency(mp->latency), [](gpointer userData) {
        delete static_cast<SharedLatency*>(userData);
    });

    auto addResult = locker->m_pipelines.set(deviceId, WTF::move(mp));
    return *addResult.iterator->value;
}

void GStreamerAudioMixer::teardownPipeline(const String& deviceId)
{
    ASSERT(isMainThread());
    std::unique_ptr<MixerPipeline> idlePipeline;
    {
        DataMutexLocker locker { m_streamingMembers };
        // A producer may have registered since the timer was started, or while this was waiting
        // for the lock: the timer is no longer active while it fires.
        auto it = locker->m_pipelines.find(deviceId);
        if (it == locker->m_pipelines.end() || it->value->mixer->numsinkpads) {
            GST_DEBUG("Teardown of device '%s' skipped, the pipeline is in use.", makeUTF8CString(deviceId));
            return;
        }
        GST_DEBUG("Teardown timeout reached for device '%s'.", makeUTF8CString(deviceId));
        idlePipeline = takeIdlePipeline(locker, deviceId);
    }
    // deviceId belongs to the timer running this function, which is destroyed with idlePipeline
    // when returning: it must not be used after this point.
}

std::unique_ptr<GStreamerAudioMixer::MixerPipeline> GStreamerAudioMixer::takeIdlePipeline(DataMutexLocker<StreamingMembers>& locker, const String& deviceId)
{
    auto mp = locker->m_pipelines.take(deviceId);
    RELEASE_ASSERT(mp && !mp->mixer->numsinkpads);
    GST_DEBUG_OBJECT(mp->pipeline.get(), "Destroying idle pipeline for device '%s'.", makeUTF8CString(deviceId));
    unregisterPipeline(mp->pipeline);
    disconnectSimpleBusMessageCallback(mp->pipeline.get());
    return mp;
}

void GStreamerAudioMixer::updatePipelineState(DataMutexLocker<StreamingMembers>& locker, MixerPipeline& mp, const String& deviceId)
{
    // The pipeline is shared by all the producers of the device: it runs as long as one of them
    // does. It stays in READY below PAUSED, the teardown timer handles idle pipelines.
    GstState state = GST_STATE_READY;
    for (auto& producer : locker->m_producers.values()) {
        if (producer.deviceId == deviceId)
            state = std::max(state, producer.state);
    }

    GST_DEBUG_OBJECT(mp.pipeline.get(), "Setting the pipeline to %s (%u mixer pads).", gst_state_get_name(state), mp.mixer->numsinkpads);
    gst_element_set_state(mp.pipeline.get(), state);
}

void GStreamerAudioMixer::ensureState(GstStateChange stateChange, const GRefPtr<GstPad>& mixerPad)
{
    DataMutexLocker locker { m_streamingMembers };
    auto it = locker->m_producers.find(mixerPad.get());
    RELEASE_ASSERT(it != locker->m_producers.end());
    auto& producer = it->value;

    auto pipelineIt = locker->m_pipelines.find(producer.deviceId);
    RELEASE_ASSERT(pipelineIt != locker->m_pipelines.end());
    auto& mp = *pipelineIt->value;

    GST_DEBUG_OBJECT(mp.pipeline.get(), "Handling %s transition of producer %" GST_PTR_FORMAT ".", gst_state_change_get_name(stateChange), mixerPad.get());

    auto previousState = std::exchange(producer.state, GST_STATE_TRANSITION_NEXT(stateChange));
    if (previousState == producer.state)
        return;

    // The producer is unregistered when it reaches GST_STATE_NULL, the pipeline is only updated then.
    if (producer.state < GST_STATE_READY)
        return;

    updatePipelineState(locker, mp, producer.deviceId);
}

GStreamerAudioMixer::Producer GStreamerAudioMixer::registerProducer(GstElement* interaudioSink, std::optional<int> forcedSampleRate, std::optional<GstClockTime> periodTime, const String& deviceId, const GRefPtr<GstDevice>& device)
{
    // The mixer of a device requiring a fixed buffer size runs at the device rate.
    if (auto* configuration = virtualAudioDeviceConfiguration(device))
        forcedSampleRate = configuration->sampleRate;

    DataMutexLocker locker { m_streamingMembers };
    // The strings stored in m_streamingMembers are only referenced with the lock held: this copy
    // isn't shared with the sink, and it's destroyed before the lock is released.
    String resolvedDeviceId = deviceId.isEmpty() ? "default"_s : deviceId.isolatedCopy();
    auto& mp = ensureMixerPipeline(locker, resolvedDeviceId, device);

    bool shouldStart = !mp.mixer->numsinkpads;
    GRefPtr mixerPad = adoptGRef(gst_element_request_pad_simple(mp.mixer.get(), "sink_%u"));
    if (!mixerPad) {
        // The producer renders nowhere, its sink handles the missing producer.
        GST_ERROR_OBJECT(mp.pipeline.get(), "Failed to request a mixer pad for %" GST_PTR_FORMAT ", the audio rendered on it is discarded", interaudioSink);
        ASSERT_NOT_REACHED_WITH_MESSAGE("Failed to request a mixer pad, the audio rendered on it is discarded");
        if (shouldStart)
            mp.teardownTimer.startOneShot(s_teardownTimeout);
        return { };
    }

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
        auto* capsfilter = makeRateCapsFilter(*forcedSampleRate);
        gst_bin_add(GST_BIN_CAST(bin), capsfilter);
        gst_element_link(audioResample, capsfilter);
    }

    if (GRefPtr pad = adoptGRef(gst_bin_find_unlinked_pad(GST_BIN_CAST(bin), GST_PAD_SRC)))
        gst_element_add_pad(GST_ELEMENT_CAST(bin), gst_ghost_pad_new("src", pad.get()));
    if (GRefPtr pad = adoptGRef(gst_bin_find_unlinked_pad(GST_BIN_CAST(bin), GST_PAD_SINK)))
        gst_element_add_pad(GST_ELEMENT_CAST(bin), gst_ghost_pad_new("sink", pad.get()));

    gst_bin_add_many(GST_BIN_CAST(mp.pipeline.get()), src, bin, nullptr);
    gst_element_link(src, bin);

    GRefPtr srcPad = adoptGRef(gst_element_get_static_pad(bin, "src"));
    gst_pad_link(srcPad.get(), mixerPad.get());

    if (shouldStart)
        gst_element_set_state(mp.pipeline.get(), GST_STATE_READY);
    else
        gst_bin_sync_children_states(GST_BIN_CAST(mp.pipeline.get()));

    locker->m_producers.set(mixerPad.get(), ProducerState { resolvedDeviceId, GST_STATE_READY });

    GST_DEBUG_OBJECT(mp.pipeline.get(), "Registered audio producer %" GST_PTR_FORMAT, mixerPad.get());
    dumpBinToDotFile(mp.pipeline.get(), "audio-mixer-after-producer-registration"_s);
    return { WTF::move(mixerPad), mp.latency };
}

void GStreamerAudioMixer::unregisterProducer(const GRefPtr<GstPad>& mixerPad)
{
    DataMutexLocker locker { m_streamingMembers };
    auto it = locker->m_producers.find(mixerPad.get());
    RELEASE_ASSERT(it != locker->m_producers.end());
    auto deviceId = it->value.deviceId;

    auto pipelineIt = locker->m_pipelines.find(deviceId);
    RELEASE_ASSERT(pipelineIt != locker->m_pipelines.end());
    auto& mp = *pipelineIt->value;

    GST_DEBUG_OBJECT(mp.pipeline.get(), "Unregistering audio producer %" GST_PTR_FORMAT " from device '%s'.", mixerPad.get(), makeUTF8CString(deviceId));

    GRefPtr peer = adoptGRef(gst_pad_get_peer(mixerPad.get()));
    GRefPtr bin = adoptGRef(gst_pad_get_parent_element(peer.get()));
    GRefPtr sinkPad = adoptGRef(gst_element_get_static_pad(bin.get(), "sink"));
    GRefPtr srcPad = adoptGRef(gst_pad_get_peer(sinkPad.get()));
    GRefPtr interaudioSrc = adoptGRef(gst_pad_get_parent_element(srcPad.get()));
    GST_LOG_OBJECT(mp.pipeline.get(), "interaudiosrc: %" GST_PTR_FORMAT, interaudioSrc.get());

    // The mixer pad is released before the producer elements are stopped: the streaming thread of
    // the producer can be waiting in the mixer pad for its queued buffer to be consumed, which
    // never happens while the pipeline is paused (its sink waits for preroll). Stopping the
    // interaudiosrc would then wait for that thread forever. Releasing the pad flushes it, waking
    // the thread up. The producer data is dropped from then on, an unlinked push would post an error.
    gst_pad_add_probe(peer.get(), GST_PAD_PROBE_TYPE_DATA_DOWNSTREAM, [](GstPad*, GstPadProbeInfo*, gpointer) -> GstPadProbeReturn {
        return GST_PAD_PROBE_DROP;
    }, nullptr, nullptr);
    gst_element_release_request_pad(mp.mixer.get(), mixerPad.get());

    gstElementLockAndSetState(interaudioSrc.get(), GST_STATE_NULL);
    gstElementLockAndSetState(bin.get(), GST_STATE_NULL);
    gst_element_unlink(interaudioSrc.get(), bin.get());

    gst_bin_remove_many(GST_BIN_CAST(mp.pipeline.get()), interaudioSrc.get(), bin.get(), nullptr);

    locker->m_producers.remove(it);

    // The remaining producers may all be in a lower state than the one that is gone.
    if (mp.mixer->numsinkpads) {
        updatePipelineState(locker, mp, deviceId);
        return;
    }

    // When no more producers remain, move to PAUSED and schedule deferred teardown.
    gst_element_set_state(mp.pipeline.get(), GST_STATE_PAUSED);
    mp.teardownTimer.startOneShot(s_teardownTimeout);
    GST_DEBUG_OBJECT(mp.pipeline.get(), "No producers left for device '%s', paused pipeline; teardown in %.0f seconds.", makeUTF8CString(deviceId), s_teardownTimeout.seconds());
    dumpBinToDotFile(mp.pipeline.get(), "audio-mixer-after-producer-unregistration"_s);
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif
