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
#include "GStreamerCommon.h"
#include <gst/app/gstappsink.h>
#include <gst/audio/audio-info.h>
#include <gst/base/gstadapter.h>

#if ENABLE(MEDIA_STREAM)
#include "GStreamerAudioData.h"
#include "GStreamerMediaStreamSource.h"
#endif

namespace WebCore {

// For now the provider supports only files at a fixed sample bitrate.
static const float gSampleBitRate = 44100;

GST_DEBUG_CATEGORY(webkit_audio_provider_debug);
#define GST_CAT_DEFAULT webkit_audio_provider_debug

static void initializeDebugCategory()
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_audio_provider_debug, "webkitaudioprovider", 0, "WebKit WebAudio Provider");
    });
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

static void copyGStreamerBuffersToAudioChannel(GstAdapter* adapter, AudioBus* bus , int channelNumber, size_t framesToProcess)
{
    auto available = gst_adapter_available(adapter);
    if (!available) {
        GST_TRACE("Adapter empty, silencing bus");
        bus->zero();
        return;
    }

    GST_TRACE("%zu samples available for channel %d (%zu frames requested)", available, channelNumber, framesToProcess);
    size_t bytes = framesToProcess * sizeof(float);
    if (available >= bytes) {
        gst_adapter_copy(adapter, bus->channel(channelNumber)->mutableData(), 0, bytes);
        gst_adapter_flush(adapter, bytes);
    } else
        bus->zero();
}

AudioSourceProviderGStreamer::AudioSourceProviderGStreamer()
    : m_notifier(MainThreadNotifier<MainThreadNotification>::create())
{
    initializeDebugCategory();
}

#if ENABLE(MEDIA_STREAM)
AudioSourceProviderGStreamer::AudioSourceProviderGStreamer(MediaStreamTrackPrivate& source)
    : m_notifier(MainThreadNotifier<MainThreadNotification>::create())
{
    initializeDebugCategory();
    auto pipelineName = makeString("WebAudioProvider_MediaStreamTrack_", source.id());
    m_pipeline = gst_element_factory_make("pipeline", pipelineName.utf8().data());
    GST_DEBUG_OBJECT(m_pipeline.get(), "MediaStream WebAudio provider created");
    auto src = webkitMediaStreamSrcNew();
    webkitMediaStreamSrcAddTrack(WEBKIT_MEDIA_STREAM_SRC(src), &source, true);

    m_audioSinkBin = gst_parse_bin_from_description("tee name=audioTee", true, nullptr);

    auto* decodebin = makeGStreamerElement("decodebin3", nullptr);
    g_signal_connect_swapped(decodebin, "pad-added", G_CALLBACK(+[](AudioSourceProviderGStreamer* provider, GstPad* pad) {
        auto padCaps = adoptGRef(gst_pad_query_caps(pad, nullptr));
        bool isAudio = doCapsHaveType(padCaps.get(), "audio");
        if (webkitGstCheckVersion(1, 18, 0))
            RELEASE_ASSERT(isAudio);
        else if (!isAudio)
            return;

        auto sinkPad = adoptGRef(gst_element_get_static_pad(provider->m_audioSinkBin.get(), "sink"));
        gst_pad_link(pad, sinkPad.get());
        gst_element_sync_state_with_parent(provider->m_audioSinkBin.get());
    }), this);

    gst_bin_add_many(GST_BIN_CAST(m_pipeline.get()), src, decodebin, m_audioSinkBin.get(), nullptr);
    gst_element_link(src, decodebin);

    auto bus = adoptGRef(gst_pipeline_get_bus(GST_PIPELINE(m_pipeline.get())));
    ASSERT(bus);

    gst_bus_set_sync_handler(bus.get(), [](GstBus*, GstMessage* messageRef, gpointer userData) -> GstBusSyncReply {
        auto message = adoptGRef(messageRef);
        auto* decodebin = GST_ELEMENT_CAST(userData);
        if (GST_MESSAGE_TYPE(message.get()) == GST_MESSAGE_LATENCY) {
            auto pipeline = adoptGRef(gst_element_get_parent(decodebin));
            gst_bin_recalculate_latency(GST_BIN_CAST(pipeline.get()));
            return GST_BUS_DROP;
        }

        if (GST_MESSAGE_TYPE(message.get()) != GST_MESSAGE_STREAM_COLLECTION || GST_MESSAGE_SRC(message.get()) != GST_OBJECT_CAST(decodebin))
            return GST_BUS_DROP;

        GRefPtr<GstStreamCollection> collection;
        gst_message_parse_stream_collection(message.get(), &collection.outPtr());
        if (!collection)
            return GST_BUS_DROP;

        unsigned size = gst_stream_collection_get_size(collection.get());
        GList* streams = nullptr;
        for (unsigned i = 0; i < size; i++) {
            auto* stream = gst_stream_collection_get_stream(collection.get(), i);
            auto streamType = gst_stream_get_stream_type(stream);
            if (streamType == GST_STREAM_TYPE_AUDIO) {
                streams = g_list_append(streams, const_cast<char*>(gst_stream_get_stream_id(stream)));
                break;
            }
        }
        if (streams) {
            gst_element_send_event(decodebin, gst_event_new_select_streams(streams));
            g_list_free(streams);
        }

        return GST_BUS_DROP;
    }, gst_object_ref(decodebin), gst_object_unref);
}
#endif

