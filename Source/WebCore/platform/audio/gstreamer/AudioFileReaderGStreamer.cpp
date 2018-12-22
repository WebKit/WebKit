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

#if ENABLE(WEB_AUDIO)

#include "AudioFileReader.h"
#include "AudioBus.h"
#include "GRefPtrGStreamer.h"
#include <gio/gio.h>
#include <gst/app/gstappsink.h>
#include <gst/audio/audio-info.h>
#include <gst/gst.h>
#include <wtf/MainThread.h>
#include <wtf/Noncopyable.h>
#include <wtf/RunLoop.h>
#include <wtf/Threading.h>
#include <wtf/WeakPtr.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebCore {

class AudioFileReader : public CanMakeWeakPtr<AudioFileReader> {
    WTF_MAKE_NONCOPYABLE(AudioFileReader);
public:
    AudioFileReader(const char* filePath);
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
    const char* m_filePath { nullptr };

    float m_sampleRate { 0 };
    int m_channels { 0 };
    GRefPtr<GstBufferList> m_frontLeftBuffers;
    GRefPtr<GstBufferList> m_frontRightBuffers;

    GRefPtr<GstElement> m_pipeline;
    unsigned m_channelSize { 0 };
    GRefPtr<GstElement> m_decodebin;
    GRefPtr<GstElement> m_deInterleave;
    bool m_errorOccurred { false };
};

