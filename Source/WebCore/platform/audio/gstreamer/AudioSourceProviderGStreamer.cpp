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
#include <wtf/glib/GThreadSafeWeakPtr.h>
#include <wtf/text/MakeString.h>

#if ENABLE(MEDIA_STREAM)
#include "GStreamerAudioData.h"
#include "GStreamerMediaStreamSource.h"
#include "MediaStreamPrivate.h"
#endif

namespace WebCore {

GST_DEBUG_CATEGORY(webkit_audio_provider_debug);
#define GST_CAT_DEFAULT webkit_audio_provider_debug

#if GST_CHECK_VERSION(1, 22, 0)
#define ASP_DEBUG(...) GST_DEBUG_ID(m_providerId.data(), __VA_ARGS__)
#define ASP_TRACE(...) GST_TRACE_ID(m_providerId.data(), __VA_ARGS__)
#define ASP_WARNING(...) GST_WARNING_ID(m_providerId.data(), __VA_ARGS__)
#define ASP_MEMDUMP(...) GST_MEMDUMP_ID(m_providerId.data(), __VA_ARGS__)
#define ASP_TRACE(...) GST_TRACE_ID(m_providerId.data(), __VA_ARGS__)
#else
#define ASP_DEBUG(...) GST_DEBUG(__VA_ARGS__)
#define ASP_TRACE(...) GST_TRACE(__VA_ARGS__)
#define ASP_WARNING(...) GST_WARNING(__VA_ARGS__)
#define ASP_MEMDUMP(...) GST_MEMDUMP(__VA_ARGS__)
#define ASP_TRACE(...) GST_TRACE(__VA_ARGS__)
#endif

static void initializeAudioSourceProviderDebugCategory()
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

static void onGStreamerDeinterleavePadRemovedCallback(GstElement*, GstPad* pad, AudioSourceProviderGStreamer* provider)
{
    provider->handleRemovedDeinterleavePad(pad);
}

void AudioSourceProviderGStreamer::copyGStreamerBuffersToAudioChannel(GstAdapter* adapter, AudioBus& bus, int channelNumber, size_t framesToProcess)
{
    auto available = gst_adapter_available(adapter);
    if (!available) {
        ASP_TRACE("Adapter empty, silencing bus");
        bus.zero();
        return;
    }

    ASP_TRACE("%zu frames available for channel %d (%zu frames requested)", available / sizeof(float), channelNumber, framesToProcess);
    size_t bytes = framesToProcess * sizeof(float);
    if (available >= bytes) {
        gst_adapter_copy(adapter, bus.channel(channelNumber)->mutableData(), 0, bytes);
        gst_adapter_flush(adapter, bytes);
    } else {
        ASP_TRACE("Not enough data available %zu < %zu", available, bytes);
        bus.zero();
    }
}

AudioSourceProviderGStreamer::AudioSourceProviderGStreamer()
    : m_notifier(MainThreadNotifier<MainThreadNotification>::create())
{
    initialize();
}

void AudioSourceProviderGStreamer::initialize()
{
    initializeAudioSourceProviderDebugCategory();

    static Atomic<uint64_t> nProvider = 0;
    m_providerId = makeString("webkit-audio-source-provider-"_s, nProvider.exchangeAdd(1)).ascii();
}

#if ENABLE(MEDIA_STREAM)
AudioSourceProviderGStreamer::AudioSourceProviderGStreamer(MediaStreamTrackPrivate& source)
    : m_captureSource(source)
    , m_notifier(MainThreadNotifier<MainThreadNotification>::create())
{
    initialize();
    registerWebKitGStreamerElements();

    auto pipelineNamePrefix = ""_s;
    if (m_captureSource->source().isIncomingAudioSource())
        pipelineNamePrefix = "incoming-"_s;

    auto pipelineName = makeString(pipelineNamePrefix, "WebAudioProvider_MediaStreamTrack_"_s, source.id());
    m_pipeline = gst_element_factory_make("pipeline", pipelineName.utf8().data());
    registerActivePipeline(m_pipeline);
    ASP_DEBUG("MediaStream WebAudio provider created");

    m_streamPrivate = MediaStreamPrivate::create(Logger::create(this), { source });

    m_audioSinkBin = gst_parse_bin_from_description("tee name=audioTee", true, nullptr);

    auto* decodebin = makeGStreamerElement("uridecodebin3"_s);

    g_signal_connect_swapped(decodebin, "source-setup", G_CALLBACK(+[](AudioSourceProviderGStreamer* provider, GstElement* sourceElement) {
        if (!WEBKIT_IS_MEDIA_STREAM_SRC(sourceElement)) {
            ASSERT_NOT_REACHED();
            return;
        }
        webkitMediaStreamSrcSetStream(WEBKIT_MEDIA_STREAM_SRC(sourceElement), provider->m_streamPrivate.get(), false);
    }), this);

    g_signal_connect_swapped(decodebin, "pad-added", G_CALLBACK(+[](AudioSourceProviderGStreamer* provider, GstPad* pad) {
        auto padCaps = adoptGRef(gst_pad_query_caps(pad, nullptr));
        bool isAudio = doCapsHaveType(padCaps.get(), "audio"_s);
        RELEASE_ASSERT(isAudio);

        auto sinkPad = adoptGRef(gst_element_get_static_pad(provider->m_audioSinkBin.get(), "sink"));
        gst_pad_link(pad, sinkPad.get());
        gst_element_sync_state_with_parent(provider->m_audioSinkBin.get());
    }), this);

    gst_bin_add_many(GST_BIN_CAST(m_pipeline.get()), decodebin, m_audioSinkBin.get(), nullptr);

    connectSimpleBusMessageCallback(m_pipeline.get(), [weakDecodebin = GThreadSafeWeakPtr(decodebin)](auto message) mutable {
        auto decodebin = weakDecodebin.get();
        if (!decodebin)
            return;

        if (GST_MESSAGE_TYPE(message) != GST_MESSAGE_STREAM_COLLECTION)
            return;

        GRefPtr<GstStreamCollection> collection;
        gst_message_parse_stream_collection(message, &collection.outPtr());
        if (!collection)
            return;

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
        if (!streams)
            return;

        gst_element_send_event(decodebin.get(), gst_event_new_select_streams(streams));
        g_list_free(streams);
    });

    g_object_set(decodebin, "uri", "mediastream://", nullptr);
}
#endif

AudioSourceProviderGStreamer::~AudioSourceProviderGStreamer()
{
    ASP_DEBUG("Disposing");
    m_notifier->invalidate();

    auto deinterleave = adoptGRef(gst_bin_get_by_name(GST_BIN_CAST(m_audioSinkBin.get()), "deinterleave"));
    if (deinterleave && m_client) {
        g_signal_handler_disconnect(deinterleave.get(), m_deinterleavePadAddedHandlerId);
        g_signal_handler_disconnect(deinterleave.get(), m_deinterleavePadRemovedHandlerId);
    }

    setClient(nullptr);
#if ENABLE(MEDIA_STREAM)
    if (m_pipeline) {
        disconnectSimpleBusMessageCallback(m_pipeline.get());
        unregisterPipeline(m_pipeline);
        gst_element_set_state(m_pipeline.get(), GST_STATE_NULL);
    }
#endif
    ASP_DEBUG("Disposing DONE");
}

void AudioSourceProviderGStreamer::configureAudioBin(GstElement* audioBin, GstElement* audioSink)
{
    m_audioSinkBin = audioBin;

    GstElement* audioTee = gst_element_factory_make("tee", "audioTee");
    GstElement* audioQueue = gst_element_factory_make("queue", nullptr);
    GstElement* audioConvert = makeGStreamerElement("audioconvert"_s);
    GstElement* audioConvert2 = makeGStreamerElement("audioconvert"_s);
    GstElement* audioResample = makeGStreamerElement("audioresample"_s);
    GstElement* audioResample2 = makeGStreamerElement("audioresample"_s);
    GstElement* volumeElement = makeGStreamerElement("volume"_s, "volume"_s);

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

void AudioSourceProviderGStreamer::provideInput(AudioBus& bus, size_t framesToProcess)
{
    ASP_TRACE("Fetching buffers from adapters");
    if (!m_adapterLock.tryLock())
        return;

    Locker locker { AdoptLock, m_adapterLock };
    for (const auto& [channelId, adapter] : m_adapters)
        copyGStreamerBuffersToAudioChannel(adapter.get(), bus, channelId - 1, framesToProcess);
}

GstFlowReturn AudioSourceProviderGStreamer::handleSample(GstAppSink* sink, bool isPreroll)
{
    ASP_TRACE("Pulling audio sample from the sink");
    auto sample = adoptGRef(isPreroll ? gst_app_sink_try_pull_preroll(sink, 0) : gst_app_sink_try_pull_sample(sink, 0));
    if (!sample)
        return gst_app_sink_is_eos(sink) ? GST_FLOW_EOS : GST_FLOW_ERROR;

    if (!m_client)
        return GST_FLOW_OK;

    GstBuffer* buffer = gst_sample_get_buffer(sample.get());
    if (!buffer)
        return GST_FLOW_ERROR;

    ASP_TRACE("Storing audio sample %" GST_PTR_FORMAT, buffer);
    {
        Locker locker { m_adapterLock };
        GQuark quark = g_quark_from_static_string("channel-id");
        int channelId = GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(sink), quark));
        ASP_DEBUG("Channel ID: %d", channelId);

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

    ASP_DEBUG("Setting up client %p (previous: %p)", newClient.get(), client());

    bool previousClientWasValid = !!m_client;
    m_client = WTF::move(newClient);

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

        ASP_DEBUG("Cleaning up audio deinterleave chain");
        gstElementLockAndSetState(audioQueue.get(), GST_STATE_NULL);
        gstElementLockAndSetState(audioConvert.get(), GST_STATE_NULL);
        gstElementLockAndSetState(audioResample.get(), GST_STATE_NULL);
        gstElementLockAndSetState(capsFilter.get(), GST_STATE_NULL);
        gstElementLockAndSetState(deInterleave.get(), GST_STATE_NULL);
        gst_element_unlink_many(audioTee.get(), audioQueue.get(), audioConvert.get(), audioResample.get(), capsFilter.get(), deInterleave.get(), nullptr);
        gst_bin_remove_many(GST_BIN_CAST(m_audioSinkBin.get()), audioQueue.get(), audioConvert.get(), audioResample.get(), capsFilter.get(), deInterleave.get(), nullptr);
        gst_element_release_request_pad(audioTee.get(), teeSrcPad.get());
    }

