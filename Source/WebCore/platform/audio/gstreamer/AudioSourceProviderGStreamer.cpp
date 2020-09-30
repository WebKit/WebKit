/*
 *  Copyright (C) 2014 Igalia S.L
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
#include "AudioSourceProviderGStreamer.h"

#if ENABLE(WEB_AUDIO) && ENABLE(VIDEO) && USE(GSTREAMER)

#include "AudioBus.h"
#include "AudioSourceProviderClient.h"
#include <gst/app/gstappsink.h>
#include <gst/audio/audio-info.h>
#include <gst/base/gstadapter.h>

#if ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC)
#include "GStreamerAudioData.h"
#include "GStreamerMediaStreamSource.h"
#endif

namespace WebCore {

// For now the provider supports only stereo files at a fixed sample
// bitrate.
static const int gNumberOfChannels = 2;
static const float gSampleBitRate = 44100;

static GstFlowReturn onAppsinkNewBufferCallback(GstAppSink* sink, gpointer userData)
{
    return static_cast<AudioSourceProviderGStreamer*>(userData)->handleAudioBuffer(sink);
}

static void onGStreamerDeinterleavePadAddedCallback(GstElement*, GstPad* pad, AudioSourceProviderGStreamer* provider)
{
    provider->handleNewDeinterleavePad(pad);
}

static void onGStreamerDeinterleaveReadyCallback(GstElement*, AudioSourceProviderGStreamer* provider)
{
    provider->deinterleavePadsConfigured();
}

static void onGStreamerDeinterleavePadRemovedCallback(GstElement*, GstPad* pad, AudioSourceProviderGStreamer* provider)
{
    provider->handleRemovedDeinterleavePad(pad);
}

static GstPadProbeReturn onAppsinkFlushCallback(GstPad*, GstPadProbeInfo* info, gpointer userData)
{
    if (GST_PAD_PROBE_INFO_TYPE(info) & (GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM | GST_PAD_PROBE_TYPE_EVENT_FLUSH)) {
        GstEvent* event = GST_PAD_PROBE_INFO_EVENT(info);
        if (GST_EVENT_TYPE(event) == GST_EVENT_FLUSH_STOP) {
            AudioSourceProviderGStreamer* provider = reinterpret_cast<AudioSourceProviderGStreamer*>(userData);
            provider->clearAdapters();
        }
    }
    return GST_PAD_PROBE_OK;
}

static void copyGStreamerBuffersToAudioChannel(GstAdapter* adapter, AudioBus* bus , int channelNumber, size_t framesToProcess)
{
    if (!gst_adapter_available(adapter)) {
        bus->zero();
        return;
    }

    size_t bytes = framesToProcess * sizeof(float);
    if (gst_adapter_available(adapter) >= bytes) {
        gst_adapter_copy(adapter, bus->channel(channelNumber)->mutableData(), 0, bytes);
        gst_adapter_flush(adapter, bytes);
    } else
        bus->zero();
}

AudioSourceProviderGStreamer::AudioSourceProviderGStreamer()
    : m_notifier(MainThreadNotifier<MainThreadNotification>::create())
    , m_client(nullptr)
    , m_deinterleaveSourcePads(0)
    , m_deinterleavePadAddedHandlerId(0)
    , m_deinterleaveNoMorePadsHandlerId(0)
    , m_deinterleavePadRemovedHandlerId(0)
{
    m_frontLeftAdapter = gst_adapter_new();
    m_frontRightAdapter = gst_adapter_new();
}

#if ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC)
AudioSourceProviderGStreamer::AudioSourceProviderGStreamer(MediaStreamTrackPrivate& source)
    : m_notifier(MainThreadNotifier<MainThreadNotification>::create())
    , m_client(nullptr)
    , m_deinterleaveSourcePads(0)
    , m_deinterleavePadAddedHandlerId(0)
    , m_deinterleaveNoMorePadsHandlerId(0)
    , m_deinterleavePadRemovedHandlerId(0)
{
    m_frontLeftAdapter = gst_adapter_new();
    m_frontRightAdapter = gst_adapter_new();
    auto pipelineName = makeString("WebAudioProvider_MediaStreamTrack_", source.id());
    m_pipeline = adoptGRef(GST_ELEMENT(g_object_ref_sink(gst_element_factory_make("pipeline", pipelineName.utf8().data()))));
    auto src = webkitMediaStreamSrcNew();
    webkitMediaStreamSrcAddTrack(WEBKIT_MEDIA_STREAM_SRC(src), &source, true);

    m_audioSinkBin = adoptGRef(GST_ELEMENT(g_object_ref_sink(gst_parse_bin_from_description("tee name=audioTee", true, nullptr))));

    gst_bin_add_many(GST_BIN(m_pipeline.get()), src, m_audioSinkBin.get(), nullptr);
    gst_element_link(src, m_audioSinkBin.get());

    connectSimpleBusMessageCallback(m_pipeline.get());
}
#endif

AudioSourceProviderGStreamer::~AudioSourceProviderGStreamer()
{
    m_notifier->invalidate();

    GRefPtr<GstElement> deinterleave = adoptGRef(gst_bin_get_by_name(GST_BIN(m_audioSinkBin.get()), "deinterleave"));
    if (deinterleave && m_client) {
        g_signal_handler_disconnect(deinterleave.get(), m_deinterleavePadAddedHandlerId);
        g_signal_handler_disconnect(deinterleave.get(), m_deinterleaveNoMorePadsHandlerId);
        g_signal_handler_disconnect(deinterleave.get(), m_deinterleavePadRemovedHandlerId);
    }

    if (m_pipeline)
        gst_element_set_state(m_pipeline.get(), GST_STATE_NULL);

    g_object_unref(m_frontLeftAdapter);
    g_object_unref(m_frontRightAdapter);
}

void AudioSourceProviderGStreamer::configureAudioBin(GstElement* audioBin, GstElement* audioSink)
{
    m_audioSinkBin = audioBin;

    GstElement* audioTee = gst_element_factory_make("tee", "audioTee");
    GstElement* audioQueue = gst_element_factory_make("queue", nullptr);
    GstElement* audioConvert = gst_element_factory_make("audioconvert", nullptr);
    GstElement* audioConvert2 = gst_element_factory_make("audioconvert", nullptr);
    GstElement* audioResample = gst_element_factory_make("audioresample", nullptr);
    GstElement* audioResample2 = gst_element_factory_make("audioresample", nullptr);
    GstElement* volumeElement = gst_element_factory_make("volume", "volume");

    gst_bin_add_many(GST_BIN_CAST(m_audioSinkBin.get()), audioTee, audioQueue, audioConvert, audioResample, volumeElement, audioConvert2, audioResample2, audioSink, nullptr);

    // Add a ghostpad to the bin so it can proxy to tee.
    auto audioTeeSinkPad = adoptGRef(gst_element_get_static_pad(audioTee, "sink"));
    gst_element_add_pad(m_audioSinkBin.get(), gst_ghost_pad_new("sink", audioTeeSinkPad.get()));

    // Link a new src pad from tee to queue ! audioconvert ! audioresample ! volume ! audioconvert !
    // audioresample ! audiosink. The audioresample and audioconvert are needed to ensure the audio
    // sink receives buffers in the correct format.
    gst_element_link_pads_full(audioTee, "src_%u", audioQueue, "sink", GST_PAD_LINK_CHECK_NOTHING);
    gst_element_link_pads_full(audioQueue, "src", audioConvert, "sink", GST_PAD_LINK_CHECK_NOTHING);
    gst_element_link_pads_full(audioConvert, "src", audioResample, "sink", GST_PAD_LINK_CHECK_NOTHING);
    gst_element_link_pads_full(audioResample, "src", volumeElement, "sink", GST_PAD_LINK_CHECK_NOTHING);
    gst_element_link_pads_full(volumeElement, "src", audioConvert2, "sink", GST_PAD_LINK_CHECK_NOTHING);
    gst_element_link_pads_full(audioConvert2, "src", audioResample2, "sink", GST_PAD_LINK_CHECK_NOTHING);
    gst_element_link_pads_full(audioResample2, "src", audioSink, "sink", GST_PAD_LINK_CHECK_NOTHING);
}

void AudioSourceProviderGStreamer::provideInput(AudioBus* bus, size_t framesToProcess)
{
    auto locker = holdLock(m_adapterMutex);
    copyGStreamerBuffersToAudioChannel(m_frontLeftAdapter, bus, 0, framesToProcess);
    copyGStreamerBuffersToAudioChannel(m_frontRightAdapter, bus, 1, framesToProcess);
}

GstFlowReturn AudioSourceProviderGStreamer::handleAudioBuffer(GstAppSink* sink)
{
    if (!m_client)
        return GST_FLOW_OK;

    // Pull a buffer from appsink and store it the appropriate buffer
    // list for the audio channel it represents.
    GRefPtr<GstSample> sample = adoptGRef(gst_app_sink_pull_sample(sink));
    if (!sample)
        return gst_app_sink_is_eos(sink) ? GST_FLOW_EOS : GST_FLOW_ERROR;

    GstBuffer* buffer = gst_sample_get_buffer(sample.get());
    if (!buffer)
        return GST_FLOW_ERROR;

    GstCaps* caps = gst_sample_get_caps(sample.get());
    if (!caps)
        return GST_FLOW_ERROR;

    GstAudioInfo info;
    gst_audio_info_from_caps(&info, caps);

    auto locker = holdLock(m_adapterMutex);

    // Check the first audio channel. The buffer is supposed to store
    // data of a single channel anyway.
    switch (GST_AUDIO_INFO_POSITION(&info, 0)) {
    case GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT:
    case GST_AUDIO_CHANNEL_POSITION_MONO:
        gst_adapter_push(m_frontLeftAdapter, gst_buffer_ref(buffer));
        break;
    case GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT:
        gst_adapter_push(m_frontRightAdapter, gst_buffer_ref(buffer));
        break;
    default:
        break;
    }

    return GST_FLOW_OK;
}

void AudioSourceProviderGStreamer::setClient(AudioSourceProviderClient* client)
{
    if (m_client)
        return;

    ASSERT(client);
    m_client = client;

    if (m_pipeline)
        gst_element_set_state(m_pipeline.get(), GST_STATE_PLAYING);

    // The volume element is used to mute audio playback towards the
    // autoaudiosink. This is needed to avoid double playback of audio
    // from our audio sink and from the WebAudio AudioDestination node
    // supposedly configured already by application side.
    GRefPtr<GstElement> volumeElement = adoptGRef(gst_bin_get_by_name(GST_BIN(m_audioSinkBin.get()), "volume"));

    if (volumeElement)
        g_object_set(volumeElement.get(), "mute", TRUE, nullptr);

    // The audioconvert and audioresample elements are needed to
    // ensure deinterleave and the sinks downstream receive buffers in
    // the format specified by the capsfilter.
    GstElement* audioQueue = gst_element_factory_make("queue", nullptr);
    GstElement* audioConvert  = gst_element_factory_make("audioconvert", nullptr);
    GstElement* audioResample = gst_element_factory_make("audioresample", nullptr);
    GstElement* capsFilter = gst_element_factory_make("capsfilter", nullptr);
    GstElement* deInterleave = gst_element_factory_make("deinterleave", "deinterleave");

    g_object_set(deInterleave, "keep-positions", TRUE, nullptr);
    m_deinterleavePadAddedHandlerId = g_signal_connect(deInterleave, "pad-added", G_CALLBACK(onGStreamerDeinterleavePadAddedCallback), this);
    m_deinterleaveNoMorePadsHandlerId = g_signal_connect(deInterleave, "no-more-pads", G_CALLBACK(onGStreamerDeinterleaveReadyCallback), this);
    m_deinterleavePadRemovedHandlerId = g_signal_connect(deInterleave, "pad-removed", G_CALLBACK(onGStreamerDeinterleavePadRemovedCallback), this);

    GstCaps* caps = gst_caps_new_simple("audio/x-raw", "rate", G_TYPE_INT, static_cast<int>(gSampleBitRate),
        "channels", G_TYPE_INT, gNumberOfChannels,
        "format", G_TYPE_STRING, GST_AUDIO_NE(F32),
        "layout", G_TYPE_STRING, "interleaved", nullptr);

    g_object_set(capsFilter, "caps", caps, nullptr);
    gst_caps_unref(caps);

    gst_bin_add_many(GST_BIN(m_audioSinkBin.get()), audioQueue, audioConvert, audioResample, capsFilter, deInterleave, nullptr);

    GRefPtr<GstElement> audioTee = adoptGRef(gst_bin_get_by_name(GST_BIN(m_audioSinkBin.get()), "audioTee"));

    // Link a new src pad from tee to queue ! audioconvert !
    // audioresample ! capsfilter ! deinterleave. Later
    // on each deinterleaved planar audio channel will be routed to an
    // appsink for data extraction and processing.
    gst_element_link_pads_full(audioTee.get(), "src_%u", audioQueue, "sink", GST_PAD_LINK_CHECK_NOTHING);
    gst_element_link_pads_full(audioQueue, "src", audioConvert, "sink", GST_PAD_LINK_CHECK_NOTHING);
    gst_element_link_pads_full(audioConvert, "src", audioResample, "sink", GST_PAD_LINK_CHECK_NOTHING);
    gst_element_link_pads_full(audioResample, "src", capsFilter, "sink", GST_PAD_LINK_CHECK_NOTHING);
    gst_element_link_pads_full(capsFilter, "src", deInterleave, "sink", GST_PAD_LINK_CHECK_NOTHING);

    gst_element_sync_state_with_parent(audioQueue);
    gst_element_sync_state_with_parent(audioConvert);
    gst_element_sync_state_with_parent(audioResample);
    gst_element_sync_state_with_parent(capsFilter);
    gst_element_sync_state_with_parent(deInterleave);
}

void AudioSourceProviderGStreamer::handleNewDeinterleavePad(GstPad* pad)
{
    m_deinterleaveSourcePads++;

    if (m_deinterleaveSourcePads > 2) {
        g_warning("The AudioSourceProvider supports only mono and stereo audio. Silencing out this new channel.");
        GstElement* queue = gst_element_factory_make("queue", nullptr);
        GstElement* sink = gst_element_factory_make("fakesink", nullptr);
        g_object_set(sink, "async", FALSE, nullptr);
        gst_bin_add_many(GST_BIN(m_audioSinkBin.get()), queue, sink, nullptr);

        GRefPtr<GstPad> sinkPad = adoptGRef(gst_element_get_static_pad(queue, "sink"));
        gst_pad_link_full(pad, sinkPad.get(), GST_PAD_LINK_CHECK_NOTHING);

        GQuark quark = g_quark_from_static_string("peer");
        g_object_set_qdata(G_OBJECT(pad), quark, sinkPad.get());
        gst_element_link_pads_full(queue, "src", sink, "sink", GST_PAD_LINK_CHECK_NOTHING);
        gst_element_sync_state_with_parent(queue);
        gst_element_sync_state_with_parent(sink);
        return;
    }

    // A new pad for a planar channel was added in deinterleave. Plug
    // in an appsink so we can pull the data from each
    // channel. Pipeline looks like:
    // ... deinterleave ! queue ! appsink.
    GstElement* queue = gst_element_factory_make("queue", nullptr);
    GstElement* sink = gst_element_factory_make("appsink", nullptr);

    GstAppSinkCallbacks callbacks;
    callbacks.eos = nullptr;
    callbacks.new_preroll = nullptr;
    callbacks.new_sample = onAppsinkNewBufferCallback;
    gst_app_sink_set_callbacks(GST_APP_SINK(sink), &callbacks, this, nullptr);

    g_object_set(sink, "async", FALSE, nullptr);

    GRefPtr<GstCaps> caps = adoptGRef(gst_caps_new_simple("audio/x-raw", "rate", G_TYPE_INT, static_cast<int>(gSampleBitRate),
        "channels", G_TYPE_INT, 1,
        "format", G_TYPE_STRING, GST_AUDIO_NE(F32),
        "layout", G_TYPE_STRING, "interleaved", nullptr));

    gst_app_sink_set_caps(GST_APP_SINK(sink), caps.get());

    gst_bin_add_many(GST_BIN(m_audioSinkBin.get()), queue, sink, nullptr);

    GRefPtr<GstPad> sinkPad = adoptGRef(gst_element_get_static_pad(queue, "sink"));
    gst_pad_link_full(pad, sinkPad.get(), GST_PAD_LINK_CHECK_NOTHING);

    GQuark quark = g_quark_from_static_string("peer");
    g_object_set_qdata(G_OBJECT(pad), quark, sinkPad.get());

    gst_element_link_pads_full(queue, "src", sink, "sink", GST_PAD_LINK_CHECK_NOTHING);

    sinkPad = adoptGRef(gst_element_get_static_pad(sink, "sink"));
    gst_pad_add_probe(sinkPad.get(), GST_PAD_PROBE_TYPE_EVENT_FLUSH, onAppsinkFlushCallback, this, nullptr);

    gst_element_sync_state_with_parent(queue);
    gst_element_sync_state_with_parent(sink);
}

void AudioSourceProviderGStreamer::handleRemovedDeinterleavePad(GstPad* pad)
{
    m_deinterleaveSourcePads--;

    // Remove the queue ! appsink chain downstream of deinterleave.
    GQuark quark = g_quark_from_static_string("peer");
    GstPad* sinkPad = GST_PAD_CAST(g_object_get_qdata(G_OBJECT(pad), quark));
    if (!sinkPad)
        return;

    GRefPtr<GstElement> queue = adoptGRef(gst_pad_get_parent_element(sinkPad));
    GRefPtr<GstPad> queueSrcPad = adoptGRef(gst_element_get_static_pad(queue.get(), "src"));
    GRefPtr<GstPad> appsinkSinkPad = adoptGRef(gst_pad_get_peer(queueSrcPad.get()));
    GRefPtr<GstElement> sink = adoptGRef(gst_pad_get_parent_element(appsinkSinkPad.get()));
    gst_element_set_state(sink.get(), GST_STATE_NULL);
    gst_element_set_state(queue.get(), GST_STATE_NULL);
    gst_element_unlink(queue.get(), sink.get());
    gst_bin_remove_many(GST_BIN(m_audioSinkBin.get()), queue.get(), sink.get(), nullptr);
}

void AudioSourceProviderGStreamer::deinterleavePadsConfigured()
{
    m_notifier->notify(MainThreadNotification::DeinterleavePadsConfigured, [this] {
        ASSERT(m_client);
        ASSERT(m_deinterleaveSourcePads == gNumberOfChannels);

        m_client->setFormat(m_deinterleaveSourcePads, gSampleBitRate);
    });
}

void AudioSourceProviderGStreamer::clearAdapters()
{
    auto locker = holdLock(m_adapterMutex);
    gst_adapter_clear(m_frontLeftAdapter);
    gst_adapter_clear(m_frontRightAdapter);
}

} // WebCore

#endif // ENABLE(WEB_AUDIO) && ENABLE(VIDEO) && USE(GSTREAMER)