static void copyGstreamerBuffersToAudioChannel(GstBufferList* buffers, AudioChannel* audioChannel)
{
    float* destination = audioChannel->mutableData();
    unsigned bufferCount = gst_buffer_list_length(buffers);
    for (unsigned i = 0; i < bufferCount; ++i) {
        GstBuffer* buffer = gst_buffer_list_get(buffers, i);
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

AudioFileReader::AudioFileReader(const char* filePath)
    : m_runLoop(RunLoop::current())
    , m_filePath(filePath)
{
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

GstFlowReturn AudioFileReader::handleSample(GstAppSink* sink)
{
    GRefPtr<GstSample> sample = adoptGRef(gst_app_sink_pull_sample(sink));
    if (!sample)
        return GST_FLOW_ERROR;

    GstBuffer* buffer = gst_sample_get_buffer(sample.get());
    if (!buffer)
        return GST_FLOW_ERROR;

    GstCaps* caps = gst_sample_get_caps(sample.get());
    if (!caps)
        return GST_FLOW_ERROR;

    GstAudioInfo info;
    gst_audio_info_from_caps(&info, caps);
    int frames = gst_buffer_get_size(buffer) / info.bpf;

    // Check the first audio channel. The buffer is supposed to store
    // data of a single channel anyway.
    switch (GST_AUDIO_INFO_POSITION(&info, 0)) {
    case GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT:
    case GST_AUDIO_CHANNEL_POSITION_MONO:
        gst_buffer_list_add(m_frontLeftBuffers.get(), gst_buffer_ref(buffer));
        m_channelSize += frames;
        break;
    case GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT:
        gst_buffer_list_add(m_frontRightBuffers.get(), gst_buffer_ref(buffer));
        break;
    default:
        break;
    }

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
    default:
        break;
    }
}

void AudioFileReader::handleNewDeinterleavePad(GstPad* pad)
{
    // A new pad for a planar channel was added in deinterleave. Plug
    // in an appsink so we can pull the data from each
    // channel. Pipeline looks like:
    // ... deinterleave ! queue ! appsink.
    GstElement* queue = gst_element_factory_make("queue", nullptr);
    GstElement* sink = gst_element_factory_make("appsink", nullptr);

    static GstAppSinkCallbacks callbacks = {
        nullptr, // eos
        nullptr, // new_preroll
        // new_sample
        [](GstAppSink* sink, gpointer userData) -> GstFlowReturn {
            return static_cast<AudioFileReader*>(userData)->handleSample(sink);
        },
        { nullptr }
    };
    gst_app_sink_set_callbacks(GST_APP_SINK(sink), &callbacks, this, nullptr);

    g_object_set(sink, "sync", FALSE, nullptr);

    gst_bin_add_many(GST_BIN(m_pipeline.get()), queue, sink, nullptr);

    GRefPtr<GstPad> sinkPad = adoptGRef(gst_element_get_static_pad(queue, "sink"));
    gst_pad_link_full(pad, sinkPad.get(), GST_PAD_LINK_CHECK_NOTHING);

    gst_element_link_pads_full(queue, "src", sink, "sink", GST_PAD_LINK_CHECK_NOTHING);

    gst_element_sync_state_with_parent(queue);
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

    // A decodebin pad was added, plug in a deinterleave element to
    // separate each planar channel. Sub pipeline looks like
    // ... decodebin2 ! audioconvert ! audioresample ! capsfilter ! deinterleave.
    GstElement* audioConvert  = gst_element_factory_make("audioconvert", nullptr);
    GstElement* audioResample = gst_element_factory_make("audioresample", nullptr);
    GstElement* capsFilter = gst_element_factory_make("capsfilter", nullptr);
    m_deInterleave = gst_element_factory_make("deinterleave", "deinterleave");

    g_object_set(m_deInterleave.get(), "keep-positions", TRUE, nullptr);
    g_signal_connect_swapped(m_deInterleave.get(), "pad-added", G_CALLBACK(deinterleavePadAddedCallback), this);
    g_signal_connect_swapped(m_deInterleave.get(), "no-more-pads", G_CALLBACK(deinterleaveReadyCallback), this);

    GRefPtr<GstCaps> caps = adoptGRef(gst_caps_new_simple("audio/x-raw",
        "rate", G_TYPE_INT, static_cast<int>(m_sampleRate),
        "channels", G_TYPE_INT, m_channels,
        "format", G_TYPE_STRING, GST_AUDIO_NE(F32),
        "layout", G_TYPE_STRING, "interleaved", nullptr));
    g_object_set(capsFilter, "caps", caps.get(), nullptr);

    gst_bin_add_many(GST_BIN(m_pipeline.get()), audioConvert, audioResample, capsFilter, m_deInterleave.get(), nullptr);

    GRefPtr<GstPad> sinkPad = adoptGRef(gst_element_get_static_pad(audioConvert, "sink"));
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

    // Build the pipeline (giostreamsrc | filesrc) ! decodebin2
    // A deinterleave element is added once a src pad becomes available in decodebin.
    m_pipeline = gst_pipeline_new(nullptr);

    GRefPtr<GstBus> bus = adoptGRef(gst_pipeline_get_bus(GST_PIPELINE(m_pipeline.get())));
    ASSERT(bus);
    gst_bus_set_sync_handler(bus.get(), [](GstBus*, GstMessage* message, gpointer userData) {
        auto& reader = *static_cast<AudioFileReader*>(userData);
        if (&reader.m_runLoop == &RunLoop::current())
            reader.handleMessage(message);
        else {
            GRefPtr<GstMessage> protectMessage(message);
            auto weakThis = makeWeakPtr(reader);
            reader.m_runLoop.dispatch([weakThis, protectMessage] {
                if (weakThis)
                    weakThis->handleMessage(protectMessage.get());
            });
        }
        gst_message_unref(message);
        return GST_BUS_DROP;
    }, this, nullptr);

    GstElement* source;
    if (m_data) {
        ASSERT(m_dataSize);
        source = gst_element_factory_make("giostreamsrc", nullptr);
        GRefPtr<GInputStream> memoryStream = adoptGRef(g_memory_input_stream_new_from_data(m_data, m_dataSize, nullptr));
        g_object_set(source, "stream", memoryStream.get(), nullptr);
    } else {
        source = gst_element_factory_make("filesrc", nullptr);
        g_object_set(source, "location", m_filePath, nullptr);
    }

    m_decodebin = gst_element_factory_make("decodebin", "decodebin");
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
    m_sampleRate = sampleRate;
    m_channels = mixToMono ? 1 : 2;

    m_frontLeftBuffers = adoptGRef(gst_buffer_list_new());
    m_frontRightBuffers = adoptGRef(gst_buffer_list_new());

    // Start the pipeline processing just after the loop is started.
    m_runLoop.dispatch([this] { decodeAudioForBusCreation(); });
    m_runLoop.run();

    // Set pipeline to GST_STATE_NULL state here already ASAP to
    // release any resources that might still be used.
    gst_element_set_state(m_pipeline.get(), GST_STATE_NULL);

    if (m_errorOccurred)
        return nullptr;

    auto audioBus = AudioBus::create(m_channels, m_channelSize, true);
    audioBus->setSampleRate(m_sampleRate);

    copyGstreamerBuffersToAudioChannel(m_frontLeftBuffers.get(), audioBus->channel(0));
    if (!mixToMono)
        copyGstreamerBuffersToAudioChannel(m_frontRightBuffers.get(), audioBus->channel(1));

    return audioBus;
}

RefPtr<AudioBus> createBusFromAudioFile(const char* filePath, bool mixToMono, float sampleRate)
{
    RefPtr<AudioBus> returnValue;
    auto thread = Thread::create("AudioFileReader", [&returnValue, filePath, mixToMono, sampleRate] {
        returnValue = AudioFileReader(filePath).createBus(sampleRate, mixToMono);
    });
    thread->waitForCompletion();
    return returnValue;
}

RefPtr<AudioBus> createBusFromInMemoryAudioFile(const void* data, size_t dataSize, bool mixToMono, float sampleRate)
{
    RefPtr<AudioBus> returnValue;
    auto thread = Thread::create("AudioFileReader", [&returnValue, data, dataSize, mixToMono, sampleRate] {
        returnValue = AudioFileReader(data, dataSize).createBus(sampleRate, mixToMono);
    });
    thread->waitForCompletion();
    return returnValue;
}

} // WebCore

#endif // ENABLE(WEB_AUDIO)