AudioSourceProviderGStreamer::~AudioSourceProviderGStreamer()
{
    m_notifier->invalidate();

    auto deinterleave = adoptGRef(gst_bin_get_by_name(GST_BIN_CAST(m_audioSinkBin.get()), "deinterleave"));
    if (deinterleave && m_client) {
        g_signal_handler_disconnect(deinterleave.get(), m_deinterleavePadAddedHandlerId);
        g_signal_handler_disconnect(deinterleave.get(), m_deinterleaveNoMorePadsHandlerId);
        g_signal_handler_disconnect(deinterleave.get(), m_deinterleavePadRemovedHandlerId);
    }

    setClient(nullptr);
#if ENABLE(MEDIA_STREAM)
    if (m_pipeline)
        gst_element_set_state(m_pipeline.get(), GST_STATE_NULL);
#endif
}

void AudioSourceProviderGStreamer::configureAudioBin(GstElement* audioBin, GstElement* audioSink)
{
    m_audioSinkBin = audioBin;

    GstElement* audioTee = gst_element_factory_make("tee", "audioTee");
    GstElement* audioQueue = gst_element_factory_make("queue", nullptr);
    GstElement* audioConvert = makeGStreamerElement("audioconvert", nullptr);
    GstElement* audioConvert2 = makeGStreamerElement("audioconvert", nullptr);
    GstElement* audioResample = makeGStreamerElement("audioresample", nullptr);
    GstElement* audioResample2 = makeGStreamerElement("audioresample", nullptr);
    GstElement* volumeElement = makeGStreamerElement("volume", "volume");

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
    GST_TRACE("Fetching buffers from adapters");
    if (!m_adapterLock.tryLock())
        return;

    Locker locker { AdoptLock, m_adapterLock };
    for (auto& it : m_adapters)
        copyGStreamerBuffersToAudioChannel(it.value.get(), bus, it.key - 1, framesToProcess);
}

GstFlowReturn AudioSourceProviderGStreamer::handleSample(GstAppSink* sink, bool isPreroll)
{
    GST_TRACE("Pulling audio sample from the sink");
    auto sample = adoptGRef(isPreroll ? gst_app_sink_try_pull_preroll(sink, 0) : gst_app_sink_try_pull_sample(sink, 0));
    if (!sample)
        return gst_app_sink_is_eos(sink) ? GST_FLOW_EOS : GST_FLOW_ERROR;

    if (!m_client)
        return GST_FLOW_OK;

    GstBuffer* buffer = gst_sample_get_buffer(sample.get());
    if (!buffer)
        return GST_FLOW_ERROR;

    GST_TRACE("Storing audio sample %" GST_PTR_FORMAT, sample.get());
    {
        Locker locker { m_adapterLock };
        GQuark quark = g_quark_from_static_string("channel-id");
        int channelId = GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(sink), quark));
        GST_DEBUG("Channel ID: %d", channelId);

        auto result = m_adapters.ensure(channelId, [&] {
            return gst_adapter_new();
        });
        auto* adapter = result.iterator->value.get();
        gst_adapter_push(adapter, gst_buffer_ref(buffer));
    }

    if (gst_app_sink_is_eos(sink))
        return GST_FLOW_EOS;
    return GST_FLOW_OK;
}