    if (m_client) {
        // The audioconvert and audioresample elements are needed to
        // ensure deinterleave and the sinks downstream receive buffers in
        // the format specified by the capsfilter.
        auto* audioQueue = gst_element_factory_make("queue", "queue");
        auto* audioConvert = makeGStreamerElement("audioconvert"_s, "audioconvert"_s);
        auto* audioResample = makeGStreamerElement("audioresample"_s, "audioresample"_s);
        auto* capsFilter = gst_element_factory_make("capsfilter", "capsfilter");
        auto* deInterleave = makeGStreamerElement("deinterleave"_s, "deinterleave"_s);

        ASP_DEBUG("Setting up audio deinterleave chain");
        g_object_set(deInterleave, "keep-positions", TRUE, nullptr);
        m_deinterleavePadAddedHandlerId = g_signal_connect(deInterleave, "pad-added", G_CALLBACK(onGStreamerDeinterleavePadAddedCallback), this);
        m_deinterleavePadRemovedHandlerId = g_signal_connect(deInterleave, "pad-removed", G_CALLBACK(onGStreamerDeinterleavePadRemovedCallback), this);

        auto caps = adoptGRef(gst_caps_new_simple("audio/x-raw",
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
    ASP_DEBUG("New pad %" GST_PTR_FORMAT, pad);

    // A new pad for a planar channel was added in deinterleave. Plug
    // in an appsink so we can pull the data from each
    // channel. Pipeline looks like:
    // ... deinterleave ! queue ! appsink.
    auto* queue = gst_element_factory_make("queue", nullptr);
    auto* sink = makeGStreamerElement("appsink"_s);

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
#if GST_CHECK_VERSION(1, 24, 0)
        // propose_allocation
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

    auto caps = adoptGRef(gst_caps_new_simple("audio/x-raw",
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
    gst_pad_add_probe(sinkPad.get(), static_cast<GstPadProbeType>(GST_PAD_PROBE_TYPE_EVENT_FLUSH | GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM), [](GstPad*, GstPadProbeInfo* info, gpointer userData) {
        auto event = GST_PAD_PROBE_INFO_EVENT(info);
        auto provider = reinterpret_cast<AudioSourceProviderGStreamer*>(userData);

        if (GST_EVENT_TYPE(event) == GST_EVENT_FLUSH_STOP) {
            provider->clearAdapters();
            return GST_PAD_PROBE_OK;
        }
        if (GST_EVENT_TYPE(event) == GST_EVENT_CAPS) {
            GstCaps* caps;
            gst_event_parse_caps(event, &caps);
            provider->determineSampleRate(caps);
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

    ASP_DEBUG("Pad %" GST_PTR_FORMAT " gone", pad);
    m_deinterleaveSourcePads--;
    if (m_deinterleaveConfiguredSourcePads)
        m_deinterleaveConfiguredSourcePads--;

    GQuark quark = g_quark_from_static_string("peer");
    GstPad* sinkPad = GST_PAD_CAST(g_object_get_qdata(G_OBJECT(pad), quark));
    if (!sinkPad)
        return;

    auto queue = adoptGRef(gst_pad_get_parent_element(sinkPad));
    auto srcPad = adoptGRef(gst_element_get_static_pad(queue.get(), "src"));
    auto sinkSinkPad = adoptGRef(gst_pad_get_peer(srcPad.get()));
    auto sink = adoptGRef(gst_pad_get_parent_element(sinkSinkPad.get()));

    g_signal_handlers_disconnect_by_data(sink.get(), sink.get());

    gst_element_set_state(sink.get(), GST_STATE_NULL);
    gst_element_set_state(queue.get(), GST_STATE_NULL);
    gst_pad_unlink(srcPad.get(), sinkSinkPad.get());
    gst_pad_unlink(pad, sinkPad);
    gst_bin_remove_many(GST_BIN_CAST(m_audioSinkBin.get()), queue.get(), sink.get(), nullptr);
}

void AudioSourceProviderGStreamer::determineSampleRate(const GstCaps* caps)
{
    ASP_DEBUG("Determining sample rate from %" GST_PTR_FORMAT, caps);
    if (gst_caps_is_empty(caps) || gst_caps_is_any(caps)) [[unlikely]]
        return;

    m_deinterleaveConfiguredSourcePads++;
    if (m_deinterleaveConfiguredSourcePads != m_deinterleaveSourcePads)
        return;

    const auto structure = gst_caps_get_structure(caps, 0);
    auto sampleRate = gstStructureGet<int>(structure, "rate"_s);
    if (!sampleRate) [[unlikely]]
        return;

    ASP_DEBUG("Deinterleave configured with %d channels, notifying client", m_deinterleaveSourcePads);
    m_notifier->notify(MainThreadNotification::DeinterleavePadsConfigured, [numberOfChannels = m_deinterleaveSourcePads, client = m_client, sampleRate = *sampleRate] {
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

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO) && ENABLE(VIDEO) && USE(GSTREAMER)
