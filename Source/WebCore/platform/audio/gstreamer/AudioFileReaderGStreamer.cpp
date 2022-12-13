/*
 *  Copyright (C) 2011, 2012 Igalia S.L
 *  Copyright (C) 2011 Zan Dobersek  <zandobersek@gmail.com>
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
#include "AudioFileReader.h"

#if ENABLE(WEB_AUDIO)

#include "AudioBus.h"
#include "GStreamerCommon.h"
#include <gio/gio.h>
#include <gst/app/gstappsink.h>
#include <gst/audio/audio-info.h>
#include <gst/gst.h>
#include <wtf/MainThread.h>
#include <wtf/Noncopyable.h>
#include <wtf/PrintStream.h>
#include <wtf/RunLoop.h>
#include <wtf/Threading.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

GST_DEBUG_CATEGORY(webkit_audio_file_reader_debug);
#define GST_CAT_DEFAULT webkit_audio_file_reader_debug

static void initializeDebugCategory()
{
    ensureGStreamerInitialized();
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_audio_file_reader_debug, "webkitaudiofilereader", 0, "WebKit WebAudio FileReader");
    });
}

class AudioFileReader : public CanMakeWeakPtr<AudioFileReader> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(AudioFileReader);
public:
    AudioFileReader(const void* data, size_t dataSize);
    ~AudioFileReader();

    RefPtr<AudioBus> createBus(float sampleRate, bool mixToMono);

private:
    static void deinterleavePadAddedCallback(AudioFileReader*, GstPad*);
    static void deinterleaveReadyCallback(AudioFileReader*);
    static void decodebinPadAddedCallback(AudioFileReader*, GstPad*);

    void handleMessage(GstMessage*);
    void handleNewDeinterleavePad(GstPad*);
    void deinterleavePadsConfigured();
    void plugDeinterleave(GstPad*);
    void decodeAudioForBusCreation();
    GstFlowReturn handleSample(GstAppSink*);

    RunLoop& m_runLoop;
    const void* m_data { nullptr };
    size_t m_dataSize { 0 };
    float m_sampleRate { 0 };
    int m_channels { 0 };
    HashMap<int, GRefPtr<GstBufferList>> m_buffers;
    GRefPtr<GstElement> m_pipeline;
    std::optional<int> m_firstChannelType;
    unsigned m_channelSize { 0 };
    GRefPtr<GstElement> m_decodebin;
    GRefPtr<GstElement> m_deInterleave;
    bool m_errorOccurred { false };
};

static void copyGstreamerBuffersToAudioChannel(const GRefPtr<GstBufferList>& buffers, AudioChannel* audioChannel)
{
    float* destination = audioChannel->mutableData();
    unsigned bufferCount = gst_buffer_list_length(buffers.get());
    for (unsigned i = 0; i < bufferCount; ++i) {
        GstBuffer* buffer = gst_buffer_list_get(buffers.get(), i);
        ASSERT(buffer);
        gsize bufferSize = gst_buffer_get_size(buffer);
        gst_buffer_extract(buffer, 0, destination, bufferSize);
        destination += bufferSize / sizeof(float);
    }
}

void AudioFileReader::deinterleavePadAddedCallback(AudioFileReader* reader, GstPad* pad)
{
    reader->handleNewDeinterleavePad(pad);
}

void AudioFileReader::deinterleaveReadyCallback(AudioFileReader* reader)
{
    reader->deinterleavePadsConfigured();
}

void AudioFileReader::decodebinPadAddedCallback(AudioFileReader* reader, GstPad* pad)
{
    reader->plugDeinterleave(pad);
}

AudioFileReader::AudioFileReader(const void* data, size_t dataSize)
    : m_runLoop(RunLoop::current())
    , m_data(data)
    , m_dataSize(dataSize)
{
}

AudioFileReader::~AudioFileReader()
{
    if (m_pipeline) {
        GRefPtr<GstBus> bus = adoptGRef(gst_pipeline_get_bus(GST_PIPELINE(m_pipeline.get())));
        ASSERT(bus);
        gst_bus_set_sync_handler(bus.get(), nullptr, nullptr, nullptr);

        gst_element_set_state(m_pipeline.get(), GST_STATE_NULL);
        m_pipeline = nullptr;
    }

    if (m_decodebin) {
        g_signal_handlers_disconnect_matched(m_decodebin.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
        m_decodebin = nullptr;
    }

    if (m_deInterleave) {
        g_signal_handlers_disconnect_matched(m_deInterleave.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
        m_deInterleave = nullptr;
    }
}

static inline std::optional<int> channelTypeFromCaps(GstCaps* caps)
{
    int channelId = 0;
    GstAudioInfo info;
    gst_audio_info_from_caps(&info, caps);
    switch (GST_AUDIO_INFO_POSITION(&info, 0)) {
    case GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT:
    case GST_AUDIO_CHANNEL_POSITION_MONO:
        channelId = AudioBus::ChannelLeft;
        break;
    case GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT:
        channelId = AudioBus::ChannelRight;
        break;
    case GST_AUDIO_CHANNEL_POSITION_LFE1:
        channelId = AudioBus::ChannelLFE;
        break;
    case GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER:
        channelId = AudioBus::ChannelCenter;
        break;
    case GST_AUDIO_CHANNEL_POSITION_SURROUND_LEFT:
    case GST_AUDIO_CHANNEL_POSITION_REAR_LEFT:
        channelId = AudioBus::ChannelSurroundLeft;
        break;
    case GST_AUDIO_CHANNEL_POSITION_SURROUND_RIGHT:
    case GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT:
        channelId = AudioBus::ChannelSurroundRight;
        break;
    default:
        GST_WARNING("Unhandled channel: %d", GST_AUDIO_INFO_POSITION(&info, 0));
        return { };
    };
    return channelId;
}

GstFlowReturn AudioFileReader::handleSample(GstAppSink* sink)
{
    auto sample = adoptGRef(gst_app_sink_try_pull_sample(sink, 0));
    if (!sample)
        return gst_app_sink_is_eos(sink) ? GST_FLOW_EOS : GST_FLOW_ERROR;

    GstBuffer* buffer = gst_sample_get_buffer(sample.get());
    if (!buffer)
        return GST_FLOW_ERROR;

    GstCaps* caps = gst_sample_get_caps(sample.get());
    if (!caps)
        return GST_FLOW_ERROR;

    auto channelType = channelTypeFromCaps(caps);
    if (!channelType)
        return GST_FLOW_ERROR;

    if (!m_firstChannelType) {
        ASSERT_NOT_REACHED();
        return GST_FLOW_ERROR;
    }

    if (*channelType == *m_firstChannelType) {
        GstAudioInfo info;
        gst_audio_info_from_caps(&info, caps);
        m_channelSize += gst_buffer_get_size(buffer) / info.bpf;
    }

    // Shift hash table key values by one, otherwise we would hit an ASSERT here when channelType is
    // 0 (Left), which is also KeyTraits::emptyValue() which is not allowed.
    int keyId = *channelType + 1;
    auto result = m_buffers.ensure(keyId, [] {
        return adoptGRef(gst_buffer_list_new());
    });
    auto& bufferList = result.iterator->value;
    ASSERT(gst_buffer_list_is_writable(bufferList.get()));
    gst_buffer_list_add(bufferList.get(), gst_buffer_ref(buffer));
    return GST_FLOW_OK;
}

void AudioFileReader::handleMessage(GstMessage* message)
{
    ASSERT(&m_runLoop == &RunLoop::current());

    GUniqueOutPtr<GError> error;
    GUniqueOutPtr<gchar> debug;

    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_EOS:
        m_runLoop.stop();
        break;
    case GST_MESSAGE_WARNING:
        gst_message_parse_warning(message, &error.outPtr(), &debug.outPtr());
        g_warning("Warning: %d, %s. Debug output: %s", error->code,  error->message, debug.get());
        break;
    case GST_MESSAGE_ERROR:
        gst_message_parse_error(message, &error.outPtr(), &debug.outPtr());
        g_warning("Error: %d, %s. Debug output: %s", error->code,  error->message, debug.get());
        m_errorOccurred = true;
        gst_element_set_state(m_pipeline.get(), GST_STATE_NULL);
        m_runLoop.stop();
        break;
    case GST_MESSAGE_STATE_CHANGED: {
        if (GST_MESSAGE_SRC(message) != GST_OBJECT(m_pipeline.get()))
            break;

        GstState oldState, newState, pending;
        gst_message_parse_state_changed(message, &oldState, &newState, &pending);
        GST_INFO_OBJECT(m_pipeline.get(), "State changed (old: %s, new: %s, pending: %s)",
            gst_element_state_get_name(oldState), gst_element_state_get_name(newState), gst_element_state_get_name(pending));

        auto dotFileName = makeString(GST_OBJECT_NAME(m_pipeline.get()), '_', gst_element_state_get_name(oldState), '_', gst_element_state_get_name(newState));
        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN_CAST(m_pipeline.get()), GST_DEBUG_GRAPH_SHOW_ALL, dotFileName.utf8().data());
        break;
    }
    case GST_MESSAGE_LATENCY:
        gst_bin_recalculate_latency(GST_BIN_CAST(m_pipeline.get()));
        break;
    default:
        break;
    }
}

void AudioFileReader::handleNewDeinterleavePad(GstPad* pad)
{
    // A new pad for a planar channel was added in deinterleave. Plug
    // in an appsink so we can pull the data from each
    // channel. Pipeline looks like:
    // ... deinterleave ! appsink.
    GstElement* sink = makeGStreamerElement("appsink", nullptr);

    if (!m_firstChannelType) {
        auto caps = adoptGRef(gst_pad_query_caps(pad, nullptr));
        auto channelType = channelTypeFromCaps(caps.get());
        if (channelType)
            m_firstChannelType = WTFMove(channelType);
    }
    m_channels++;

    static GstAppSinkCallbacks callbacks = {
        nullptr, // eos
        nullptr, // new_preroll
        // new_sample
        [](GstAppSink* sink, gpointer userData) -> GstFlowReturn {
            return static_cast<AudioFileReader*>(userData)->handleSample(sink);
        },
#if GST_CHECK_VERSION(1, 20, 0)
        // new_event
        nullptr,
#endif
        { nullptr }
    };
    gst_app_sink_set_callbacks(GST_APP_SINK(sink), &callbacks, this, nullptr);

    g_object_set(sink, "sync", FALSE, "async", FALSE, "enable-last-sample", FALSE, nullptr);

    gst_bin_add(GST_BIN_CAST(m_pipeline.get()), sink);

    auto sinkPad = adoptGRef(gst_element_get_static_pad(sink, "sink"));
    gst_pad_link_full(pad, sinkPad.get(), GST_PAD_LINK_CHECK_NOTHING);

    gst_element_sync_state_with_parent(sink);
}

void AudioFileReader::deinterleavePadsConfigured()
{
    // All deinterleave src pads are now available, let's roll to
    // PLAYING so data flows towards the sinks and it can be retrieved.
    gst_element_set_state(m_pipeline.get(), GST_STATE_PLAYING);
}

void AudioFileReader::plugDeinterleave(GstPad* pad)
{
    // Ignore any additional source pads just in case.
    if (m_deInterleave)
        return;

    auto padCaps = adoptGRef(gst_pad_query_caps(pad, nullptr));
    if (!doCapsHaveType(padCaps.get(), "audio/x-raw"))
        return;

    // A decodebin pad was added, plug in a deinterleave element to
    // separate each planar channel. Sub pipeline looks like
    // ... decodebin2 ! audioconvert ! audioresample ! capsfilter ! deinterleave.
    GstElement* audioConvert  = makeGStreamerElement("audioconvert", nullptr);
    GstElement* audioResample = makeGStreamerElement("audioresample", nullptr);
    GstElement* capsFilter = gst_element_factory_make("capsfilter", nullptr);
    m_deInterleave = makeGStreamerElement("deinterleave", "deinterleave");

    g_object_set(m_deInterleave.get(), "keep-positions", TRUE, nullptr);
    g_signal_connect_swapped(m_deInterleave.get(), "pad-added", G_CALLBACK(deinterleavePadAddedCallback), this);
    g_signal_connect_swapped(m_deInterleave.get(), "no-more-pads", G_CALLBACK(deinterleaveReadyCallback), this);

    auto caps = adoptGRef(gst_caps_new_simple("audio/x-raw", "rate", G_TYPE_INT, static_cast<int>(m_sampleRate),
        "format", G_TYPE_STRING, GST_AUDIO_NE(F32), "layout", G_TYPE_STRING, "interleaved", nullptr));
    g_object_set(capsFilter, "caps", caps.get(), nullptr);

    gst_bin_add_many(GST_BIN(m_pipeline.get()), audioConvert, audioResample, capsFilter, m_deInterleave.get(), nullptr);

    auto sinkPad = adoptGRef(gst_element_get_static_pad(audioConvert, "sink"));
    gst_pad_link_full(pad, sinkPad.get(), GST_PAD_LINK_CHECK_NOTHING);

    gst_element_link_pads_full(audioConvert, "src", audioResample, "sink", GST_PAD_LINK_CHECK_NOTHING);
    gst_element_link_pads_full(audioResample, "src", capsFilter, "sink", GST_PAD_LINK_CHECK_NOTHING);
    gst_element_link_pads_full(capsFilter, "src", m_deInterleave.get(), "sink", GST_PAD_LINK_CHECK_NOTHING);

    gst_element_sync_state_with_parent(audioConvert);
    gst_element_sync_state_with_parent(audioResample);
    gst_element_sync_state_with_parent(capsFilter);
    gst_element_sync_state_with_parent(m_deInterleave.get());
}

void AudioFileReader::decodeAudioForBusCreation()
{
    ASSERT(&m_runLoop == &RunLoop::current());

    // Build the pipeline giostreamsrc ! decodebin
    // A deinterleave element is added once a src pad becomes available in decodebin.
    static Atomic<uint32_t> pipelineId;
    m_pipeline = gst_pipeline_new(makeString("audio-file-reader-", pipelineId.exchangeAdd(1)).ascii().data());

    GRefPtr<GstBus> bus = adoptGRef(gst_pipeline_get_bus(GST_PIPELINE(m_pipeline.get())));
    ASSERT(bus);
    gst_bus_set_sync_handler(bus.get(), [](GstBus*, GstMessage* message, gpointer userData) {
        auto& reader = *static_cast<AudioFileReader*>(userData);
        if (&reader.m_runLoop == &RunLoop::current())
            reader.handleMessage(message);
        else {
            GRefPtr<GstMessage> protectMessage(message);
            WeakPtr weakThis { reader };
            reader.m_runLoop.dispatch([weakThis, protectMessage] {
                if (weakThis)
                    weakThis->handleMessage(protectMessage.get());
            });
        }
        gst_message_unref(message);
        return GST_BUS_DROP;
    }, this, nullptr);

    ASSERT(m_data);
    ASSERT(m_dataSize);
    auto* source = makeGStreamerElement("giostreamsrc", nullptr);
    auto memoryStream = adoptGRef(g_memory_input_stream_new_from_data(m_data, m_dataSize, nullptr));
    g_object_set(source, "stream", memoryStream.get(), nullptr);

    m_decodebin = makeGStreamerElement("decodebin", "decodebin");
    g_signal_connect_swapped(m_decodebin.get(), "pad-added", G_CALLBACK(decodebinPadAddedCallback), this);

    gst_bin_add_many(GST_BIN(m_pipeline.get()), source, m_decodebin.get(), nullptr);
    gst_element_link_pads_full(source, "src", m_decodebin.get(), "sink", GST_PAD_LINK_CHECK_NOTHING);

    // Catch errors here immediately, there might not be an error message if we're unlucky.
    if (gst_element_set_state(m_pipeline.get(), GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE) {
        g_warning("Error: Failed to set pipeline to PAUSED");
        m_errorOccurred = true;
        m_runLoop.stop();
    }
}

RefPtr<AudioBus> AudioFileReader::createBus(float sampleRate, bool mixToMono)
{
    GST_DEBUG("Scheduling audio decoding task, sampleRate: %f, mixToMono: %s", sampleRate, boolForPrinting(mixToMono));
    m_sampleRate = sampleRate;

    // Start the pipeline processing just after the loop is started.
    m_runLoop.dispatch([this] {
        decodeAudioForBusCreation();
    });
    m_runLoop.run();

    // Set pipeline to GST_STATE_NULL state here already ASAP to
    // release any resources that might still be used.
    gst_element_set_state(m_pipeline.get(), GST_STATE_NULL);

    if (m_errorOccurred) {
        m_buffers.clear();
        return nullptr;
    }

    GST_DEBUG("Decoding done, transfering data to audio bus containing %d channels, each with %u frames", m_channels, m_channelSize);
    auto audioBus = AudioBus::create(m_channels, m_channelSize, true);
    audioBus->setSampleRate(m_sampleRate);

    for (auto& [key, buffer] : m_buffers)
        copyGstreamerBuffersToAudioChannel(buffer, audioBus->channelByType(key - 1));

    m_buffers.clear();

    if (mixToMono)
        return AudioBus::createByMixingToMono(audioBus.get());
    return audioBus;
}

RefPtr<AudioBus> createBusFromInMemoryAudioFile(const void* data, size_t dataSize, bool mixToMono, float sampleRate)
{
    initializeDebugCategory();
    GST_DEBUG("Creating bus from in-memory audio data (%zu bytes)", dataSize);
    RefPtr<AudioBus> bus;
    auto thread = Thread::create("AudioFileReader", [&bus, data, dataSize, mixToMono, sampleRate] {
        bus = AudioFileReader(data, dataSize).createBus(sampleRate, mixToMono);
    });
    thread->waitForCompletion();
    return bus;
}

} // WebCore

#endif // ENABLE(WEB_AUDIO)