void AudioSourceProviderGStreamer::setClient(WeakPtr<AudioSourceProviderClient>&& newClient)
{
    if (client() == newClient.get())
        return;

#if ENABLE(MEDIA_STREAM)
    GST_DEBUG_OBJECT(m_pipeline.get(), "Setting up client %p (previous: %p)", newClient.get(), client());
#endif
    bool previousClientWasValid = !!m_client;
    m_client = WTFMove(newClient);

    // The volume element is used to mute audio playback towards the
    // autoaudiosink. This is needed to avoid double playback of audio
    // from our audio sink and from the WebAudio AudioDestination node
    // supposedly configured already by application side.
    auto volumeElement = adoptGRef(gst_bin_get_by_name(GST_BIN_CAST(m_audioSinkBin.get()), "volume"));

    if (volumeElement) {
        bool shouldMute = !!m_client;
        g_object_set(volumeElement.get(), "mute", shouldMute, nullptr);
    }

    auto audioTee = adoptGRef(gst_bin_get_by_name(GST_BIN_CAST(m_audioSinkBin.get()), "audioTee"));
    if (!m_client || previousClientWasValid) {
        auto audioQueue = adoptGRef(gst_bin_get_by_name(GST_BIN_CAST(m_audioSinkBin.get()), "queue"));
        auto audioConvert = adoptGRef(gst_bin_get_by_name(GST_BIN_CAST(m_audioSinkBin.get()), "audioconvert"));
        auto audioResample = adoptGRef(gst_bin_get_by_name(GST_BIN_CAST(m_audioSinkBin.get()), "audioresample"));
        auto capsFilter = adoptGRef(gst_bin_get_by_name(GST_BIN_CAST(m_audioSinkBin.get()), "capsfilter"));
        auto deInterleave = adoptGRef(gst_bin_get_by_name(GST_BIN_CAST(m_audioSinkBin.get()), "deinterleave"));
        auto queueSinkPad = adoptGRef(gst_element_get_static_pad(audioQueue.get(), "sink"));
        auto teeSrcPad = adoptGRef(gst_pad_get_peer(queueSinkPad.get()));

        GST_DEBUG("Cleaning up audio deinterleave chain");
        gst_element_set_locked_state(m_audioSinkBin.get(), true);

        gst_element_set_state(audioQueue.get(), GST_STATE_NULL);
        gst_element_set_state(audioConvert.get(), GST_STATE_NULL);
        gst_element_set_state(audioResample.get(), GST_STATE_NULL);
        gst_element_set_state(capsFilter.get(), GST_STATE_NULL);
        gst_element_set_state(deInterleave.get(), GST_STATE_NULL);
        gst_element_unlink_many(audioTee.get(), audioQueue.get(), audioConvert.get(), audioResample.get(), capsFilter.get(), deInterleave.get(), nullptr);
        gst_element_set_locked_state(m_audioSinkBin.get(), false);
        gst_bin_remove_many(GST_BIN_CAST(m_audioSinkBin.get()), audioQueue.get(), audioConvert.get(), audioResample.get(), capsFilter.get(), deInterleave.get(), nullptr);
        gst_element_release_request_pad(audioTee.get(), teeSrcPad.get());
    }

    if (m_client) {
        // The audioconvert and audioresample elements are needed to
        // ensure deinterleave and the sinks downstream receive buffers in
        // the format specified by the capsfilter.
        auto* audioQueue = gst_element_factory_make("queue", "queue");
        auto* audioConvert = makeGStreamerElement("audioconvert", "audioconvert");
        auto* audioResample = makeGStreamerElement("audioresample", "audioresample");
        auto* capsFilter = gst_element_factory_make("capsfilter", "capsfilter");
        auto* deInterleave = makeGStreamerElement("deinterleave", "deinterleave");

        GST_DEBUG("Setting up audio deinterleave chain");
        g_object_set(deInterleave, "keep-positions", TRUE, nullptr);
        m_deinterleavePadAddedHandlerId = g_signal_connect(deInterleave, "pad-added", G_CALLBACK(onGStreamerDeinterleavePadAddedCallback), this);
        m_deinterleaveNoMorePadsHandlerId = g_signal_connect(deInterleave, "no-more-pads", G_CALLBACK(onGStreamerDeinterleaveReadyCallback), this);
        m_deinterleavePadRemovedHandlerId = g_signal_connect(deInterleave, "pad-removed", G_CALLBACK(onGStreamerDeinterleavePadRemovedCallback), this);

        auto caps = adoptGRef(gst_caps_new_simple("audio/x-raw", "rate", G_TYPE_INT, static_cast<int>(gSampleBitRate),
            "format", G_TYPE_STRING, GST_AUDIO_NE(F32), "layout", G_TYPE_STRING, "interleaved", nullptr));
        g_object_set(capsFilter, "caps", caps.get(), nullptr);

        gst_bin_add_many(GST_BIN_CAST(m_audioSinkBin.get()), audioQueue, audioConvert, audioResample, capsFilter, deInterleave, nullptr);

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

    m_deinterleaveSourcePads = 0;
    clearAdapters();
#if ENABLE(MEDIA_STREAM)
    if (m_pipeline)
        gst_element_set_state(m_pipeline.get(), m_client ? GST_STATE_PLAYING : GST_STATE_NULL);
#endif
}

void AudioSourceProviderGStreamer::handleNewDeinterleavePad(GstPad* pad)
{
#if ENABLE(MEDIA_STREAM)
    GST_DEBUG_OBJECT(m_pipeline.get(), "New pad %" GST_PTR_FORMAT, pad);
#endif

    // A new pad for a planar channel was added in deinterleave. Plug
    // in an appsink so we can pull the data from each
    // channel. Pipeline looks like:
    // ... deinterleave ! queue ! appsink.
    auto* queue = gst_element_factory_make("queue", nullptr);
    auto* sink = makeGStreamerElement("appsink", nullptr);

    static GstAppSinkCallbacks callbacks = {
        nullptr,
        [](GstAppSink* sink, gpointer userData) -> GstFlowReturn {
            return static_cast<AudioSourceProviderGStreamer*>(userData)->handleSample(sink, true);
        },
        [](GstAppSink* sink, gpointer userData) -> GstFlowReturn {
            return static_cast<AudioSourceProviderGStreamer*>(userData)->handleSample(sink, false);
        },
#if GST_CHECK_VERSION(1, 20, 0)
        // new_event
        nullptr,
#endif
        { nullptr }
    };
    gst_app_sink_set_callbacks(GST_APP_SINK(sink), &callbacks, this, nullptr);
    // The provider client might request samples faster than the current clock speed, so this sink
    // should process buffers as fast as possible.
    g_object_set(sink, "async", FALSE, "sync", FALSE, nullptr);

    // Some intermediate bins are eating up the EOS message posted to the bus of the inner bin that
    // holds the appsink. Make sure that the main pipeline gets notified about it, so the player
    // private can properly handle EOS.
    g_signal_connect_swapped(GST_APP_SINK(sink), "eos", G_CALLBACK(+[](GstElement*, GstElement* appsink) {
        GstElement* pipeline;
        for (pipeline = appsink; pipeline && GST_ELEMENT_PARENT(pipeline); pipeline = GST_ELEMENT_PARENT(pipeline)) { }
        if (pipeline && pipeline->bus)
            gst_bus_post(pipeline->bus, gst_message_new_eos(GST_OBJECT(appsink)));
    }), sink);

    auto caps = adoptGRef(gst_caps_new_simple("audio/x-raw", "rate", G_TYPE_INT, static_cast<int>(gSampleBitRate),
        "channels", G_TYPE_INT, 1, "format", G_TYPE_STRING, GST_AUDIO_NE(F32), "layout", G_TYPE_STRING, "interleaved", nullptr));
    gst_app_sink_set_caps(GST_APP_SINK(sink), caps.get());

    gst_bin_add_many(GST_BIN_CAST(m_audioSinkBin.get()), queue, sink, nullptr);

    gst_element_link(queue, sink);

    auto sinkPad = adoptGRef(gst_element_get_static_pad(queue, "sink"));
    gst_pad_link_full(pad, sinkPad.get(), GST_PAD_LINK_CHECK_NOTHING);

    GQuark quark = g_quark_from_static_string("peer");
    g_object_set_qdata(G_OBJECT(pad), quark, sinkPad.get());
    m_deinterleaveSourcePads++;
    GQuark channelIdQuark = g_quark_from_static_string("channel-id");
    g_object_set_qdata(G_OBJECT(sink), channelIdQuark, GINT_TO_POINTER(m_deinterleaveSourcePads));

    sinkPad = adoptGRef(gst_element_get_static_pad(sink, "sink"));
    gst_pad_add_probe(sinkPad.get(), GST_PAD_PROBE_TYPE_EVENT_FLUSH, [](GstPad*, GstPadProbeInfo* info, gpointer userData) {
        if (GST_PAD_PROBE_INFO_TYPE(info) & (GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM | GST_PAD_PROBE_TYPE_EVENT_FLUSH)) {
            GstEvent* event = GST_PAD_PROBE_INFO_EVENT(info);
            if (GST_EVENT_TYPE(event) == GST_EVENT_FLUSH_STOP) {
                auto* provider = reinterpret_cast<AudioSourceProviderGStreamer*>(userData);
                provider->clearAdapters();
            }
        }
        return GST_PAD_PROBE_OK;
    }, this, nullptr);

    gst_element_sync_state_with_parent(queue);
    gst_element_sync_state_with_parent(sink);
}

void AudioSourceProviderGStreamer::handleRemovedDeinterleavePad(GstPad* pad)
{
    if (GST_PAD_DIRECTION(pad) != GST_PAD_SRC)
        return;

    GST_DEBUG("Pad %" GST_PTR_FORMAT " gone", pad);
    m_deinterleaveSourcePads--;

    GQuark quark = g_quark_from_static_string("peer");
    GstPad* sinkPad = GST_PAD_CAST(g_object_get_qdata(G_OBJECT(pad), quark));
    if (!sinkPad)
        return;

    auto queue = adoptGRef(gst_pad_get_parent_element(sinkPad));
    auto srcPad = adoptGRef(gst_element_get_static_pad(queue.get(), "src"));
    auto sinkSinkPad = adoptGRef(gst_pad_get_peer(srcPad.get()));
    auto sink = adoptGRef(gst_pad_get_parent_element(sinkSinkPad.get()));

    g_signal_handlers_disconnect_by_data(sink.get(), sink.get());

    gst_pad_unlink(srcPad.get(), sinkSinkPad.get());
    gst_element_set_state(queue.get(), GST_STATE_NULL);
    gst_element_set_state(sink.get(), GST_STATE_NULL);
    gst_bin_remove_many(GST_BIN_CAST(m_audioSinkBin.get()), queue.get(), sink.get(), nullptr);
}

void AudioSourceProviderGStreamer::deinterleavePadsConfigured()
{
    GST_DEBUG("Deinterleave configured with %d channels, notifying client", m_deinterleaveSourcePads);
    m_notifier->notify(MainThreadNotification::DeinterleavePadsConfigured, [numberOfChannels = m_deinterleaveSourcePads, sampleRate = gSampleBitRate, client = m_client] {
        if (client)
            client->setFormat(numberOfChannels, sampleRate);
    });
}

void AudioSourceProviderGStreamer::clearAdapters()
{
    Locker locker { m_adapterLock };
    for (auto& adapter : m_adapters.values())
        gst_adapter_clear(adapter.get());
}

} // WebCore

#endif // ENABLE(WEB_AUDIO) && ENABLE(VIDEO) && USE(GSTREAMER)
